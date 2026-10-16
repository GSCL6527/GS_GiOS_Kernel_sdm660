# Wayne APF compatibility and connection delay

The normal OSS test image boots with the original, non-debug ramdisk. Its WLAN
firmware advertises APF version 2. Android's network stack nevertheless asks the
vendor HAL to read APF working memory. The driver dispatches this through its
APF 3.0 command path and waits for a response the legacy firmware does not send.

Actual device captures show `hdd_apf_read_memory: Target response timed out` at
72.587, 82.827 and 93.067 seconds. IpClient logs contain matching waits of roughly
ten seconds before pre-DHCP completion, IPv4 address configuration and subsequent
filter reads. The on-air DHCP Discover/Offer/Request/Ack exchange in an earlier
capture took approximately 27 milliseconds. Authentication and DHCP success do
not establish that the full connection or Internet validation was timely.

The driver's own APF debug reporting selects legacy `apf_enabled_v2` for version
2 or lower and the interpreter context for newer versions. Reject WRITE, READ,
ENABLE and DISABLE work-memory/interpreter commands for legacy firmware with
EOPNOTSUPP before scheduling firmware work. Keep legacy SET and GET commands,
packet filtering, and Linux BPF/JIT available. Do not silently return fake packet
contents or globally disable BPF.

The combined kernel and device-tree build passed. Two checked live reconnects
obtained IPv4 in 1.49 and 1.539 seconds, and Internet VALIDATED state in 2.756
and 2.21 seconds. No APF read timeout appeared in the new captures. Initial
pre-DHCP completion took approximately 24 milliseconds. Legacy filter operation
has not been exhaustively tested with crafted packets.
The initial wrong phone date also caused HTTPS probe certificate failures;
automatic time later corrected it. This is a separate ROM startup-time issue,
not evidence that APF fixes all possible Wi-Fi failures.

Raw network logs remain private under .third-update-artifacts/device-debug/
flash-test/reconnected-test and are not included in public source commits.
