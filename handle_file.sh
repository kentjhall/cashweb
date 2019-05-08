#!/bin/bash

# Unfortunately this had to be a separate script because of how find -exec works

if [ "$#" -lt 3 ]; then
	echo "usage: ./handle_file <file-path> <fee-satoshis-per-chunk> <dir-path> [bitcoin-cli flags]"
	exit 1
fi

if [ "$1" != "$3/dir.cash" ]; then
	echo "/${1#*/}**$(./send_file.sh $1 $2 $4)*" | tr -d '\n' >> $3/dir.cash
fi
