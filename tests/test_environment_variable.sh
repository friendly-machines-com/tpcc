#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

long=
i=0
while test "$i" -lt 400
do
	long="${long}x"
	i=$((i + 1))
done
bytes=$(printf '\200\377')
unset TPCC_ENV_MISSING


tpcc_translate -o"$tmp/environment_variable.cc" \
	tests/environment_variable.pp
if ! rg -Fq '::u_sysutils::p_getenvironmentvariable' \
	"$tmp/environment_variable.cc"
then
	echo "GetEnvironmentVariable did not use its SysUtils RTL operation" >&2
	exit 1
fi
if ! rg -Fq '::u_baseunix::p_fpgetenv' \
	"$tmp/environment_variable.cc"
then
	echo "BaseUnix.FpGetEnv did not use its borrowed-pointer RTL operation" >&2
	exit 1
fi

tpcc_build "$tmp/environment_variable" \
	"$tmp/environment_variable.cc" \
	"$tmp/baseunix.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

TPCC_ENV_PRESENT='alpha beta:gamma' \
TPCC_ENV_EMPTY= \
TPCC_ENV_LONG="$long" \
TPCC_ENV_BYTES="$bytes" \
tpcc_run \
	"$tmp/environment_variable"

echo "GetEnvironmentVariable tests passed"
