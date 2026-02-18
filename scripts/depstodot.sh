#!/bin/sh

MEDIR="$(dirname "$0")"

make "$@" clean
make V=1 "$@" \
    | sed -f "$MEDIR/make-deps.sed" \
    | sort | uniq \
    | awk -f "$MEDIR/depstodot.awk" \
    | dot -Tpdf > deps.pdf

