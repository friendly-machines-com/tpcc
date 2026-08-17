#!/bin/sh
set -eu

# tests/ fixture naming policy, checked mechanically so a human never has to
# discover it again:
#
#   .pp       Pascal test source
#   .expected checked-in expected translator output (goldens)
#   .cpp      handwritten C++ test source
#   .error    first-line diagnostic expectation
#
# A .cc file in tests/ is always a stray: the translator writes generated
# output into $tmp via -o, so any .cc here is either an in-place compile
# leftover (which can silently overwrite a golden) or checked-in junk.
. "$(dirname -- "$0")/testlib.sh"

strays=$(cd tests && ls -- *.cc 2>/dev/null || true)
if [ -n "$strays" ]; then
	echo "stray generated .cc files in tests/:" >&2
	echo "$strays" >&2
	exit 1
fi

for golden in tests/*.expected; do
	[ -e "$golden" ] || continue
	if [ ! -f "tests/$(basename "$golden" .expected).pp" ]; then
		echo "golden $golden has no matching .pp input" >&2
		exit 1
	fi
done

echo "fixture hygiene tests passed"
