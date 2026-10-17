#!/vendor/bin/sh
# One-shot compatibility policy for the tested Wayne Oplus port.
set -eu
if [ "$(getprop persist.lkgeek.blur_compat)" != 0 ]; then
    /system/bin/wm disable-blur 1
fi
key=oplus_customize_status_bar_enable_when_lock
if [ "$(/system/bin/settings get system "$key")" = null ]; then
    /system/bin/settings put system "$key" 1
fi
saved_rate=$(getprop persist.lkgeek.pixie.rate)
case "$saved_rate" in
    ''|*[!0-9]*) ;;
    *)
        if [ -w /sys/module/tcp_pixie/parameters/rate ]; then
            printf '%s\n' "$saved_rate" > /sys/module/tcp_pixie/parameters/rate
        fi
        ;;
esac
if grep -qw pixie /proc/sys/net/ipv4/tcp_available_congestion_control; then
    printf 'pixie\n' > /proc/sys/net/ipv4/tcp_congestion_control
fi
