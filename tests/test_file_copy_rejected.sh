#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-file-copy-rejected-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

for source in \
	file_copy_assignment_rejected \
	file_record_copy_rejected \
	file_value_parameter_rejected \
	file_const_parameter_rejected
do
	if ./mp -Furtl -o"$tmp/$source.cc" \
		"tests/$source.pp" >"$tmp/$source.out" 2>&1
	then
		echo "$source was accepted" >&2
		exit 1
	fi
	if ! rg -q \
		'file values and values containing files cannot be assigned|file types and types containing files require var or out parameters' \
		"$tmp/$source.out"
	then
		echo "$source produced the wrong diagnostic" >&2
		cat "$tmp/$source.out" >&2
		exit 1
	fi
done

echo "file-copy rejection tests passed"
