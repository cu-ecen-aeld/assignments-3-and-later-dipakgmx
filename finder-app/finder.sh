#!/bin/bash

if [ $# -eq 2 ]
then
	filesdir=$1
	searchstr=$2
else
	echo "Script expected 2 parameters whereas received $# parameters"
	exit 1
fi

if [ ! -d "$filesdir" ]
then
	echo "Directory $1 does not exist"
	exit 1
fi

filecount=$(find "$filesdir" -type f | wc -l)
matchcount=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $filecount and the number of matching lines are $matchcount"



