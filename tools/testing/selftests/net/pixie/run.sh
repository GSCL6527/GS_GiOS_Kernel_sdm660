#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
${CC:-cc} -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$here/include" "$here/test.c" -o "$build/test"
"$build/test"
