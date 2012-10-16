#!/bin/bash

for n in `seq 2 12`; do
	scp -o ConnectTimeout=1 $1 sw$n:$2
done
