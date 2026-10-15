# Wayne V4L2 registration boot deadlock

A real-device warm-reset capture of kernel a038726b1142 ends with
`calling msm_vidc_init` at 0.776878 seconds without its return. The console was
retrieved from ramoops in Recovery. A header-only diagnostic with
`initcall_blacklist=msm_vidc_init` advances through camera SMMU and camera core
initialization, then stops in `msm_cci_init_module` instead. Neither test reaches
Android ADB. These are two captures of the new kernel, not Recovery's own log.

Both drivers register V4L2 devices. The imported __video_register_device takes
videodev_lock immediately before device_register and never releases it, including
on the success return. The next registration blocks acquiring the same global
mutex. Its registration failure path calls put_device, whose release callback
also acquires videodev_lock, so that path can deadlock as well.

Remove this unmatched acquisition. Preserve the earlier release callback setup
and v4l2_device_get, which ensure the registration failure path has a valid
release callback and balanced lifetime. Existing short critical sections still
protect minor allocation, lookup and removal. No hardware driver is disabled.

Also restore the camera core's media_entity_pads_init return-value assignment.
The imported comparison expression always makes its less-than-zero error test
false and loses the actual initialization error. This is a separate error-path
bug, not evidence of a second cause of the reported boot hang.

## Diagnostic reset handling

Forced Power/VolumeUp resets originally triggered a PMIC hard reset and erased
RAM, hiding the failed kernel console. For this investigation only, Recovery and
the temporary boot DT set qcom,pon_1 qcom,s2-type from hard reset (7) to warm reset
(1). A Recovery-to-Recovery control preserved an explicit console marker, and
both subsequent failed-kernel captures contain the new kernel's own initcalls.
Original Recovery is backed up locally and must be restored after diagnostics.
This diagnostic reset-policy change is not part of the production source fix.

## Validation status

Source lock and lifetime paths reviewed; git diff --check passes. Production
vendor/wayne-oss-perf_defconfig Image.gz and DTBs rebuilt successfully; gzip
decompression matches Image. A temporary boot with all media drivers enabled
and no initcall blacklist reaches the second screen according to the user,
where earlier boots stopped at the first logo. The device then loops in the Android boot animation. Android ADB and full
system boot are not yet confirmed. Earlier successful QEMU checks do not establish successful
phone boot.


## Subsequent Android watchdog

The fixed kernel completes VIDC and camera initcalls and runs Android for more
than 240 seconds without a captured kernel panic. The recovered system_server
ANR reports its main thread waiting on the ActivityManagerService lock held by
thread 290. That thread is waiting in LmkdConnection.waitForConnection while
applying OOM adjustments. Init logs show lmkd repeatedly exiting with status 0.
The installed lmkd binary contains PSI and vmpressure monitor support and an
explicit error for kernels without either pressure events or an in-kernel LMK.

The production Wayne config has both PSI and MEMCG disabled. Enable PSI with its
normal enabled-at-boot default, MEMCG and swap accounting so Android has pressure
monitoring and per-process memory control interfaces. This addresses a concrete
missing interface; the watchdog's disappearance and complete phone boot still
require a fresh device test. The old kernel's extracted IKCONFIG was inaccurate
and is not used to infer that these features were absent in the working kernel.
