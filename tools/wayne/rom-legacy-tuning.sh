#!/system/bin/sh
# Quarantine the inspected LKTeamFix boot jobs; retain their original files.
set -eu
[ "$(id -u)" = 0 ] || exit 1
backup=/data/local/tmp/lkgeek-lkteamfix-backup
reserve=/sys/fs/ext4/mmcblk0p62/reserved_clusters
old=
cleanup() {
    sync
    if [ -n "$old" ]; then echo "$old" > "$reserve"; fi
    mount -o remount,ro /vendor
}
case "${1:-status}" in
apply)
    test -d /vendor/LKTeamFix
    for name in scheduled ramset uiboost; do test -f "/vendor/etc/init/$name.rc"; done
    if [ ! -d "$backup" ]; then
        mkdir -m 700 "$backup"
        cp -p /vendor/build.prop "$backup/build.prop"
        getprop persist.sys.shutdown.mode > "$backup/shutdown-mode"
        for name in scheduled ramset uiboost; do
            cp -p "/vendor/etc/init/$name.rc" "$backup/$name.rc"
        done
        if [ -f /data/cron.d/root ]; then cp -p /data/cron.d/root "$backup/cron-root"; fi
        if [ -f /data/cron.sh ]; then cp -p /data/cron.sh "$backup/cron.sh"; fi
    fi
    mount -o remount,rw /vendor
    old=$(cat "$reserve")
    echo 0 > "$reserve"
    trap cleanup EXIT HUP INT TERM
    for name in scheduled ramset uiboost; do
        # Each inspected declaration is disabled/oneshot, with only this trigger.
        sed "/^[[:space:]]*start $name[[:space:]]*$/d" "/vendor/etc/init/$name.rc" > "$backup/$name.next"
        cat "$backup/$name.next" > "/vendor/etc/init/$name.rc"
        restorecon "/vendor/etc/init/$name.rc"
        setprop ctl.stop "$name"
    done
    sed '/^ro\.config\.hw_quickpoweron=/d; /^persist\.sys\.shutdown\.mode=/d' /vendor/build.prop > "$backup/prop.next"
    printf '\nro.config.hw_quickpoweron=false\npersist.sys.shutdown.mode=normal\n' >> "$backup/prop.next"
    cat "$backup/prop.next" > /vendor/build.prop
    restorecon /vendor/build.prop
    setprop persist.sys.shutdown.mode normal
    if [ -f /data/cron.d/root ]; then
        sed '\|^[[:space:]]*\*/30 \* \* \* \* /data/cron.sh[[:space:]]*$|d' /data/cron.d/root > "$backup/cron.next"
        cat "$backup/cron.next" > /data/cron.d/root
        chmod 0600 /data/cron.d/root
    fi
    if [ -f /data/cron.sh ]; then chmod 0700 /data/cron.sh; fi
    # The old job replaced this directory with an empty mode-000 regular file.
    if [ -f /data/vendor/wlan_logs ] && [ ! -s /data/vendor/wlan_logs ]; then
        cp -p /data/vendor/wlan_logs "$backup/wlan-logs-file"
        rm /data/vendor/wlan_logs
        mkdir /data/vendor/wlan_logs
        chown wifi:wifi /data/vendor/wlan_logs
        chmod 0770 /data/vendor/wlan_logs
        restorecon /data/vendor/wlan_logs
    fi
    echo 'Quarantined boot jobs; reboot to restore kernel thermal defaults.'
    ;;
restore)
    test -f "$backup/build.prop"
    mount -o remount,rw /vendor
    old=$(cat "$reserve")
    echo 0 > "$reserve"
    trap cleanup EXIT HUP INT TERM
    for name in scheduled ramset uiboost; do
        cat "$backup/$name.rc" > "/vendor/etc/init/$name.rc"
        restorecon "/vendor/etc/init/$name.rc"
    done
    cat "$backup/build.prop" > /vendor/build.prop
    restorecon /vendor/build.prop
    setprop persist.sys.shutdown.mode "$(cat "$backup/shutdown-mode")"
    if [ -f "$backup/cron-root" ]; then cp -p "$backup/cron-root" /data/cron.d/root; fi
    if [ -f "$backup/cron.sh" ]; then cp -p "$backup/cron.sh" /data/cron.sh; fi
    # Preserve new diagnostic logs; do not recreate the broken mode-000 file.
    echo 'Original boot jobs restored; reboot required. This re-enables their thermal/logging overrides.'
    ;;
status)
    for name in scheduled ramset uiboost; do getprop "init.svc.$name"; done
    getprop persist.sys.shutdown.mode
    ls -ld /data/vendor/wlan_logs
    ;;
*) echo 'Usage: sh rom-legacy-tuning.sh apply|restore|status' >&2; exit 2 ;;
esac
