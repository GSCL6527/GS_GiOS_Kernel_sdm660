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

The fix is pending build and device pressure/stability validation. A successful
boot alone does not establish that the delayed-panic regression is fixed. The
previously observed camera HAL close stall remains a separate unresolved issue
until its own regression checks pass.

Private pstore: .third-update-artifacts/device-debug/flash-test/reconnected-test/
reboot-after-hang/pstore. Keep the original capture locally rather than committing
raw device logs or device identifiers.
