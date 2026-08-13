#!/bin/sh

# Shared policy for TPCC's shell test drivers.
#
# Sourcing this file establishes an isolated temporary directory, changes to
# the repository root, and installs cleanup. Individual tests retain only
# their inputs, test-specific compiler options, assertions, and diagnostics.
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_name=${0##*/}
test_name=${test_name#test_}
test_name=${test_name%.sh}
tmp=${TMPDIR:-/tmp}/tpcc-"$test_name"-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"
cd "$root"

# Invoke TPCC with the unit path common to every Pascal test. Arguments remain
# ordinary compiler arguments so a test can add its own unit paths, defines,
# output name, and source file.
tpcc_translate()
{
	./mp -Furtl "$@"
}

# Build and link translated Pascal under one suite-wide policy. Pascal permits
# storage aliasing which the generated C++ represents directly, so every such
# build disables type-based alias analysis. Sanitizer recovery is disabled so
# a detected runtime defect cannot be hidden by a later successful assertion.
tpcc_build()
(
	output=$1
	shift
	"${CXX:-g++}" \
		-std=c++20 \
		-Wall \
		-Wextra \
		-Wpedantic \
		-fno-strict-aliasing \
		-fsanitize=address,undefined \
		-fno-sanitize-recover=all \
		-Irtl \
		-I"$tmp" \
		"$@" \
		-o "$output"
)

# The frame-intrinsics test inspects generated stack frames. Sanitizer
# instrumentation would change the subject, so that one test uses this build
# while retaining every other generated-code option.
tpcc_build_unsanitized()
(
	output=$1
	shift
	"${CXX:-g++}" \
		-std=c++20 \
		-Wall \
		-Wextra \
		-Wpedantic \
		-fno-strict-aliasing \
		-Irtl \
		-I"$tmp" \
		"$@" \
		-o "$output"
)

# Type-check generated C++ without linking it. This uses the same backend
# contract as a generated-code build, except for runtime instrumentation which
# cannot execute in a syntax-only compilation.
tpcc_check_generated()
{
	"${CXX:-g++}" \
		-std=c++20 \
		-Wall \
		-Wextra \
		-Wpedantic \
		-fno-strict-aliasing \
		-Irtl \
		-I"$tmp" \
		-fsyntax-only \
		"$@"
}

# Handwritten C++ tests of the compiler itself do not inherit assumptions made
# by generated Pascal code.
tpcc_build_native()
(
	output=$1
	shift
	"${CXX:-c++}" \
		-std=c++20 \
		-Wall \
		-Wextra \
		-Wpedantic \
		"$@" \
		-o "$output"
)

# Keep the sanitizer runtime policy beside the sanitizer build policy.
# Callers may prefix this function with a different ASAN_OPTIONS assignment
# when the behavior under test requires an additional sanitizer option.
tpcc_run()
{
	ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "$@"
}
