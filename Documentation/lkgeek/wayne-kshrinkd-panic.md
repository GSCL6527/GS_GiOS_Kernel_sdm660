# Wayne delayed reboot: kshrinkd NULL memcg

A normal OSS boot (40df597a5de0-dirty, including the subsequent build fixes) ran
for 516 seconds before a confirmed kernel panic. The recovered pstore reports a
NULL dereference at address 0x7c, with `shrink_slab+0x330/0x780` called from
`kshrinkd+0xa0/0x240`. It is not merely an Android camera-service restart.

The custom kshrinkd loop initialized memcg to NULL and called shrink_slab before
fetching the first cgroup. Once MEMCG is enabled, NULL is not the root cgroup and
the call enters shrink_slab_memcg with an invalid target. Initialize the iterator
before the first call, as drop_slab_node already does. Continue the full iterator
until NULL so reference handling and the MEMCG-disabled single global pass are
preserved. Also replace unsigned-underflow priority modulo with an explicit
wrap to DEF_PRIORITY, matching the loop's intended priority sequence.

The combined fix built Image.gz and device trees successfully. The production
ramdisk image was flashed, read back and booted as 1a28b19f6284. A bounded 1.5 GiB
anonymous-memory test touched, verified and released every allocated page;
kshrinkd CPU ticks increased from 0 to 19. The phone then remained running past
1,400 seconds without the previous panic. This exercises the failing reclaim
path, but does not establish unlimited stability or resolve every display issue.
The previously observed camera HAL close stall remains a separate observation;
the user subsequently confirmed normal camera operation.

Private pstore: .third-update-artifacts/device-debug/flash-test/reconnected-test/
reboot-after-hang/pstore. Keep the original capture locally rather than committing
raw device logs or device identifiers.
