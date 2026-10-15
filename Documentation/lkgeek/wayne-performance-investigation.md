# Wayne latency and blur investigation

## Observations after irq_work repair

The user confirms unlock, Settings, home and screen off/on no longer cause the
previous blank-screen hang. Severe stutter remains, especially with blur.
Production boot and original Recovery have been installed and read back.
SurfaceFlinger reports 60 Hz and uses Qualcomm Adreno 512 OpenGL ES 3.2.

Captured SF output contains repeated out-of-order buffer transaction warnings
for the launcher and HWC Unsupported (8) errors. These are not evidence that the
kernel executes GPU command buffers out of order. HWC missed-frame counts exceed
GPU missed-frame counts. The original indefinite merged-fence stall has not
recurred after irq_work repair; buffer submission/HWC and blur costs remain under
investigation.

The ROM places launcher and SystemUI in the restricted cpuset (0-3), including
while awake. Temporarily expanding it to 0-7 grants access to all CPUs but did not
remove the user's stutter. The initial observation of zero active big CPUs was a
snapshot; later core_ctl reports two, with other big CPUs isolated normally.
Do not report a proven broken core_ctl based on the earlier snapshot.

Temporarily disabling cross-window blur with wm disable-blur 1 gives a clear
user-reported improvement: no stutter. Blur was restored afterward. A temporary
GPU minimum-frequency experiment is not a final fix and must not be persisted.
No system/vendor partition was changed for these tests.

## Scheduler sysctl bounds defect

MAX_CLUSTERS is 3 and MAX_MARGIN_LEVELS is 2, so each migration-margin sysctl
array has two elements. The handler instead resized table->maxlen to
(num_sched_clusters - 1) entries without checking allocated capacity. The device
exposed three entries: up read 94 94 84; down read 84 84 0. The third up entry is
the first down entry. System.map places boost_mutex directly after the down
array, so a three-entry down write can corrupt the mutex.

Runtime parameter validation triggered this defect. Testing stopped and the
phone was rebooted into Recovery to clear temporary state. No sysctl tuning is
persisted. The defect is a correctness issue, not a tuning strategy.

Bound the handler's element count by MAX_MARGIN_LEVELS. In the update path, map
additional intermediate clusters to the final allocated margin rather than
indexing beyond the array. Preserve handling of one/two-cluster machines and
the existing final-cluster exclusion.

Validation of the new bound and real-device blur performance remains pending.
After rebuilding, test that up/down expose at most two entries and writes to
one vector leave the other unchanged, including extra input. Do not write
migration parameters on the old kernel again.
