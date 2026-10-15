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

1. **GICv3 firmware access policy.** The import removed the original
   ARM_GIC_V3_NO_ACCESS_CONTROL switch and unconditionally accessed GICR_WAKER.
   Wayne has GICv3 access control enabled; its previous configuration left the
   removed switch disabled. Restore the switch and the original guarded/no-op
   behavior, explicitly disabled in Wayne's defconfig. Non-Qualcomm configurations
   retain the imported behavior by default. This code runs in interrupt-controller
   initialization before ramoops registers, making it a credible early-hang
   candidate. Only a device boot can establish causality.
2. **Special interrupt IDs.** The imported IRQ drain loop stopped only at 1023.
   Exit for the complete special/reserved ID range 1020-1023 (and other IDs outside
   the supported normal/LPI ranges), keeping regular IRQ, SGI and LPI processing.
   A special ID must not keep the CPU spinning inside the interrupt handler.
3. **Early memory policy applied twice.** Remove the duplicated
   update_memory_limit() call and obsolete bootloader-limit assignment. Save the
   bootloader address limit before applying the offline-memory policy once.
   With CONFIG_OVERRIDE_MEMORY_LIMIT disabled the policy function is a no-op;
   this is an integration defect, not proof of the reported hang.
4. **SCM shared-memory failure handling.** Return -ENODEV instead of calling
   gen_pool_alloc() on a missing bridge pool. Clear pool/buffer pointers on
   initialization cleanup so a failed initialization cannot leave stale objects.
5. **GPU/display QoS masks.** cpumask_empty() only queries a mask. Use
   cpumask_clear() before populating the KGSL and rotator affinity masks, including
   repeated initialization. This corrects QoS behavior; it is not established as
   the first-logo cause.
6. **Truthful embedded configuration.** Generate IKCONFIG from KCONFIG_CONFIG,
   not the unrelated sdm660-perf-full_defconfig. Previously extracted settings
   (including pstore and device options) could falsely describe the booted image.

## Other paths reviewed

Compared startup assembly, MMU setup, SMP, RNG initialization, clock/CPR changes,
MMC reset/probe changes, reserved-memory/Wayne device-tree changes and
KernelSU/SUSFS execution/LSM hooks against the original local tree. The prior
ARM64 global stack-canary compiler fix and the non-LTO BPF allocator guard remain
in place. No additional causative defect was established in these paths. Do not
infer that all hardware/firmware, Android SELinux policy or userspace combinations
have been verified by this review.

## Validation

Validation results are recorded below after the final build and runtime checks.
Local outputs and private device captures are under the ignored
`.third-update-artifacts/boot-review/` and `device-debug/` directories. Raw phone
logs/partition contents are not committed.
