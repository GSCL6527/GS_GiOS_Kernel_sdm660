# Wayne power and display investigation

User reports intermittent screen wake, a backlight flash at sleep, excessive
video power, and intermittent inversion of shutdown/restart menu outcomes.
These are separate acceptance checks; a static change is not a runtime pass.

## Phantom reference-board keys

The live gpio-keys device advertises keys 115, 528 and 766. Framework logs show
FOCUS (528) and CAMERA (766) presses immediately followed by WAKE_REASON_WAKE_KEY.
The generic SDM660 reference device tree installs camera focus/snapshot GPIOs
64/113; Wayne has no corresponding physical camera keys. The Xiaomi MI overlay
already removes these nodes, but Wayne does not include that overlay.

Remove these two nodes only from the Wayne board and stop gpio-keys configuring
those unrelated TLMM pins. Retain the PM660L volume-up default pin configuration
and volume-up key. Do not filter all wake keys or disable fingerprint/touch wake.
This should eliminate this evidenced wake source; other legitimate notifications,
power-key or charging wakes must still be distinguished in logs.

## Governors and power accounting

Current CPU policies use schedutil, with no rate-limit override (0/0). GPU uses
msm-adreno-tz. Policy0 supports 633600..1843200 kHz and policy4
1113600..2208000 kHz: the big policy is already at its hardware-table minimum
while idle. Do not invent a lower supported frequency or lower peak limits.
The earlier 500/20000 schedutil trial did not improve scrolling and was reverted.
Deep sleep is enabled (lpm_levels.sleep_disabled=N). PowerHAL exposes real
INTERACTION/LAUNCH/EXPENSIVE_RENDERING stats and requires workload-specific
residency measurements before changing hints. USB charging measurements do not
establish battery-life improvement.

The failed global SurfaceFlinger 22/10.5ms timing trial remains reverted.
Keep the verified Venus hardware-decoder compatibility repair and Pixie TCP.

## Pending display and power-menu evidence

The first coarse screen-off capture observed actual WLED falling to zero and
remaining there. It cannot exclude a shorter flash between samples. A native
5ms sampler captures actual_brightness and panel power state for a tighter
sleep/wake transition check. Kernel LED cached brightness is not actual WLED.

Restart and shutdown kernel callbacks are distinct: restart prepares PMIC reset;
poweroff selects PON_POWER_OFF_SHUTDOWN. Warm-reboot policy only applies to the
restart path. No inversion has been established from these static checks.
Capture the menu's actual sys.powerctl request and terminal PMIC logs before
changing reset encodings or swapping actions. A plugged-in charging boot can
also resemble an unwanted restart and must be distinguished from command reversal.


## Installed kernel regression

7a667247c129 built Image.gz and device trees successfully; gzip/embedded config,
Wayne mount contract and ghost-key absence checks passed. Normal-ramdisk boot
SHA256 8900dcdad4e5a9930c8fdb60f88344bc38b0a377619f361bc0ce897959cfa2b8
was installed after a full old-partition backup, then matched full readback.
Android booted as 7a667247c129 and the real gpio-keys device advertises only 115.
A 20-second screen-off observation stayed Asleep with actual WLED zero. This
bounded check does not prove all intermittent wake/flash cases are fixed.
Wi-Fi reconnected; Pixie, keymaster and vibrator service state checks succeeded.
A separate 16MiB temporary userdata/USB transfer matched byte-for-byte, with the
file removed afterward. No new kernel oops appeared; MDSS notifier warning remains.

## LKTeamFix boot-job interference

The user pointed out /vendor/LKTeamFix (case-sensitive). Its inspected boot jobs
are not portable governor profiles. ramset kills logd, replaces the WLAN log
directory with an empty mode-000 file, drops caches, compacts memory and creates
a half-hourly cache/compaction/trim job. It also has a shell syntax error, missing
modules/paths, and tries to force an input device out of runtime power management.
scheduled raises GPU/DDR trip points to 125000; actual gpu-usr and gpu-step
trip points were all 125000 before quarantine. It also invokes unavailable
radio/debug tools and opaque helper binaries. uiboost's script does not exist.
These findings are concrete compatibility/logging/thermal problems, not reasons
to suppress their error messages or disable thermal protection.

The reversible tools/wayne/rom-legacy-tuning.sh installer backs up vendor init
rules/build.prop and the matching cron files under
/data/local/tmp/lkgeek-lkteamfix-backup. It removes only those three boot-start
commands, retains their original script files, and removes the exact scheduled
/data/cron.sh entry without dropping other cron jobs. WLAN logging is restored
to a wifi-owned mode-0770 directory if the old empty regular file is present.
It removes the legacy quick-poweron/hibernate configuration in favor of normal
shutdown. Original settings are backed up; this configuration cleanup is not
proof the intermittent menu inversion is repaired. A reboot is required to
reset the old job's runtime thermal overrides and reload the init declarations.
Post-reboot thermal and performance checks are required before claiming success.

The original scripts changed additional settings and disabled some packages;
quarantine does not guess or reset every historical user preference. Do not
reenable unrelated packages, copy reference radio commands or reinstate the
failed global SurfaceFlinger timing trial as a generic power optimization.
