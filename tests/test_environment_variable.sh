#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-environment-variable-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

long=
i=0
while test "$i" -lt 400
do
	long="${long}x"
	i=$((i + 1))
done
bytes=$(printf '\200\377')
unset TPCC_ENV_MISSING

cd "$root"

./mp -Furtl -o"$tmp/environment_variable.cc" \
	tests/environment_variable.pp
if ! rg -Fq '::u_system::p_getenvironmentvariable' \
	"$tmp/environment_variable.cc"
then
	echo "GetEnvironmentVariable did not use its SysUtils RTL operation" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/environment_variable.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/environment_variable"

TPCC_ENV_PRESENT='alpha beta:gamma' \
TPCC_ENV_EMPTY= \
TPCC_ENV_LONG="$long" \
TPCC_ENV_BYTES="$bytes" \
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/environment_variable"

echo "GetEnvironmentVariable tests passed"
