#!/bin/sh


if [ -z "$1" ] || [ -z "$2" ]; then
   echo "Error: Please provide both directory path and search string."
  exit 1
fi

if [ ! -d "$1" ]; then
   echo "Error: '$1' is not a  directory or does not exist."
   exit 1
fi

file_count=0
line_count=0

file_count=$(find "$1" -type f | wc -l )
line_count=$(grep -r "$2" "$1" | wc -l )

echo " The number of files are $file_count and the number of matching lines are $line_count" 

