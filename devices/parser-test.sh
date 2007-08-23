#!/bin/bash

testdir=devices/parser-tests

function test_dir()
{
	dir="$1"
	./parser-test "$dir" 2>/dev/null |
		diff -u "$dir/expected-output" -
}

set -ex

for test in $testdir/*
do
	echo $test
	test_dir "$test"
done

echo "All tests passed"
