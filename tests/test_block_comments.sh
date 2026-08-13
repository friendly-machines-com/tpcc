#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/block_comments.cc" tests/block_comments.pp

tpcc_build "$tmp/block_comments" \
	"$tmp/block_comments.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/block_comments"

if tpcc_translate -o"$tmp/rejected.cc" \
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
