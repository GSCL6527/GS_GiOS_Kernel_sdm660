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


### Quarantine reboot check

After the controlled normal reboot Android again ran 7a667247c129. The three
legacy services had never started (empty init.svc state), the cron entry was
absent, WLAN logs remained a wifi-owned directory, and logd ran as PID 524.
Quick power-on reads false and shutdown mode reads normal. GPU thermal trips
returned to their defaults: gpu-usr critical 125000/passive 95000 and gpu-step
95000. The 125000 critical hardware threshold is intentional and is not removed;
the erroneous override of the lower control trip has been undone.

Without the old scheduled job forcing both schedutil rate limits to zero, the
vendor defaults are now up=500 us/down=20000 us. Top-app has CPUs 0-7, foreground
0-3,6-7, normal frequency maxima remain, and the GPU governor is msm-adreno-tz.
A same-scene A/B/A trial compares these defaults to the old zero/zero override;
its script restores the vendor defaults and original screen timeout on exit.
Frame and residency results are needed before claiming a performance/power gain.
No autonomous energy-saving percentage or successful power-menu repair is claimed.


### Governor A/B/A evidence

All three 12-switch StoryVideoActivity runs completed with both log markers and
zero hardware decoder errors, software fallbacks and in-window qbuf failures.
The vendor-default A runs rendered 920/367 application frames with 50/45 missed
deadlines (5.43%/12.26%); zero/zero B rendered 406 with 52 misses (12.81%).
The different rendered-frame counts and evolving video/cache/thermal state mean
this does not establish a fixed improvement percentage. In the second A run,
mean frequency residency was 1294.7/1630.6 MHz for the little/big policies versus
1308.2/1596.1 MHz in B, so this is not evidence of a net power reduction either.
The vendor defaults are retained without forcing an unproven zero/zero override.
Frequency residency includes idle time and is not an energy measurement.
The original screen timeout was restored after the trial. Logd stayed at PID 524
throughout, with no kernel oops; the known MDSS notifier warning remains.


### Actual power-menu check after configuration repair

The installed framework's IWindowManager interface declares showGlobalActions
as transaction 116; using that verified interface opens the actual Oplus menu.
The upper restart gesture called lowLevelReboot(userrequested); the next boot
reported reboot,userrequested and completed Android startup. Preserved terminal
kernel logs explicitly show Restarting system, Going down for restart, warm
reset and secondary-PMIC configuration without a reported PMIC configuration
failure. The debug string lowLevelReboot,shutdown is misleading: the actual
method writes reboot to sys.powerctl, not shutdown.

The lower shutdown gesture called lowLevelShutdown(userrequested), with the
framework recording Performing low-level shutdown and USB subsequently absent.
Physical screen/off-mode-charging status and power-on follow-up are pending the
user. One successful restart path and one correctly dispatched shutdown request
do not prove an intermittent fault can never recur. No binary framework method
or kernel shutdown/restart encoding was swapped.
