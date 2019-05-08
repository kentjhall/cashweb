#!/bin/bash

if [ "$#" -lt 1 ]; then
	echo "usage: ./send_dir.sh <dir-path> [bitcoin-cli flags]"
	exit 1
fi

echo "*" | tr -d '\n' > $1/dir.cash

find $1 -type f -exec ./handle_file.sh "{}" $1 $2 \;

t=$(./send_file.sh $1/dir.cash $2) # I just don't want this to print to stdout
echo
echo $(cat $1/dir.cash)
rm $1/dir.cash
