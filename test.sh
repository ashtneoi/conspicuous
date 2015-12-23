#!/usr/bin/env bash


X="/tmp/j8zjv89wjepfq283f8hnzvj89jfwije"

shopt -s nullglob

fail() {
	echo "FAIL: $1" >&2
	exit 1
}

for name in tests/assemble/*.s; do
	echo $name
	./cpic $name >${X}.a || fail "$name failed to assemble"
	diff ${X}.a ${name%.s}.hex || fail "$name didn't assemble correctly"
done

for name in tests/assemble-fail/*.s; do
	echo $name
	./cpic $name >/dev/null && fail "$name assembled successfully"
done

T=$(echo tests/same-A/*.s)
if ! [[ -z ${T:+nonempty} ]]; then
	for name in $(basename -a $T); do
		echo $name
		./cpic tests/same-A/$name >${X}.a||
			fail "tests/same-A/$name didn't assemble correctly"
		./cpic tests/same-B/$name >${X}.b||
			fail "tests/same-B/$name didn't assemble correctly"
		diff ${X}.{a,b} || \
			fail "tests/same-A/$name is not equivalent to tests/same-B/$name"
	done
fi

rm -f ${X}.{a,b}
