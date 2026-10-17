#!/system/bin/sh
# Remove an init write intended for a newer GPU, preserving the GPU service.
set -eu
backup=/data/local/tmp/lkgeek-rom-init-backup
rc=/system/etc/init/gpuservice.rc
node=/sys/class/kgsl/kgsl-3d0/dcvs_boost
case "${1:-status}" in
apply)
    test -d /sys/class/kgsl/kgsl-3d0
    if [ -e "$node" ]; then
        echo 'GPU supports dcvs_boost; init configuration retained.'
        exit 0
    fi
    test -f "$rc"
    mkdir -p "$backup"
    chmod 0700 "$backup"
    if [ ! -f "$backup/gpuservice.rc" ]; then cp -p "$rc" "$backup/gpuservice.rc"; fi
    sed '\|^[[:space:]]*write /sys/class/kgsl/kgsl-3d0/dcvs_boost 1[[:space:]]*$|d' "$rc" > "$backup/gpuservice.next"
    test -s "$backup/gpuservice.next"
    mount -o remount,rw /
    trap 'sync; mount -o remount,ro /' EXIT HUP INT TERM
    cat "$backup/gpuservice.next" > "$rc"
    restorecon "$rc"
    echo 'Removed unsupported GPU init write; GPU service retained. Reboot applies init changes.'
    ;;
restore)
    test -f "$backup/gpuservice.rc"
    mount -o remount,rw /
    trap 'sync; mount -o remount,ro /' EXIT HUP INT TERM
    cat "$backup/gpuservice.rc" > "$rc"
    restorecon "$rc"
    ;;
status)
    ls -l "$rc"
    if [ -e "$node" ]; then echo 'dcvs_boost present'; else echo 'dcvs_boost unavailable'; fi
    ;;
*) echo 'Usage: sh rom-init-compat.sh apply|restore|status' >&2; exit 2 ;;
esac
