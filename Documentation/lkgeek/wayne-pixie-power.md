# Pixie TCP and Wayne runtime compatibility

The final kernel retains Pixie TCP as requested. CUBIC was used temporarily as
a diagnostic control, not as the final algorithm. The previous Pixie requested
65535 * 12 bytes per connection with GFP_ATOMIC. This approximately 768 KiB
request requires a high-order contiguous allocation; device logs show repeated
order-8 allocation failures from pixie_init during WLAN receive processing.
The old u16 index could also address entry 65535 beyond the allocation, its
feedback math divided by an unchecked ACK sum, and ACK processing printed
unconditionally. These defects can increase CPU work, memory fragmentation and
log turnover. They do not establish that every application stall is TCP-related.

The repair retains Pixie's configured rate and ACK/loss feedback model while
aggregating feedback into 256 time buckets. Storage is 3072 bytes per connection
instead of 786420, with bounded indices and expiration work. The bucket width
covers the original feedback interval to within bucket granularity rather than
retaining only the most recent 256 ACKs. The timestamp comparison handles u32
wrap; wide counters and bounded products prevent arithmetic overflow. Zero ACKs
and allocation failure use the base pacing rate, and the congestion window is
kept positive. Invalid rate/feedback parameters are rejected. ACK-path printk
is removed without suppressing unrelated network errors.

The defconfig already requested DEFAULT_PIXIE, but Kconfig had no such choice.
Add the missing choice and DEFAULT_TCP_CONG mapping so regeneration actually
selects Pixie. Verify both the resulting configuration and the live sysctl.

Run the host regression against the actual implementation with:

```
tools/testing/selftests/net/pixie/run.sh
```

ASan/UBSan completed 304149 assertions across 100000 samples, timestamp wrap,
ring eviction, zero ACKs, extreme counters, pacing caps, invalid parameters,
allocation failure and 1000 socket lifetimes. This is a host arithmetic and
memory-lifetime check, not a substitute for kernel TCP/throughput tests.

## Other findings

* The unneeded Keymaster 4.0 service repeatedly fails because libkeymaster4.so
  is absent. The manifest declares 3.0 and the 3.0 service is running. Stopping
  only the failed 4.0 service stops that retry loop; persistent installation and
  verification remain separate from this runtime check. Do not erase keys.
* The Bluetooth UART initially marks an unrequested wakeup IRQ enabled. Its
  first runtime resume then enables the main IRQ before request_irq, causing
  irq_startup warnings. Mark wakeup freed/disabled until startup requests it.
* MDSS event_timer replaces an existing PM QoS IRQ affinity notifier, producing
  a startup warning. The conflicting ownership still requires a coordinated
  fix; do not hide the warning in generic IRQ code.
* Thermal HAL type 8 is BCL_PERCENTAGE. The soc value is battery capacity and
  must not be reinterpreted as degrees Celsius. CPU/GPU/skin thermal protection
  is retained.
* An initial 500/20000 us schedutil trial did not improve the scrolling
  measurements, so the original 0/0 values were restored. Do not ship it as an
  established performance improvement.

The phone is USB-connected and charging. Battery current is therefore not an
isolated system power measurement; no percentage battery-life improvement is
claimed from these captures. Compare frame deadlines and CPU/GPU residency
under matched conditions, then validate unplugged discharge separately.

## Device validation of c362f9b70541

The normal-ramdisk image was flashed and its complete payload matched the boot
partition readback. Android booted as c362f9b70541, with Pixie selected and the
previous 150000000-byte/s configured target preserved. A native on-device TCP
server checked Pixie on every accepted socket; 64 connections transferred a
total of 64 MiB with matching payload hashes. This is an on-device TCP test via
ADB forwarding, not a WLAN speed measurement. The new captures show no Pixie
high-order allocation failures or ACK printk flood.

Bluetooth off/on succeeded, the 3.0 keymaster and keystore remained running, and
kernel warnings dropped from two to one: the UART warning no longer appeared;
the known MDSS notifier ownership warning remains. ROM compositor cache and GL
backpressure settings loaded after reboot, and blur remained disabled through
the one-shot boot policy. These results do not establish that Bilibili vertical
video stutter is fixed: that scene exposed an independent UBWC buffer-size
mismatch, documented in wayne-vertical-video.md.

`tools/wayne/rom-power-compat.sh apply` installs this tested ROM policy from the
three files in that directory. Its backup resides on the phone under
/data/local/tmp/lkgeek-rom-power-backup. `restore` restores original vendor
properties and the orphan service declaration; it requires reboot to reload
compositor configuration. It does not erase keystore data or change CPU/GPU
maximum frequencies or thermal limits. The original Pixie target is saved in
persist.lkgeek.pixie.rate. Blur policy can be opted out with
persist.lkgeek.blur_compat=0; the earlier rom-compat.sh restore restores its prior
runtime preference. The original vendor build.prop lacked a final newline;
the installer explicitly inserts one before appending properties.


## Orphan vibrator service

The Oplus lmvibrator service repeatedly exited successfully and restarted every
five seconds, while the actual QTI vibrator AIDL service was already running.
The reversible installer now adds disabled/oneshot only when that QTI service
is running and the orphan declaration exists. Its original declaration is
backed up alongside the other vendor originals. The orphan was stopped on the
phone, QTI remained running, and a 100 ms framework vibration request completed.
Physical vibration strength was not independently observed. This eliminates an
invalid service retry loop; it is not a measured battery-life improvement.
