#!/vendor/bin/sh
# Adapter for this Oplus ROM's debug.tracing.screen_brightness init trigger.
# Oplus normal brightness is 0..8191; Wayne's MDSS LED exports its own maximum.
set -eu
node=/sys/class/leds/lcd-backlight
value=${1:-}
case "$value" in -1|-1.0) value=0 ;; esac
case "$value" in
    *.*) value=${value%%.*} ;;
esac
case "$value" in
    ''|*[!0-9]*) exit 1 ;;
esac
# Reject excessively large input before shell arithmetic.
[ ${#value} -le 5 ] || exit 1
[ "$value" -le 8191 ] || value=8191
read -r maximum < "$node/max_brightness"
case "$maximum" in ''|*[!0-9]*) exit 1 ;; esac
[ "$maximum" -gt 0 ] && [ "$maximum" -le 65535 ] || exit 1
level=$(( (value * maximum + 4095) / 8191 ))
printf '%s\n' "$level" > "$node/brightness"
