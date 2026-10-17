#!/system/bin/sh
# Align the ported launcher's resolution profile with WindowManager.
set -eu
backup=/data/local/tmp/lkgeek-launcher-density-backup
case "${1:-status}" in
apply)
    density=$(wm density | awk '/Physical density:/ {d=$3} /Override density:/ {d=$3} END {print d}')
    case "$density" in ''|*[!0-9]*) echo 'Unable to read display density' >&2; exit 1 ;; esac
    if [ "$density" -lt 160 ] || [ "$density" -gt 960 ]; then
        echo 'Display density outside supported range' >&2; exit 1
    fi
    # Oplus treats 420 as a resolution enum and translates it to 480.
    if [ "$density" = 420 ]; then
        echo 'This launcher reserves 420 as a resolution enum; app support is required for exact 420 DPI.' >&2
        exit 1
    fi
    mkdir -p "$backup"
    chmod 0700 "$backup"
    if [ ! -f "$backup/original-display-density.prop" ]; then
        getprop persist.sys.display.density > "$backup/original-display-density.prop"
    fi
    previous=$(getprop persist.sys.display.density)
    setprop persist.sys.display.density "$density"
    if [ "$previous" != "$density" ]; then
        am force-stop com.android.launcher
        am start -a android.intent.action.MAIN -c android.intent.category.HOME
    fi
    echo "Launcher resolution density: $density DPI; desktop data retained."
    ;;
restore)
    test -f "$backup/original-display-density.prop"
    setprop persist.sys.display.density "$(cat "$backup/original-display-density.prop")"
    am force-stop com.android.launcher
    am start -a android.intent.action.MAIN -c android.intent.category.HOME
    ;;
status)
    wm density
    getprop persist.sys.display.density
    ;;
*) echo 'Usage: sh rom-launcher-density.sh apply|restore|status' >&2; exit 2 ;;
esac
