#!/bin/bash

if [ -z "$1" ] || [ -z "$2" ]; then
        echo "Error: Please provide both directory path and a string to write."
        exit 1
fi

writefile="$1" 
dirname=$(dirname "$writefile")

if [ ! -d "$dirname" ]; then
       mkdir -p "$dirname"
fi

if ! echo "$2" > "$writefile"; then 
        echo "Error: could not write to file"
        exit 1
fi 


echo "Successfully wrote '$2' to '$writefile'."

