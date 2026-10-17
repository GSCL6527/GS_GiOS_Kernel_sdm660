#!/system/bin/sh
# Restore the existing SDM660 thermal daemon's missing init registration.
set -eu
[ "$(id -u)" = 0 ] || { echo 'Root is required' >&2; exit 1; }
backup=/data/local/tmp/lkgeek-thermal-init-backup
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
target=/vendor/etc/init/lkgeek-thermal-engine.rc
reserve=/sys/fs/ext4/mmcblk0p62/reserved_clusters
old_reserve=
rollback=0
restore_file() {
    if [ -f "$backup/original.rc" ]; then
        cat "$backup/original.rc" > "$target"
        restorecon "$target"
    else
        rm -f "$target"
    fi
}
cleanup() {
    if [ "$rollback" = 1 ]; then restore_file || true; fi
    sync
    if [ -n "$old_reserve" ]; then echo "$old_reserve" > "$reserve"; fi
    mount -o remount,ro /vendor
}
case "${1:-status}" in
apply)
    [ "$(getprop ro.board.platform)" = sdm660 ]
    test -x /vendor/bin/thermal-engine
    test -s /vendor/etc/thermal-engine.conf
    test -f "$here/lkgeek-thermal-engine.rc"
    # Refuse duplicate service definitions supplied by another ROM.
    if grep -R -l '^service thermal-engine ' /vendor/etc/init /system/etc/init /system_ext/etc/init 2>/dev/null | grep -v -F "$target" >/dev/null; then
        echo 'An existing thermal-engine init definition must be checked first.' >&2
        exit 1
    fi
    if [ ! -d "$backup" ]; then
        mkdir -m 700 "$backup"
        cp -p /vendor/etc/thermal-engine.conf "$backup/thermal-engine.conf"
        if [ -f "$target" ]; then cp -p "$target" "$backup/original.rc"; fi
    fi
    mount -o remount,rw /vendor
    trap cleanup EXIT HUP INT TERM
    if [ -e "$reserve" ]; then old_reserve=$(cat "$reserve"); echo 0 > "$reserve"; fi
    rollback=1
    cat "$here/lkgeek-thermal-engine.rc" > "$target"
    chmod 0644 "$target"
    restorecon "$target"
    cmp "$here/lkgeek-thermal-engine.rc" "$target"
    rollback=0
    echo 'Thermal init installed using the existing daemon and unchanged configuration; reboot loads it.'
    ;;
restore)
    test -d "$backup"
    mount -o remount,rw /vendor
    trap cleanup EXIT HUP INT TERM
    if [ -e "$reserve" ]; then old_reserve=$(cat "$reserve"); echo 0 > "$reserve"; fi
    restore_file
    echo 'Previous init registration restored; reboot applies it.'
    ;;
status)
    getprop init.svc.thermal-engine
    pidof thermal-engine || true
    getprop init.svc.android.thermal-hal
    ;;
*) echo 'Usage: sh rom-thermal-compat.sh apply|restore|status' >&2; exit 2 ;;
esac
