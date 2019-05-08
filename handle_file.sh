#!/bin/bash

# Unfortunately this had to be a separate script because of how find -exec works

if [ "$#" -lt 2 ]; then
	echo "usage: ./handle_file <file-path> <dir-path> [bitcoin-cli flags]"
	exit 1
fi

if [ "$1" != "$2/dir.cash" ]; then
	echo "/${1#*/}**$(./send_file.sh $1 $3)*" | tr -d '\n' >> $2/dir.cash
fi
