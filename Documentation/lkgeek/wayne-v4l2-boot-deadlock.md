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
vendor/wayne-oss-perf_defconfig rebuild and real-device boot verification are in
progress. Earlier successful QEMU checks do not establish successful phone boot.
