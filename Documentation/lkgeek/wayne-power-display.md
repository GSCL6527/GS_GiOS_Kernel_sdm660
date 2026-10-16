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
