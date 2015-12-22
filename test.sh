#!/usr/bin/env bash


fail() { echo $1; exit 1; }

for name in $(basename -a tests/assemble/*.s); do
	diff <(./cpic tests/assemble/$name) ${name%..}.hex
	./cpic $name >/dev/null || {
		echo "FAIL: $name assemble failed ($?)"
		exit 1
	}
done

for name in tests/assemble-fail/*; do
	./cpic $name >/dev/null && {
		echo "FAIL: $name assemble succeeded ($?)"
		exit 1
	}
done

for name in $(basename -a tests/same-A/*); do
	diff <(./cpic tests/same-A/$name) <(./cpic tests/same-B/$name) || {
		echo "FAIL: tests/same-A/$name doesn't match tests/same-B/$name"
		exit 1
	}
done
