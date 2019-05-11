#!/bin/bash

if [ "$#" -lt 1 ]; then
	echo "usage: ./send_dir.sh <dir-path> [bitcoin-cli flags]"
	exit 1
fi

dircash_temp=".dircash"
echo -n > $1/$dircash_temp

find $1 -type f -exec ./handle_file.sh "{}" $1 $2 $dircash_temp \;
dir_tx=$(./send_file.sh $1/$dircash_temp $2)

echo >&2
cat $1/$dircash_temp >&2
echo >&2

echo -n $dir_tx # tx to stdout
echo >&2

rm $1/$dircash_temp
