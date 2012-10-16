#!/bin/bash

scp $1 sw1:$2
ssh sw1 ~/bin/copyFile.sh $2 $2

