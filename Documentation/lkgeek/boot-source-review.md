# Wayne first-logo hang: source review

The installed CI image identified during device inspection is based on
`931f00200ea7`. It hangs at the first logo. The provided Recovery log is from
4.19.264, not the installed 4.19.325 kernel; pstore was empty. The supplied
logdump/logfs partitions contain only zeroes, and rawdump contains no recognizable
kernel crash record. These captures do not identify the failing instruction.

A temporary, unflashed Recovery using its original working kernel/ramdisk and a
ramoops reservation booted successfully. Its console survived a controlled
Recovery -> bootloader -> temporary Recovery sequence. No failed-kernel console
was recovered after the user's forced restart. A temporary test with the new
kernel and original Recovery ramdisk did not expose USB/ADB; the planned old-DTB
control test could not be sent because the computer stopped enumerating the
phone. These observations narrow the investigation but do not prove the cause.

## Confirmed source issues corrected

1. **Wayne first-stage mounts removed.** The imported
   xiaomi-sdm660-common.dtsi deletes /firmware/android/fstab. The original Wayne
   tree supplied both system and vendor there with read-only ext4 mounts and
   `wait`, without slotselect/AVB. The inspected installed boot ramdisk contains
   init and installer extras, with no fstab of its own. The supplied Recovery
   fstab explicitly documents that system/vendor are early-mounted from the DT,
   and its log identifies static system/vendor partitions (mmcblk0p61/p62).
   Restore the complete original mount contract in wayne.dtsi, after the common
   deletion. This is the strongest concrete compatibility explanation found for
   failure before Android userspace comes up. Jasmine and other devices retain
   their imported mount policy. Mount flags match the original Wayne tree; no
   partition contents are changed.
   Device boot confirmation is still required.

2. **GICv3 firmware access policy.** The import removed the original
   ARM_GIC_V3_NO_ACCESS_CONTROL switch and unconditionally accessed GICR_WAKER.
   Wayne has GICv3 access control enabled; its previous configuration left the
   removed switch disabled. Restore the switch and the original guarded/no-op
   behavior, explicitly disabled in Wayne's defconfig. Non-Qualcomm configurations
   retain the imported behavior by default. This code runs in interrupt-controller
   initialization before ramoops registers, making it a credible early-hang
   candidate. Only a device boot can establish causality.
3. **Special interrupt IDs.** The imported IRQ drain loop stopped only at 1023.
   Exit for the complete special/reserved ID range 1020-1023 (and other IDs outside
   the supported normal/LPI ranges), keeping regular IRQ, SGI and LPI processing.
   A special ID must not keep the CPU spinning inside the interrupt handler.
4. **Early memory policy applied twice.** Remove the duplicated
   update_memory_limit() call and obsolete bootloader-limit assignment. Save the
   bootloader address limit before applying the offline-memory policy once.
   With CONFIG_OVERRIDE_MEMORY_LIMIT disabled the policy function is a no-op;
   this is an integration defect, not proof of the reported hang.
5. **SCM shared-memory failure handling.** Return -ENODEV instead of calling
   gen_pool_alloc() on a missing bridge pool. Clear pool/buffer pointers on
   initialization cleanup so a failed initialization cannot leave stale objects.
6. **GPU/display QoS masks.** cpumask_empty() only queries a mask. Use
   cpumask_clear() before populating the KGSL and rotator affinity masks, including
   repeated initialization. This corrects QoS behavior; it is not established as
   the first-logo cause.
7. **Truthful embedded configuration.** Generate IKCONFIG from KCONFIG_CONFIG,
   not the unrelated sdm660-perf-full_defconfig. Previously extracted settings
   (including pstore and device options) could falsely describe the booted image.
8. **RAM accounting API consumers.** KGSL, TCP metrics and NTFS compared the
   address of the totalram_pages inline function with memory thresholds instead
   of calling it; dm-writecache also retained the old variable syntax. Convert
   all executable consumers to totalram_pages() and remove the stale export of
   the inline function (the backing _totalram_pages is already exported).

## Other paths reviewed

Compared startup assembly, MMU setup, SMP, RNG initialization, clock/CPR changes,
MMC reset/probe changes, reserved-memory/Wayne device-tree changes and
KernelSU/SUSFS execution/LSM hooks against the original local tree. The prior
ARM64 global stack-canary compiler fix and the non-LTO BPF allocator guard remain
in place. No additional causative defect was established in these paths. Do not
infer that all hardware/firmware, Android SELinux policy or userspace combinations
have been verified by this review.

## Validation

The final production build uses vendor/wayne-oss-perf_defconfig, AOSP Clang 20
r547379, LLVM integrated assembler, ThinLTO and LLD. Image.gz and both Wayne /
Jasmine DTBs passed. Release: 4.19.325-cip132-st16-LKGeek-perf-ga038726b1142.

Passed artifact checks:

- gzip decompression reproduces Image exactly;
- IKCONFIG extracted from the finished Image equals the actual .config;
- Wayne has the restored GIC access policy and pstore, KSU and BPF JIT enabled;
- libfdt checks of the finished Wayne DTB confirm complete system/vendor entries,
  device paths, ext4, read-only mount flags, wait and available status;
- both DTBs decompile and retain ramoops; Jasmine's DTB checksum is unchanged;
- ARM64 UAPI headers_install and git diff --check pass.

Image.gz SHA-256:
09621e8dc521f928e42ffe3b7c422e6105eec83db22faf765e59ad1280b7bc1f.
Wayne DTB SHA-256:
f3837868bbbb4f093422cb2f3c780d1df828769434f4ea365627ff4ba52eeb63.
Combined Wayne Image.gz-dtb SHA-256:
e9b5dd96cbd5dce94db7180e8c8b12470f3e8dceaa9e8e64bf0cf2d1239f38d0.

The combined artifact includes the restored DT mounts. Deploy the updated DTB
along with the kernel when packaging a boot image. It is a kernel/DT payload,
not a complete Android boot image or a flashable installer.

A full non-LTO kernel build/link also passed for the virtual-platform derivative
of the Wayne configuration (LTO_NONE=y, MODULES=n, BPF_JIT=y). It enables the
virtual board/console/devtmpfs/reset devices and disables the Qualcomm early RNG;
QEMU uses a virtual DT and skips qtee_shmbridge_init, which needs TrustZone.
The restored GIC access-control option remains disabled, as on production Wayne.

QEMU 11.1.2 booted that kernel to the freestanding test init: all 41 assertions
passed, ending in THIRD_UPDATE_RUNTIME_PASS. These cover all 22 JMP32 forms,
actual JIT execution, BPF subprogram calls, helper/maps/ring buffer/BTF/cgroup
operations, KSU descriptor/UAPI/exact upstream version, legacy SUSFS/access/stat
hooks, and delivery of all five formerly filtered /dev/kmsg messages. No kernel
panic, Oops or SError was found in the successful run. The runtime kernel carries
f7c9b397a581; the only subsequent source change before production a038726b1142
was the Wayne DT mount restoration (plus its report), so executable kernel C code
is identical. Software results are retained in boot-source-review-runtime.txt.

Existing nonfatal compiler/vendor DT warnings remain. This is not a claim of a
warning-free build, a rerun of the remote Actions workflow, or a successful phone
boot. Android first-stage mounting and Qualcomm hardware must still be checked
on the actual phone with the updated kernel and DTB. The interrupted USB
connection prevented completion of that physical validation.
Local outputs and private device captures are under the ignored
`.third-update-artifacts/boot-review/` and `device-debug/` directories. Raw phone
logs/partition contents are not committed.
