# Wayne graphics stall: dropped irq_work callbacks

The V4L2 and memory-pressure fixes let Android complete boot, but unlocking could
freeze or blank the display. A temporary debug ramdisk allowed root ADB without
changing system/vendor partitions. This is not a successful usability test.

## Device evidence

SurfaceFlinger blocks in Fence::waitForever -> sync_wait -> ppoll while queuing a
GPU-composed frame. Reading its pollfd from /proc/PID/mem identified fd 166. The
sync_file diagnostic shows a pending KGSL fence, context 8, sequence 46646, while
KGSL reports the SurfaceFlinger context retired through 46645. Other captures
show queued sync objects waiting on merged SystemUI fences even after component
GPU timestamps have progressed. No kernel fix should force these fences complete
or remove their synchronization dependencies.

The added sync_file fdinfo reports each component fence's driver, timeline,
context, sequence and status. It retains file ownership while inspecting the
fence and uses the normal fence status API. It improves diagnosis without
changing submission policy or bypassing waits.

## Source defect and correction

The cumulative integration broke kernel/irq_work.c in two places:

- irq_work_run_list cleared PENDING/BUSY but never called work->func(work).
  dma_fence_array_cb_func queues irq_dma_fence_array_work to signal the merged
  fence. Dropping that callback leaves consumers waiting indefinitely even
  though component fences have completed. This also affects other irq_work
  users, including tracing/perf paths.
- irq_work_claim combined atomic_fetch_or with a second compare-exchange claim
  algorithm. After setting PENDING itself, the second algorithm commonly sees
  PENDING and rejects the work before enqueueing it.

Restore the coherent original atomic claim/pending check and runner: clear
PENDING, order accesses with the matching memory barrier, invoke the callback
within its lockdep context, and release BUSY only if it has not been requeued.
This preserves callback self-requeue and duplicate queue rejection semantics.

## Validation

Production Image.gz and DTB build and device verification are in progress. A
functional desktop, unlock, suspend/resume and repeated graphics activity must be
checked after this fix. Earlier lockscreen/boot_completed observations alone
are insufficient to declare success.
