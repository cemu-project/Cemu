#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
image_name="${CEMU_DOCKER_IMAGE:-cemu-extend:build}"
artifact_dir="${project_dir}/result/bin"
temporary_name=".Cemu_release.$$.tmp"
git_hash="$(git -C "${project_dir}" log --format=%h -1 2>/dev/null || printf unknown)"
commit_hash="$(git -C "${project_dir}" rev-parse HEAD 2>/dev/null || printf unknown)"

docker build --progress=plain --target build \
	--build-arg "GIT_HASH=${git_hash}" \
	--build-arg "CEMU_EXTEND_COMMIT_HASH=${commit_hash}" \
	-t "${image_name}" "${project_dir}"

mkdir -p "${artifact_dir}"
docker run --rm --user "$(id -u):$(id -g)" \
	-v "${artifact_dir}:/artifacts" \
	"${image_name}" \
	cp /Cemu_release "/artifacts/${temporary_name}"
mv -f "${artifact_dir}/${temporary_name}" "${artifact_dir}/Cemu_release"

printf 'Docker release build: %s\n' "${artifact_dir}/Cemu_release"
sha256sum "${artifact_dir}/Cemu_release"
