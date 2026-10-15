# KernelSU upstream update (2026-10-15)

The `third-update` branch now pins `KernelSU` to the latest fetched
ReSukiSU/ReSukiSU `main` commit:
`d7260ca1e5c128061122924d0d48754e361e135c`.
Previous pin: `9cca1f097ce3b1dacbc7e181e93f7120ea2f1729`.
The update includes 397 upstream commits. The upstream project now identifies
itself as BakaSU; the actual queried kernel version is
`v4.2.0-rc3-d7260ca1@BakaSU`, UAPI version 5.

## Legacy compatibility

Upstream currently refuses direct SUSFS builds pending its own SUSFS update.
This integration supplies a local, explicitly reviewed 4.19/SUSFS 2.1 adapter;
it does not claim that upstream has enabled SUSFS support.

The upstream submodule stays pristine and is fetched from its original URL.
`drivers/kernelsu` is now a small tracked overlay: most entries link directly
to upstream; only Kbuild, sucompat.c and sucompat.h are copied and adapted.
See `drivers/kernelsu/README.lkgeek` for the exact maintenance contract.

- Keep the existing user-pointer stat/access ABI for this 4.19 VFS, while
  retaining the latest upstream implementations for modern kernels.
- Add the new SUSFS task flags and deferred sus-path refresh worker used by
  current KSU setuid handling. Existing SUSFS features remain configured.
- Install the scoped su descriptor through the post-exec callback. Clear the
  exec marker on success/failure and avoid dereferencing a null filename when
  executing an existing file object.
- Make the SELinux symbols required by the new upstream feature accessible to
  built-in KSU; retain the existing production debug configuration.
- Remove obsolete KSU configuration names from the Wayne defconfig.

All upstream inline-hook, compatibility and static-symbol checks still run.
This is not merely removal of the upstream stop condition.

## Validation

Wayne configuration: `vendor/wayne-oss-perf_defconfig`. The AOSP Clang 20 / LLD
build commands and virtual-machine setup are documented in `third-update.md`.
The production Image.gz and Wayne/Jasmine device trees compiled with exit 0.
The final committed-source build also passed with exit 0, using code commit
`4743dbf6c261`. Release: `4.19.325-cip132-st16-LKGeek-perf-g4743dbf6c261`.
Image compression validation, source diff checks and symlink-target checks passed.

Enable the additional KSU runtime assertions with:

```sh
KSU_TEST=1 CC=clang QEMU=qemu-system-aarch64 DTC=/path/to/build/scripts/dtc/dtc \
  tools/testing/selftests/bpf/run_third_update_vm.sh \
  /path/to/vm-build/arch/arm64/boot/Image /path/to/uapi/include /path/to/results
```

All 41 assertions passed in five consecutive fresh ARM64 QEMU boots:

- Existing JMP32/JIT, BPF-to-BPF, helper, map, ring buffer, BTF, cgroup and log tests.
- Driver descriptor installation through the KSU reboot hook.
- Current KSU UAPI and exact pinned upstream version queries.
- SUSFS 2.1 version query through the updated KSU dispatch path.
- Legacy access/stat syscall hook execution.
- Explicit memlock budget setup for the rapid BPF batch. Earlier repeated runs
  exposed transient EPERM when RCU-delayed program releases exceeded the test
  process's default 64 KiB budget; the harness now sets its own 16 MiB limit.
  Production resource-limit behavior is unchanged.

The VM has no Qualcomm hardware or Android manager/ksud installation. Existing
Qualcomm device warnings and the synthetic init argv warning are retained in
its log; the successful test runs contain no panic. Physical boot, Android root
authorization, manager upgrades, modules and SELinux policy features still need
device testing. Use a manager/daemon compatible with the updated upstream UAPI.
This update does not install an APK or flash the device.

## Artifacts

Outputs and detailed logs are stored in
`/home/lkdenchin/LKGeek_sdm660/.third-update-artifacts/ksu-update/`.
The previous Lineage integration artifacts are retained separately.
This directory contains kernel/DTB outputs, not an Android boot image.

```text
4a3fbfbc2fed852a62107b27010ed502a0bcc14ce3e17be4822a8a4956dc89ae  Image.gz
578f2434725c18c1b605c490614d4b644b8c1514f59859d15007b1a3549689d9  sdm660-mtp-wayne.dtb
20a7c6bd7e06f3048bd1697f92d074347431bf6efb664739c07a490221c25d58  sdm660-mtp-jasmine.dtb
d768b981b52126edb6416be464d647e328f32269e91602a0d31cc62591c84c86  wayne.config
4b1663993d78a4200d434493ebdd5968ba665681df5d623c7da3454a9ed815d3  vm.config
```
