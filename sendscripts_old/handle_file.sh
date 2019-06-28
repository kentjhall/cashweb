#!/bin/bash

# Unfortunately this had to be a separate script because of how find -exec works

if [ "$#" -lt 3 ]; then
	echo "usage: ./handle_file <file-path> <dir-path> <file-dircash> [bitcoin-cli flags]"
	exit 1
fi

if [ "$1" != "$2/$3" ]; then
	echo "/${1#*/}*$(./send_file.sh $1 $4)" >> "$2/$3"
fi
