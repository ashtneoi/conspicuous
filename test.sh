#!/usr/bin/env bash


fail() { echo $1; exit 1; }

for name in $(basename -a tests/cpic/*); do
	echo $name
	./cpic -p 16F1454 tests/cpic/$name >/tmp/cpic.test.hex \
		|| fail "cpic failed"
	gpasm -w 1 -p 16F1454 -a INHX8M -o /tmp/gpasm.test.hex tests/gpasm/$name \
		|| fail "gpasm failed"
	diff /tmp/cpic.test.hex /tmp/gpasm.test.hex || fail "Files differ"
done
