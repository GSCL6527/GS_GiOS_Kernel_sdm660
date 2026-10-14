#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Usage: run_third_update_vm.sh KERNEL_IMAGE EXPORTED_UAPI OUTPUT_DIRECTORY
# CC must be Clang; QEMU defaults to qemu-system-aarch64.
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
status=0
timeout "${VM_TIMEOUT:-120}s" "${QEMU:-qemu-system-aarch64}" \
	-M virt,gic-version=3 -cpu cortex-a53 -smp 2 -m 512M \
	-nographic -no-reboot -kernel "$image" \
	-initrd "$output/initramfs.cpio.gz" \
	-append 'console=ttyAMA0 earlycon=pl011,0x9000000 rdinit=/init loglevel=6 nokaslr' \
	> "$output/runtime.log" 2>&1 || status=$?
cat "$output/runtime.log"
if (( status != 0 )) || ! rg -q '^THIRD_UPDATE_RUNTIME_PASS' "$output/runtime.log"; then
	exit 1
fi
