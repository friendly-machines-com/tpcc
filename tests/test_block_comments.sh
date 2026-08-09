#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-block-comments-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/block_comments.cc" tests/block_comments.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/block_comments.cc" \
	"$tmp/system.cc" \
	-o "$tmp/block_comments"
ASAN_OPTIONS=detect_leaks=1 "$tmp/block_comments"

if ./mp -Furtl -o"$tmp/rejected.cc" \
	tests/block_comment_unterminated.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an unterminated parenthesized block comment" >&2
	exit 1
fi
if ! rg -Fq 'missing end comment' "$tmp/stderr"
then
	echo "wrong unterminated block-comment diagnostic" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

echo "block-comment tests passed"
