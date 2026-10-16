# Wayne vertical-video decoder compatibility

The user identified Bilibili StoryVideoActivity (vertical videos, swipe up/down)
as the main stutter case. Earlier list-page measurements do not establish that
this scene is repaired. During a controlled eight-switch capture, HEVC codec
instances repeatedly reported OMX error 0x80001009 and IJK logged fallback to
ffplay decoding. The kernel simultaneously returned EINVAL from vb2_qbuf.

Temporary level-1 VB2 diagnostics identified the actual rejection:

* a 1241088-byte native buffer was smaller than the 1277952-byte setup size;
* a 1544192-byte native buffer was smaller than the 1589248-byte setup size.

The imported UAPI formula adds 64 luma scanlines to the interlaced UBWC worst
case, including progressive resolutions within that branch. The original
Venus 3.x allocator contract did not include this addition. For 960x720 the
captured firmware output requirement was 1159168 bytes; the legacy decoder
estimate is 1212416, within the native buffer's 1241088 bytes. For 1280x720 the
legacy estimate is 1507328, within the observed 1544192-byte native buffer.

Keep the compatibility adjustment local to vidc_3x's NV12 UBWC decoder sizing.
Do not change the shared UAPI layout or relax generic VB2 checks. Queue setup
also takes the maximum of the native-format estimate and the actual firmware
buffer requirement, so it will still reject storage smaller than firmware needs.
Invalid dimensions return zero and the larger non-interlaced-size branch is
unchanged. A host check compiles the actual decoder helper with ASan/UBSan and
checks both captured size cases and these boundary conditions.

Separate diagnostic issue: debugfs instance names used %pK. With pointer
redaction enabled every instance was named zero, so concurrent instances failed
to create diagnostic directories. Use hashed %p for unique diagnostic names;
continue redacting addresses in normal logs.

## Installed-device validation

The production Image.gz and device trees built successfully. Gzip payload,
embedded configuration and Wayne device-tree checks passed. The normal-ramdisk
boot image was flashed and its payload exactly matched partition readback;
Android reports 4.19.325-LKGeek-perf-g5af5ff1e137b.

The first bounded 12-switch StoryVideoActivity capture recorded zero HEVC
0x80001009 errors, zero software-decoder fallback messages and zero qbuf errors.
358 application frames were rendered; 60 missed their deadlines (16.76%). The
user confirmed a clear improvement, with remaining animation frame drops.
A second 12-switch capture recorded 354 frames and 71 missed deadlines (20.06%),
again with no HEVC hardware error or fallback. These are separate online-content
runs, not a controlled estimate of a percentage performance improvement.

The second full-boot kernel snapshot contains six qbuf EINVAL messages before
the capture's start. A cumulative dmesg count cannot attribute them to that
12-switch window; they need independent lifecycle investigation. Application
frame counters use before/after deltas because gfxinfo reset does not reliably
reset every window's cumulative counters on this ROM.

For the last 120 Story frames in the second capture, GPU completion after submit
had a 7.97 ms 95th percentile, while application dispatch delay and main-thread
work had 58.43 ms and 89.73 ms 95th percentiles. This identifies remaining CPU-side
frame spikes but does not establish whether the cause is scheduling, application
work or blocking. Further compositor timing trials are reversible and are not
a validated production change yet. Temporary VB2/VIDC diagnostic levels were
restored; debugfs is unmounted after inspection.


## Reversible 60 Hz compositor timing

The imported ROM used late app/SF budgets of 23.5/10.5 ms (34 ms total), early
16.5/21 ms and early-GL 21/13.5 ms. Each total exceeds two 60 Hz display periods.
A temporary restart with 22/10.5 ms for all three modes was confirmed in the
SurfaceFlinger dump: app phase changed from about 16 ms to 0.833 ms. The user
reported a further improvement in vertical animation. Two 12-switch trials
recorded 60/411 and 116/462 missed deadlines (14.60% and 25.11%), with no hardware
codec errors, software fallback or qbuf failures inside either window. The first
trial's log start marker was lost at stream startup, so its codec-log coverage
is incomplete; the second has both markers. Different content, cache state and
changed deadline budgets prevent a quantitative before/after improvement claim.
Animation drops remain; user-perceived latency improvement is not a zero-jank
result.

Subsequent user testing found a regression across the whole phone, superseding
the earlier scene-specific positive feedback. The duration trial is rejected:
all six original duration properties were restored both at runtime and in vendor
build.prop, followed by a SurfaceFlinger restart. The installer no longer writes
these duration properties. Do not reapply the 22/10.5 ms trial based on its budget
arithmetic or the earlier limited scene feedback. The independently validated
Venus decoder compatibility fix remains installed. CPU/GPU limits and thermal
protection remain unchanged.
