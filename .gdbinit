python

import gdb
import re


class BetypePrinter:
    PATTERN = re.compile("^betype<.*>$")

    def __init__(self, obj):
        self._obj = obj

    def to_string(self):
        underlying = self._obj["m_value"]
        reversed_bytes = bytes(reversed(underlying.bytes))
        return gdb.Value(reversed_bytes, underlying.type)

class MemptrPrinter:
    PATTERN = re.compile("^MEMPTR<.*>$")

    def __init__(self, obj):
        self._ptr_type = obj.type.strip_typedefs().unqualified().template_argument(0).pointer()
        self._guest_address = obj["m_value"].cast(gdb.lookup_type("uint32"))

    def ptr(self):
        if self._guest_address == 0:
            return gdb.Value(0, self._ptr_type)
        base_addr = gdb.parse_and_eval('memory_base', global_context=True)
        return (self._guest_address + base_addr).reinterpret_cast(self._ptr_type)

    def children(self):
        return [("raw", self.ptr())]

    def to_string(self):
        return self._guest_address.format_string(format="x")

def lookup(val):
    tag = val.type.strip_typedefs().unqualified().tag
    if tag is None:
        return None
    printers = [BetypePrinter, MemptrPrinter]
    for printer in printers:
        if printer.PATTERN.match(tag):
            return printer(val)
    return None

gdb.pretty_printers.append(lookup)
