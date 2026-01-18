#!/bin/bash

if [ $# -eq 2 ]
then
        writefile=$1
        writestr=$2
else
        echo "Script expected 2 parameters whereas received $# parameters"
        exit 1
fi

dirpath=$(dirname "$writefile")
mkdir -p "$dirpath"

touch $writefile
echo "$writestr">$writefile


