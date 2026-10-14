#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Usage: run_third_update_vm.sh KERNEL_IMAGE EXPORTED_UAPI OUTPUT_DIRECTORY
# CC must be Clang; QEMU defaults to qemu-system-aarch64; DTC defaults to dtc.
set -euo pipefail
image=$(realpath "${1:?kernel Image required}")
headers=$(realpath "${2:?headers_install include directory required}")
mkdir -p "${3:?output directory required}"
output=$(realpath "$3")
source_dir=$(cd "$(dirname "$0")" && pwd)
"${CC:-clang}" --target=aarch64-linux-gnu -O2 -Wall -Wextra \
	-ffreestanding -fno-builtin -fno-stack-protector -nostdlib -static \
	-fuse-ld=lld -Wl,-e,_start -I"$headers" \
	"$source_dir/third_update_vm_init.c" -o "$output/init"
python3 - "$output" <<'PY'
import gzip
import pathlib
import sys

output = pathlib.Path(sys.argv[1])
archive = bytearray()
for name, mode, data in [('init', 0o100755, (output / 'init').read_bytes()),
                         ('TRAILER!!!', 0, b'')]:
    fields = [1, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(name) + 1, 0]
    archive += b'070701' + ''.join(f'{n:08x}' for n in fields).encode()
    archive += name.encode() + b'\0'
    archive += b'\0' * (-len(archive) % 4)
    archive += data
    archive += b'\0' * (-len(archive) % 4)
(output / 'initramfs.cpio.gz').write_bytes(gzip.compress(archive, mtime=0))
PY
# Recent QEMU emits a socket node above clusters, which this 4.19 topology
# parser predates. Flatten that VM-only wrapper, retaining CPU phandles.
"${QEMU:-qemu-system-aarch64}" \
	-M "virt,gic-version=3,its=off,dumpdtb=$output/virt.dtb" \
	-cpu cortex-a53 -smp 2 -m 512M -nographic > "$output/dtb.log" 2>&1
"${DTC:-dtc}" -I dtb -O dts "$output/virt.dtb" -o "$output/virt.dts"
python3 - "$output/virt.dts" <<'PY'
import pathlib
import re
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text()
match = re.search(r'\bsocket0\s*\{', text)
if match:
    begin = match.end()
    depth = 1
    end = begin
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    assert text[end] == ';'
    text = text[:match.start()] + text[begin:end - 1] + text[end + 1:]
    path.write_text(text)
PY
"${DTC:-dtc}" -I dts -O dtb "$output/virt.dts" -o "$output/virt.dtb"
status=0
timeout "${VM_TIMEOUT:-120}s" "${QEMU:-qemu-system-aarch64}" \
	-M virt,gic-version=3,its=off -cpu cortex-a53 -smp 2 -m 512M \
	-nographic -no-reboot -kernel "$image" \
	-dtb "$output/virt.dtb" \
	-initrd "$output/initramfs.cpio.gz" \
	-append "console=ttyAMA0 earlycon=pl011,0x9000000 rdinit=/init loglevel=6 nokaslr ${VM_APPEND:-initcall_blacklist=qtee_shmbridge_init}" \
	> "$output/runtime.log" 2>&1 || status=$?
cat "$output/runtime.log"
if (( status != 0 )) || ! rg -q '^THIRD_UPDATE_RUNTIME_PASS' "$output/runtime.log"; then
	exit 1
fi
