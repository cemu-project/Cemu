#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
image_name="${CEMU_DOCKER_IMAGE:-cemu-extend:build}"
artifact_dir="${project_dir}/result/bin"
temporary_name=".Cemu_release.$$.tmp"

docker build --progress=plain --target build -t "${image_name}" "${project_dir}"
docker run --rm "${image_name}" \
	ctest --test-dir /workspace/CemuExtend/build/docker --output-on-failure

mkdir -p "${artifact_dir}"
docker run --rm --user "$(id -u):$(id -g)" \
	-v "${artifact_dir}:/artifacts" \
	"${image_name}" \
	cp /workspace/CemuExtend/bin/Cemu_release "/artifacts/${temporary_name}"
mv -f "${artifact_dir}/${temporary_name}" "${artifact_dir}/Cemu_release"

printf 'Docker release build: %s\n' "${artifact_dir}/Cemu_release"
sha256sum "${artifact_dir}/Cemu_release"
