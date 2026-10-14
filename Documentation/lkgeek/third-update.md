# third-update integration and validation

## Source and scope

- Original LKGeek head: `1e1c11baf6e4470207eb40f7dde8c449b94b942c` (`Sukisu`).
- Source: <https://github.com/LineageOS/android_kernel_xiaomi_sdm660/tree/lineage-22.2>.
- Pinned Lineage head: `0eaf76b319b586a7974036cbcdd1c1e8f35364bd`.
- Common ancestor: `a8bf86a0e0fa05070897a210d706d5c4d83c26ac`.
- Target branch: `third-update`; target device: Wayne / Xiaomi 6X.

The complete source tree delta since the common ancestor was imported with a
cumulative cherry-pick, then reconciled with LKGeek features. It is a compressed
import, not 7,756 individual commits. The original source commit IDs and subjects
are recorded in `third-update-source-commits.txt`; patch equivalence against the
original target is recorded in `third-update-patch-equivalence.txt` (463 equivalent,
7,293 without an equivalent patch ID). This includes the source's final effects
and reversions, rather than reintroducing every intermediate state.

Conflict resolution keeps coherent newer LKGeek RCU, Binder, ext4/F2FS, scheduler,
ZRAM and KernelSU/SUSFS implementations where the source used older APIs. It uses
coherent Lineage BPF, TCP core, incremental filesystem, PM QoS and libfdt code,
with adapters for retained local consumers. Source device trees were imported.

## Compatibility and diagnostics fixes

- Synchronize BPF verifier, syscall, map/helper implementations, headers and
  networking/trace consumers; avoid mixtures of incompatible backport generations.
- Implement ARM64 JMP32 immediate/register comparisons and JSET with 32-bit
  operands; resolve BPF-to-BPF addresses on the extra JIT pass. Keep the local
  modulo instruction optimization. Enable JIT without requiring loadable modules.
- Reconcile memory reclaim, rmap, compaction, vmalloc, VM fault, RCU task fields,
  file descriptor helpers, Binder logging and networking interfaces. Restore
  SELinux socket security allocation/free to match the imported pointer-based
  network stack; runtime testing caught the previous embedded-storage mismatch.
- Restore KernelSU and Kprofiles build registration and vmlinux object exports;
  correct audio header symlinks and libfdt implementation/header mismatches.
- Force the ARM64 global stack protector guard. Android-target Clang otherwise
  emits a TPIDR_EL0 userspace TLS canary access, causing a fault before the console
  initializes; this was found by the virtual-machine boot test.
- Remove the silent `/dev/kmsg` substring filters for `healthd`, `logd`,
  `DM_DEV_STATUS`, `Untracked pid` and `usb_gadget`.
- Restore SELinux granted/permissive audit behavior while retaining the SUSFS
  SID override; fix console lock handover state. Increase Wayne's log buffer to
  4 MiB and enable printk, symbols, stack traces, dynamic debug and Binder logs.
  Existing ramoops/pstore device-tree allocation is retained. A finite ring buffer
  can still overwrite old records under sustained load; capture logs promptly.

## Reproduction

Toolchain used: AOSP Clang 20 (`r547379`), LLVM integrated assembler and LLD;
compatibility binutils are the existing `gcc-64` / `gcc-32` toolchain directories.
KernelSU submodule is pinned to `9cca1f097ce3b1dacbc7e181e93f7120ea2f1729`.

```sh
export PATH=/home/lkdenchin/toolchains/clang-aosp/bin:/home/lkdenchin/toolchains/gcc-64/bin:/home/lkdenchin/toolchains/gcc-32/bin:$PATH
make O=/tmp/third-update-validation/build ARCH=arm64 LLVM=1 LLVM_IAS=1 \
  CROSS_COMPILE=aarch64-linux-android- CROSS_COMPILE_COMPAT=arm-linux-androideabi- \
  CROSS_COMPILE_ARM32=arm-linux-androideabi- HOSTCC=clang \
  LD='ld.lld --threads=2 --thinlto-jobs=2' vendor/wayne-oss-perf_defconfig
# Use the same options for Image.gz dtbs, with -j2.
```

The final-build scripts and detailed failure/repair logs are retained in
`/tmp/third-update-validation`. Compilation was performed after integration review;
subsequent builds repair discovered compilation/runtime failures.

For the QEMU validation configuration, copy the production output to a separate
output directory and enable `ARCH_VEXPRESS`, `SERIAL_AMBA_PL011`,
`SERIAL_AMBA_PL011_CONSOLE`, `DEVTMPFS`, `DEVTMPFS_MOUNT`, `POWER_RESET`,
`POWER_RESET_SYSCON`, `GPIO_PL061`; disable `QCOM_EARLY_RANDOM` (the virtual
machine has no Qualcomm TrustZone PRNG); run `olddefconfig` and build `Image.gz`
with the same toolchain. BPF/JIT, KernelSU and the production subsystems remain enabled.
Install ARM64 UAPI headers with `headers_install`, then run:

```sh
CC=clang QEMU=qemu-system-aarch64 DTC=/path/to/build/scripts/dtc/dtc \
  tools/testing/selftests/bpf/run_third_update_vm.sh \
  /path/to/vm-build/arch/arm64/boot/Image /path/to/uapi/include /path/to/results
```

The runner flattens recent QEMU's socket wrapper in its generated CPU topology
for the 4.19 parser and skips `qtee_shmbridge_init`, which requires Qualcomm
TrustZone. These are VM-only adjustments; Wayne configuration and device trees
retain their hardware services.

The freestanding init requires no userspace libraries. It checks actual JIT code
presence and all 22 JMP32 comparison forms with packet-driven values, subprogram
calls, the boot-time helper,
array map operations, ring buffer reserve/discard, BTF loading, cgroup attachment,
and round-trip delivery of all five formerly filtered log messages. Additional
JMP32 cases are registered in `lib/test_bpf.c`. The standalone printk selftest can
also run on a device with `/dev/kmsg` access.

## Validation status

Final production `Image.gz` and Wayne/Jasmine device trees built successfully
with exit status 0 after both runtime-discovered kernel fixes. The build source
commit is `bfeda0eaafff34a142dd7e98e93fc7bde29c30e9` (kernel code last changed in
`36fc013e1e1a`); release string: `4.19.325-cip132-st16-LKGeek-perf-gbfeda0eaafff`. ARM64 UAPI `headers_install`,
`gzip -t`, Wayne device-tree decompilation and `git diff --check` passed. The native
printk test compiled and skipped execution because this host has no `/dev/kmsg`.
The corrected VM booted to userspace and all 34 runtime assertions passed
(`THIRD_UPDATE_RUNTIME_PASS`), including all five log messages. The VM emits
Qualcomm socinfo/camera warnings because those devices are absent; it reports no
panic during the successful regression run. Production configuration remains
`vendor/wayne-oss-perf_defconfig`. Existing compiler warnings and Qualcomm
vendor-binding dtc warnings remain; this is not a warning-free certification.

No physical Wayne device is connected. QEMU cannot validate Qualcomm drivers,
Android boot, modem, camera, audio, suspend, charging or device-specific behavior.
A successful build and virtual regression run do not certify those hardware paths.

## Delivered artifacts

Local build artifacts are retained outside Git tracking in
`/home/lkdenchin/LKGeek_sdm660/.third-update-artifacts/`:
`Image.gz`, `sdm660-mtp-wayne.dtb`, `sdm660-mtp-jasmine.dtb`, both configuration
files, `System.map`, validation logs and `SHA256SUMS`. These are kernel/device-tree
outputs, not a flashable Android boot image. Packaging must use the device's
existing boot-image/installer layout.

Final Image.gz SHA-256:
`028c134d4586e557006afdd2a081502c149af176760cc52f9f1adf0050396a71`.
Wayne DTB SHA-256:
`578f2434725c18c1b605c490614d4b644b8c1514f59859d15007b1a3549689d9`.

All source changes and the test report are committed to `third-update`; there
are no unresolved merge entries. The report-only completion commit does not
change the tested kernel code.
