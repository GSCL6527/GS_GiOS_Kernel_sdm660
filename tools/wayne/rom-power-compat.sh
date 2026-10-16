#!/system/bin/sh
# Reversible installer for the tested Wayne Oplus vendor layout only.
set -eu
[ "$(id -u)" = 0 ] || { echo 'Root is required' >&2; exit 1; }
backup=/data/local/tmp/lkgeek-rom-power-backup
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reserve=/sys/fs/ext4/mmcblk0p62/reserved_clusters
vibrator_rc=/vendor/odm/etc/init/vendor.oplus.hardware.lmvibrator@1.0-service.rc
patch_vibrator=0
old_reserve=
rollback=0
cleanup() {
    if [ "$rollback" = 1 ]; then
        cat "$backup/build.prop" > /vendor/build.prop || true
        cat "$backup/keymaster4.rc" > /vendor/etc/init/android.hardware.keymaster@4.0-service.rc || true
        if [ -f "$backup/oplus-vibrator.rc" ]; then
            cat "$backup/oplus-vibrator.rc" > "$vibrator_rc" || true
        fi
        rm -f /vendor/bin/lkgeek-rom-power-boot.sh /vendor/etc/init/lkgeek-rom-power.rc
        setprop persist.lkgeek.pixie.rate "$(cat "$backup/pixie-rate-property")" || true
    fi
    sync
    if [ -n "$old_reserve" ]; then echo "$old_reserve" > "$reserve"; fi
    mount -o remount,ro /vendor
}
case "${1:-status}" in
apply)
    test -f /vendor/etc/init/android.hardware.keymaster@3.0-service-qti.rc
    test -f /vendor/etc/init/android.hardware.keymaster@4.0-service.rc
    test -f "$here/rom-power-boot.sh"
    test -f "$here/lkgeek-rom-power.rc"
    if [ ! -d "$backup" ]; then
        mkdir -m 700 "$backup"
        cp -p /vendor/build.prop "$backup/build.prop"
        cp -p /vendor/etc/init/android.hardware.keymaster@4.0-service.rc "$backup/keymaster4.rc"
        getprop persist.lkgeek.pixie.rate > "$backup/pixie-rate-property"
    fi
    if [ -f "$vibrator_rc" ] && [ "$(getprop init.svc.vendor.qti.vibrator)" = running ]; then
        if [ ! -f "$backup/oplus-vibrator.rc" ]; then
            cp -p "$vibrator_rc" "$backup/oplus-vibrator.rc"
        fi
        patch_vibrator=1
    fi
    if [ -z "$(getprop persist.lkgeek.pixie.rate)" ] &&
       [ -r /sys/module/tcp_pixie/parameters/rate ]; then
        setprop persist.lkgeek.pixie.rate "$(cat /sys/module/tcp_pixie/parameters/rate)"
    fi
    mount -o remount,rw /vendor
    if [ -e "$reserve" ]; then
        old_reserve=$(cat "$reserve")
        echo 0 > "$reserve"
    fi
    trap cleanup EXIT HUP INT TERM
    rollback=1
    sed '/^debug\.sf\.disable_client_composition_cache=/d; /^debug\.sf\.enable_gl_backpressure=/d; /^debug\.sf\.disable_backpressure=/d' /vendor/build.prop > "$backup/build.prop.next"
    printf '\n' >> "$backup/build.prop.next"
    cat >> "$backup/build.prop.next" <<'PROPS'
debug.sf.disable_client_composition_cache=0
debug.sf.enable_gl_backpressure=1
debug.sf.disable_backpressure=0
PROPS
    cat "$backup/build.prop.next" > /vendor/build.prop
    if ! grep -q '^[[:space:]]*disabled[[:space:]]*$' /vendor/etc/init/android.hardware.keymaster@4.0-service.rc; then
        printf '\n    disabled\n' >> /vendor/etc/init/android.hardware.keymaster@4.0-service.rc
    fi
    if [ "$patch_vibrator" = 1 ]; then
        if ! grep -q '^[[:space:]]*disabled[[:space:]]*$' "$vibrator_rc"; then
            printf '\n    disabled\n' >> "$vibrator_rc"
        fi
        if ! grep -q '^[[:space:]]*oneshot[[:space:]]*$' "$vibrator_rc"; then
            printf '    oneshot\n' >> "$vibrator_rc"
        fi
        restorecon "$vibrator_rc"
        setprop ctl.stop vendor.lmvibrator-1-0
    fi
    cat "$here/rom-power-boot.sh" > /vendor/bin/lkgeek-rom-power-boot.sh
    cat "$here/lkgeek-rom-power.rc" > /vendor/etc/init/lkgeek-rom-power.rc
    chmod 0755 /vendor/bin/lkgeek-rom-power-boot.sh
    chmod 0644 /vendor/build.prop /vendor/etc/init/lkgeek-rom-power.rc
    restorecon /vendor/build.prop /vendor/bin/lkgeek-rom-power-boot.sh /vendor/etc/init/lkgeek-rom-power.rc /vendor/etc/init/android.hardware.keymaster@4.0-service.rc
    setprop ctl.stop vendor.keymaster-4-0
    rollback=0
    echo 'Installed; reboot is required to load compositor properties and init rules.'
    ;;
restore)
    test -f "$backup/build.prop"
    test -f "$backup/keymaster4.rc"
    mount -o remount,rw /vendor
    if [ -e "$reserve" ]; then old_reserve=$(cat "$reserve"); echo 0 > "$reserve"; fi
    trap cleanup EXIT HUP INT TERM
    cat "$backup/build.prop" > /vendor/build.prop
    cat "$backup/keymaster4.rc" > /vendor/etc/init/android.hardware.keymaster@4.0-service.rc
    if [ -f "$backup/oplus-vibrator.rc" ]; then
        cat "$backup/oplus-vibrator.rc" > "$vibrator_rc"
        restorecon "$vibrator_rc"
    fi
    rm -f /vendor/bin/lkgeek-rom-power-boot.sh /vendor/etc/init/lkgeek-rom-power.rc
    setprop persist.lkgeek.pixie.rate "$(cat "$backup/pixie-rate-property")"
    restorecon /vendor/build.prop /vendor/etc/init/android.hardware.keymaster@4.0-service.rc
    echo 'Vendor policy restored; reboot is required. Runtime blur can be restored with rom-compat.sh restore.'
    ;;
status)
    getprop debug.sf.disable_client_composition_cache
    getprop debug.sf.enable_gl_backpressure
    getprop init.svc.keymaster-3-0
    getprop init.svc.vendor.keymaster-4-0
    /system/bin/wm disable-blur
    cat /proc/sys/net/ipv4/tcp_congestion_control
    ;;
*) echo 'Usage: sh rom-power-compat.sh apply|restore|status' >&2; exit 2 ;;
esac
