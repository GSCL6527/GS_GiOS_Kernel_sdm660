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

Incremental build and device switching regression are required before reporting
this video fix as passed. Temporary VB2 and VIDC debug levels were restored after
capture. Debugfs was mounted for inspection and should be unmounted after the
remaining diagnostics if it was not originally mounted.
