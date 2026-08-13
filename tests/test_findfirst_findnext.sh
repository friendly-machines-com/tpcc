#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"
mkdir -p "$tmp/work/files/subdir" "$tmp/work/home"

printf abc >"$tmp/work/files/alpha1.dat"
printf 12345 >"$tmp/work/files/alpha2.dat"
printf secret >"$tmp/work/files/.secret"
printf readonly >"$tmp/work/files/readonly.dat"
touch -m -d @123456789 "$tmp/work/files/alpha1.dat"
chmod 444 "$tmp/work/files/readonly.dat"
mkfifo "$tmp/work/files/pipe"
ln -s alpha1.dat "$tmp/work/files/file-link"
ln -s subdir "$tmp/work/files/dir-link"
ln -s missing "$tmp/work/files/broken-link"


tpcc_translate -o"$tmp/findfirst_findnext.cc" \
	tests/findfirst_findnext.pp
if ! grep -Fq '::u_sysutils::p_fileage' \
	"$tmp/findfirst_findnext.cc"
then
	echo "SysUtils.FileAge did not use p_fileage" >&2
	exit 1
fi
tpcc_build "$tmp/findfirst_findnext" \
	"$tmp/findfirst_findnext.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

cd "$tmp/work"
HOME="$tmp/work/home" \
	tpcc_run \
	"$tmp/findfirst_findnext"

echo "FindFirst/FindNext tests passed"
