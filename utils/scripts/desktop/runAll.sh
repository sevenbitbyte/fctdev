#!/bin/bash

echo "Connecting to head node"
#ssh sw1 $@
ssh sw1 ~/bin/runAll.sh $@
