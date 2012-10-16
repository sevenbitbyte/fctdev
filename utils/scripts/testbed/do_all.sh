#!/bin/bash

let log=0

if [ $1 = "-l" ]; then
	let log=1
	shift
fi

let first=$1
let last=$2
shift 2

for i in `seq $first $last` ; do
	echo "==> sw$i <=="
	if [ $log -eq 1 ]
		then ${@//\$i/sw$i} > sw$i.out 2> sw$i.err
		else ${@//\$i/sw$i}
	fi
	echo ""
done
echo "==> done <=="
