#!/system/bin/sh
# Run as root on the phone. Existing state is saved for an explicit restore.
set -eu
backup=/data/local/tmp/lkgeek-rom-compat
case "${1:-status}" in
apply)
    if [ ! -d "$backup" ]; then
        mkdir -m 700 "$backup"
        settings get global disable_window_blurs > "$backup/blur"
        getprop persist.logd.audit.rate > "$backup/audit-rate"
    fi
    wm disable-blur 1
    setprop persist.logd.audit.rate 0
    auditctl -r 0
    ;;
restore)
    test -d "$backup"
    blur=$(cat "$backup/blur")
    if [ "$blur" = null ]; then
        wm disable-blur 0
        settings delete global disable_window_blurs
    else
        wm disable-blur "$blur"
    fi
    rate=$(cat "$backup/audit-rate")
    setprop persist.logd.audit.rate "$rate"
    auditctl -r "${rate:-5}"
    ;;
status)
    wm disable-blur
    getprop persist.logd.audit.rate
    ;;
*) echo 'Usage: sh rom-compat.sh apply|restore|status' >&2; exit 2 ;;
esac
