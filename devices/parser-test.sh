#!/bin/bash

testdir=devices/parser-tests
default_rootdev=ps3da1

function test_dir()
{
	dir="$1"
	rootdev=$default_rootdev
	if [ -e "$dir/rootdev" ]
	then
		rootdev=$(cat "$dir/rootdev")
	fi
	./parser-test "$dir" $rootdev 2>/dev/null |
		diff -u "$dir/expected-output" -
}

set -ex

for test in $testdir/*
do
	echo $test
	test_dir "$test"
done

echo "All tests passed"
