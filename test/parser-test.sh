#!/bin/bash

testdir=parser
default_rootdev=ps3da1
mnt=${PREFIX}/var/petitboot/mnt

#set -ex

tests=$(ls ${mnt}/${testdir}/)

for test in $tests
do
	rootdev=$default_rootdev

	if [ -e "${mnt}/${testdir}/$test/rootdev" ]; then
		rootdev=$(cat "${mnt}/${testdir}/$test/rootdev")
	fi

	./test/parser-test "${testdir}/$test" $rootdev

#	./test/parser-test "${testdir}/$test" $rootdev 2>/dev/null |
#		diff -u "${mnt}/${testdir}/$test/expected-output" -
done

echo "All tests passed"
