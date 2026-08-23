#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/../../../.." && pwd)
explorer_bin=$(mktemp "${TMPDIR:-/tmp}/wbst-parameter-explorer.XXXXXX")
trap 'rm -f "$explorer_bin"' EXIT HUP INT TERM

cd "$project_root"

"${CC:-cc}" \
    -O2 \
    -march=native \
    -DNDEBUG \
    -DBST_DISABLE_INVARIANTS \
    -Wall \
    -Wextra \
    -Wno-unused-parameter \
    -Wno-unused-function \
    -Wno-unused-variable \
    -Wno-unused-but-set-variable \
    -o "$explorer_bin" \
    c/bst.c \
    c/tree.c \
    c/tree_arena.c \
    c/tree_persistence.c \
    c/tree_measurements.c \
    c/tree_verify.c \
    c/random.c \
    c/weight.c \
    c/wbst_bottomup.c \
    c/wbst_topdown.c \
    c/wbst_parameter_explorer.c \
    -lm

"$explorer_bin" "$@"
