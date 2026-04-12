#!/bin/sh

#first argument directory path
filesdir="$1"
#second argument the string to search
searchstr="$2"

#Check if both parameters have been specified, otherwise return error 1
if [ "$#" -ne 2 ]; then
	echo "ERROR: MISSING PARAMETERS"
	exit 1
fi

#Check if filesdir is a filesystem directory, otherwise return error 1
if [ ! -d "$filesdir" ]; then
	echo "ERROR: $filesdir IS NOT A DIRECTORY"
	exit 1
fi

X=$(find "$filesdir" -type f | wc -l)
Y=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $X and the number of matching lines are $Y"
