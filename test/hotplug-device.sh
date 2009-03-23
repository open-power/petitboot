#!/bin/bash


devmapper_name=meep
file=test.ext2

if [ $# -lt 1 ]
then
	cat <<EOF
usage: $0 [add [path]|remove]
	if <path> is specified, initialise the contents of the hotplugged
	device from the directory at <path>
EOF
	exit 1
fi

sudo true

set -ex

case "$1"
in
	add)
		dd if=/dev/zero of=$file bs=1k count=640
		mkfs.ext2 -F -m0 $file

		loopdev=$(sudo losetup -f --show $loopdev $file)
		echo loop device is $loopdev

		if [ -d $2 ]
		then
			mkdir -p mount.tmp
			sudo mount $loopdev mount.tmp
			sudo rsync -av $2 mount.tmp/
			sudo umount mount.tmp
		fi

		blocksize=$(sudo blockdev --getsize $loopdev)
		echo block size = $blocksize

		sudo dmsetup create $devmapper_name \
			--table "0 $blocksize linear $loopdev 0"
	;;
	remove)
		set +e
		awk '/^\/dev\/mapper\/'$devmapper_name'/ {print $2}' \
				/proc/mounts |
		while read mountpoint
		do
			sudo umount $mountpoint
		done
		sudo dmsetup remove $devmapper_name

		sudo losetup -j $file | cut -f1 -d: |
		while read loopdev
		do
			sudo losetup -d $loopdev
		done
	;;

esac
