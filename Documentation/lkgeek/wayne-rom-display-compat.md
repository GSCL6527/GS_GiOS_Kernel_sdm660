# Wayne Oplus ROM display compatibility

These adapters are specific to the Oplus ROM tested on Wayne. They are not
generic kernel interfaces and should not be installed on unrelated ROMs.

## Brightness forwarding

The ported vendor init rule forwards `debug.tracing.screen_brightness` into
`/sys/touchpanel/oplus_backlight`, which is absent on this kernel. The ROM's
brightness model returns a constant 1.0 for its color conversion, so its legacy
Lights path does not provide usable brightness control either. Direct writes
to Wayne's MDSS LED work and reach the underlying WLED backlight.

`tools/wayne/oplus_backlight.rc` replaces that broken forwarding rule.
`tools/wayne/oplus-backlight.sh` belongs at
`/vendor/bin/lkgeek-oplus-backlight.sh` (0755). The init file belongs at
`/vendor/etc/init/oplus_backlight.rc` (0644). Preserve the original rule before
installation, restore SELinux file contexts and return vendor to read-only.
Init must be restarted by rebooting to load the new rule.

The helper maps this ROM's normal 0..8191 range onto the LED's actual advertised
maximum, with rounding and clamping. Negative-one display-off maps to zero;
malformed values are rejected. HBM values above the normal maximum saturate at
the panel's maximum rather than requesting unsupported high-brightness operation.
It runs on property changes rather than polling or writing continuously.

Direct adapter checks: 222/2048/4096/8191/10239 produced
111/1024/2048/4095/4095 in both MDSS and WLED readback. After reboot, changing
System brightness to 512/2048/4096/8191 produced actual WLED levels
256/1024/2048/4095. Three real notification-panel slider drags produced settings
4263/2998/3319 and actual WLED levels 2131/1499/1659. Init reported successful
adapter exits. Sleep reported panel_power_on=0 and actual_brightness=0; wake
restored the selected brightness. The MDSS LED brightness attribute retains its
requested value while blanked, so use WLED actual_brightness to test darkness.

The tested vendor filesystem has only about 148 KiB free while ext4 reserves
4096 clusters internally. Installation initially failed with ENOSPC. Temporarily
lowering `reserved_clusters` allowed the two small files to be installed; the
original reserve was restored and vendor remounted read-only. This does not
resize the filesystem or justify removing unrelated vendor files.

## SystemUI black screen

A subsequent boot showed a black screenshot while SurfaceFlinger remained
responsive. SystemUI repeatedly threw SettingNotFoundException for
`oplus_customize_status_bar_enable_when_lock` during daily QS statistics.
The existing value was absent. Setting the System setting to 1 restored
SystemUI, with launcher and notification-panel output captured afterwards.
The next reboot retained the setting and SystemUI stayed running throughout
the subsequent brightness and sleep/wake checks.
Keep an existing user preference; only supply this default when the setting
is missing. To undo this device change, delete that System setting.

## Persistent time offsets

The Qualcomm time daemon runs as system, but persistent time offsets were
root-owned and still reflected 2024. Fresh offsets under `/data/vendor/time`
reflected the correct network time. `tools/wayne/sync-persistent-time.sh`
copies only ATS 1, 2 and 16 after explicit confirmation of correct system time,
backs up the previous files, and gives system ownership with mode 0600.
The complete original time directory was also backed up privately before
repair. Do not erase persist or overwrite unrelated calibration data.
