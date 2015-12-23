#!/usr/bin/env bash


shopt -s nullglob


fail() {
	echo "FAIL: $1"
	exit 1
}


for name in tests/assemble/*.s; do
	echo $name
	diff <(./cpic $name || fail "$name assemble failed ($?)") \
		${name%.s}.hex
done

for name in tests/assemble-fail/*.s; do
	echo $name
	./cpic $name >/dev/null && fail "$name assemble succeeded ($?)"
done

T=$(echo tests/same-A/*.s)
if ! [[ -z ${T:+nonempty} ]]; then
	for name in $(basename -a $T); do
		echo $name
		diff <(./cpic tests/same-A/$name) <(./cpic tests/same-B/$name) ||
			fail "tests/same-A/$name doesn't match tests/same-B/$name"
	done
fi
