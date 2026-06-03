#!/usr/bin/env bash
# Build a Debian package for termtrans.
# The script does not need root: it installs into a staging directory first,
# then asks dpkg-deb to create the final .deb file.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

package_name="${PACKAGE_NAME:-termtrans}"
build_dir="${BUILD_DIR:-${repo_root}/build/package-deb}"
work_dir="${WORK_DIR:-${repo_root}/dist/deb}"
out_dir="${OUT_DIR:-${repo_root}/dist}"
config="${CONFIG:-Release}"
maintainer="${MAINTAINER:-termtrans maintainers <noreply@example.com>}"
cmake_build_type="${CMAKE_BUILD_TYPE:-Release}"

require_command() {
  local command_name="$1"
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "Missing command: ${command_name}" >&2
    exit 1
  fi
}

read_project_version() {
  if [[ -n "${VERSION:-}" ]]; then
    printf '%s\n' "${VERSION}"
    return
  fi

  local cmake_version
  cmake_version="$(
    sed -nE 's/^[[:space:]]*project\(termtrans VERSION ([0-9][^[:space:]]*) LANGUAGES CXX\).*$/\1/p' \
      "${repo_root}/CMakeLists.txt" | head -n 1
  )"
  if [[ -n "${cmake_version}" ]]; then
    printf '%s\n' "${cmake_version}"
    return
  fi

  printf '0.1.0\n'
}

read_architecture() {
  if [[ -n "${ARCH:-}" ]]; then
    printf '%s\n' "${ARCH}"
    return
  fi

  dpkg --print-architecture
}

read_shared_library_depends() {
  local binary_path="$1"
  local depends
  local temp_dir

  temp_dir="$(mktemp -d)"
  mkdir -p "${temp_dir}/debian"
  cat >"${temp_dir}/debian/control" <<EOF
Source: ${package_name}
Section: utils
Priority: optional
Maintainer: ${maintainer}
Standards-Version: 4.7.0

Package: ${package_name}
Architecture: any
Depends: \${shlibs:Depends}
Description: Unix filter style AI translation command line tool
 termtrans translates terminal input with an OpenAI-compatible provider.
EOF

  depends="$(cd "${temp_dir}" && dpkg-shlibdeps -O "${binary_path}" 2>/dev/null |
    sed -nE 's/^shlibs:Depends=//p')" || true
  rm -rf "${temp_dir}"

  if [[ -n "${depends}" ]]; then
    printf '%s\n' "${depends}"
    return
  fi

  # Fallback dependencies are only used when dpkg-shlibdeps cannot resolve the
  # binary. For release builds, rebuild packages in the target distro image.
  printf 'libc6, libcurl4, libssl3, libsqlite3-0\n'
}

write_control_file() {
  local control_path="$1"
  local version="$2"
  local architecture="$3"
  local depends="$4"

  cat >"${control_path}" <<EOF
Package: ${package_name}
Version: ${version}
Section: utils
Priority: optional
Architecture: ${architecture}
Maintainer: ${maintainer}
Depends: ${depends}
Description: Unix filter style AI translation command line tool
 termtrans translates stdin or one direct text argument with an
 OpenAI-compatible provider and writes only translated text to stdout.
EOF
}

require_command cmake
require_command dpkg
require_command dpkg-deb

version="$(read_project_version)"
architecture="$(read_architecture)"
package_root="${work_dir}/${package_name}_${version}_${architecture}"
output_deb="${out_dir}/${package_name}_${version}_${architecture}.deb"

rm -rf "${package_root}"
mkdir -p "${package_root}/DEBIAN" "${out_dir}"

cmake -S "${repo_root}" -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE="${cmake_build_type}" \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${build_dir}" --config "${config}" --target termtrans
DESTDIR="${package_root}" cmake --install "${build_dir}" --config "${config}"

binary_path="${package_root}/usr/bin/termtrans"
if [[ ! -x "${binary_path}" ]]; then
  echo "Installed termtrans binary was not found: ${binary_path}" >&2
  exit 1
fi

depends="$(read_shared_library_depends "${binary_path}")"
write_control_file \
  "${package_root}/DEBIAN/control" \
  "${version}" \
  "${architecture}" \
  "${depends}"

dpkg-deb --build --root-owner-group "${package_root}" "${output_deb}"

echo "Created Debian package: ${output_deb}"
