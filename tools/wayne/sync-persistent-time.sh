#!/system/bin/sh
# Restore Qualcomm's persistent wall-clock offsets after trusted network time.
# Run only after checking that Android shows the correct date and time.
set -eu
[ "$(id -u)" = 0 ] || { echo 'Root is required' >&2; exit 1; }
[ "$(settings get global auto_time)" = 1 ] || { echo 'Enable automatic time first' >&2; exit 1; }
now=$(date +%s)
[ "$now" -ge 1755786785 ] || { echo 'System time is older than this ROM build' >&2; exit 1; }
src=/data/vendor/time
dst=/mnt/vendor/persist/time
[ -d "$dst" ] || { echo 'Persistent time directory is missing' >&2; exit 1; }
for n in 1 2 16; do
    [ "$(wc -c < "$src/ats_$n")" -eq 8 ] || { echo "Invalid ats_$n" >&2; exit 1; }
done
backup=/data/local/tmp/lkgeek-time-backup-$now
mkdir -m 700 "$backup"
for n in 1 2 16; do
    [ ! -e "$dst/ats_$n" ] || cp -p "$dst/ats_$n" "$backup/ats_$n"
    cat "$src/ats_$n" > "$dst/ats_$n"
    chown system:system "$dst/ats_$n"
    chmod 600 "$dst/ats_$n"
    restorecon "$dst/ats_$n"
done
sync
echo "Persistent offsets synchronized; previous files saved in $backup"
