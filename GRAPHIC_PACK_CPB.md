# Graphic pack binary patches (`.cpb`)

CemuExtend can load Cemu patch groups from CPB1 binary files. A CPB file is a
serialized patch description, not an executable format: it stores patch groups,
module matches, labels, data, and relocations that CemuExtend resolves when the
matching RPX or RPL module is loaded.

CPB is useful for generated patches and large codecave payloads because it is
smaller and faster to parse than an equivalent text patch.

## File naming and placement

Place the CPB file beside the graphic pack's `rules.txt`:

```text
graphicPacks/
└── ExamplePack/
    ├── rules.txt
    └── patch_example.cpb
```

The filename must match `patch_*.cpb`. Matching is case-insensitive. The old
`.pbin` extension is not supported.

CemuExtend selects patch formats in this order:

1. `patch_*.cpb`
2. `patch_*.asm`
3. `patches.txt`

If at least one matching CPB file exists, CemuExtend does not load ASM or
`patches.txt`, even when the CPB file is invalid. Do not ship generated ASM in
the installable graphic pack when CPB is intended to be authoritative.

## Building a CPB file

CemuExtend consumes CPB files but does not compile an ASM file into CPB. The
project that owns the payload should serialize CPB1 as part of its build.

A typical build pipeline is:

1. Link the PowerPC payload.
2. Extract the payload section as a flat binary, for example with
   `powerpc-eabi-objcopy`.
3. Pad data as required by the payload ABI, usually to four-byte alignment.
4. Write a CPB1 group containing the target module hashes, hook data, labels,
   payload data, and relocations.
5. Name the result `patch_<name>.cpb` and package it with `rules.txt`.

For example:

```sh
powerpc-eabi-objcopy --only-section=.payload client.elf -O binary payload.bin
python3 generate_patch.py payload.bin patch_client.asm patch_client.cpb
mkdir -p dist/GraphicPack
cp rules.txt patch_client.cpb dist/GraphicPack/
```

`generate_patch.py` is project-specific because hook addresses, module hashes,
entry labels, and payload layout differ between projects. A complete generator
should use big-endian integer writes and follow the format below. The
[mcwiiu-client-template](https://github.com/fooly9858/mcwiiu-client-template)
project provides a working generator and verifier:

- `tools/generate_patch.py`
- `tools/verify_cpb.py`
- `make package` or `./docker-build.sh`

For its reproducible Docker build:

```sh
git clone --recursive https://github.com/fooly9858/mcwiiu-client-template.git
cd mcwiiu-client-template
./docker-build.sh
```

Its packaged result is written to
`out/dist/<project-name>/GraphicPack/patch_<project_name>.cpb`.

## CPB1 binary format

All integer fields are unsigned and big-endian. A `string` consists of a
big-endian `u16` byte length followed by exactly that many bytes. Group names,
label names, and relocation expressions must not be empty. ASCII is recommended
for names and expressions.

### File header

| Field | Type | Description |
| --- | --- | --- |
| magic | 4 bytes | ASCII `CPB1` (`43 50 42 31`) |
| group count | `u32` | Number of groups that follow |
| groups | repeated | `group count` group records |

### Group record

| Field | Type | Description |
| --- | --- | --- |
| name | `string` | Non-empty patch group name |
| module match count | `u32` | Must be at least one |
| module matches | repeated `u32` | RPX/RPL module hashes |
| entry count | `u32` | Number of entries in this group |
| entries | repeated | Label or data entry records |

Unlike text ASM patches, CPB1 does not encode CemuExtend's RPX wildcard or
`.callback` syntax. List every supported module hash explicitly.

### Label entry

| Field | Type | Description |
| --- | --- | --- |
| entry type | `u8` | `1` |
| address | `u32` | Fixed address or codecave-relative address |
| name | `string` | Non-empty symbol name |

Labels can be referenced by relocation expressions. Generated payloads commonly
place a label at address `0`, the beginning of the group's codecave.

### Data entry

| Field | Type | Description |
| --- | --- | --- |
| entry type | `u8` | `2` |
| address | `u32` | Destination address or codecave-relative address |
| data size | `u32` | Number of data bytes |
| relocation count | `u32` | Number of relocation records |
| data | `data size` bytes | Original bytes before relocations |
| relocations | repeated | `relocation count` relocation records |

Small addresses use the existing graphic-pack codecave relocation rules. A
common layout writes the entire payload as one data entry at address `0`, then
uses fixed-address data entries for hooks and pointers.

### Relocation record

| Field | Type | Description |
| --- | --- | --- |
| type | `u8` | One of the values below |
| byte offset | `u32` | Offset into the containing data entry |
| bit offset | `u8` | Used by masked-immediate relocations |
| bit count | `u8` | Used by masked-immediate relocations |
| expression | `string` | Non-empty Cemu patch expression |

Supported relocation types are:

| Value | Name | Patched width |
| ---: | --- | ---: |
| `0` | `U32_MASKED_IMM` | 4 bytes |
| `1` | `BRANCH_S16` | 4 bytes |
| `2` | `BRANCH_S26` | 4 bytes |
| `3` | `FLOAT` | 4 bytes |
| `4` | `DOUBLE` | 8 bytes |
| `5` | `U32` | 4 bytes |
| `6` | `U16` | 2 bytes |
| `7` | `U8` | 1 byte |

The relocation range must fit completely inside its data entry. For
`U32_MASKED_IMM`, `bit count` must be 1 through 32 and `bit offset + bit count`
must not exceed 32. Set both fields to zero for other relocation types.

Expressions use the same resolver as text patches. Labels, preset variables,
imports, and the case-insensitive suffixes `@ha`, `@h`/`@hi`, and `@l`/`@lo`
are available. The function forms such as `ha(symbol)` remain available too.

For branch relocations, store a valid base branch instruction in the data. For
example, a relative unconditional branch normally starts as big-endian
`48 00 00 00`; `BRANCH_S26` fills its target displacement.

## Minimal serialization example

The following Python helpers show the byte layout used by a generator:

```python
import struct


def write_string(out, value: str) -> None:
    data = value.encode("ascii")
    out.write(struct.pack(">H", len(data)))
    out.write(data)


def write_label(out, address: int, name: str) -> None:
    out.write(struct.pack(">BI", 1, address))
    write_string(out, name)


def write_relocation(out, kind: int, offset: int,
                     bit_offset: int, bit_count: int,
                     expression: str) -> None:
    out.write(struct.pack(">BIBB", kind, offset, bit_offset, bit_count))
    write_string(out, expression)


def write_data(out, address: int, data: bytes, relocations=()) -> None:
    out.write(struct.pack(">BIII", 2, address, len(data), len(relocations)))
    out.write(data)
    for relocation in relocations:
        write_relocation(out, *relocation)
```

A complete file starts with `b"CPB1"`, then a big-endian group count, followed
by the group and entry records described above. Keep a verifier beside the
generator and compare the parsed entries against the intended addresses, data,
and relocation expressions rather than checking only the magic bytes.

## Validation and troubleshooting

Check the header quickly with:

```sh
od -An -tx1 -N4 patch_example.cpb | tr -d ' \n'
```

The expected output is `43504231`.

CemuExtend rejects the entire CPB file for an invalid magic, truncated field,
unknown entry or relocation type, empty required string, relocation outside its
data entry, invalid masked-immediate range, or trailing bytes. Because CPB takes
priority over all text patch formats, temporarily remove or rename an invalid
CPB while comparing behavior with an ASM patch.

Enable patch logging in CemuExtend when diagnosing parse or relocation errors.
The log reports whether failure happened while parsing the file or while
resolving and applying a group to a module.
