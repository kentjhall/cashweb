#!/bin/bash

if [ "$#" -lt 1 ]; then
	echo "usage: ./send_file.sh <file-path> [bitcoin-cli flags]"
	exit 1
fi

prefix_raw=""
prefix=$(echo -n $prefix_raw | xxd -p | tr -d '\n')
prefix_chars=$(echo -n $prefix | wc -c)
file_body=$(echo -n "$(<$1)" | xxd -p | tr -d '\n')
file_chars=$(echo -n $file_body | wc -c)
chunk_bytes=219
chunk_chars=$(($chunk_bytes*2))
file_done=false
at_end=false

tx=""
tx_bytes=0
chunk_low_pos=$(($file_chars - $chunk_chars + 1))
chunk_high_pos=$file_chars

while [ "$at_end" = false ]; do
	op_return_data=""
	if [ "$chunk_low_pos" -lt 1 ]; then
		if [ "$((1 - $chunk_low_pos))" -ge $prefix_chars ]; then
			op_return_data="$prefix"
			at_end=true
		fi
		chunk_low_pos=1	
	fi
	if [ "$file_done" = true ]; then
		op_return_data="$prefix$tx"
	else
		op_return_data="$op_return_data$(echo -n $file_body | cut -c $(($chunk_low_pos))-$(($chunk_high_pos)))$tx"
	fi
	if [ "$chunk_low_pos" -eq 1 ]; then
		file_done=true	
	fi

	change_address=$(bitcoin-cli $2 getrawchangeaddress)
	unfinishedtx=$(bitcoin-cli $2 -named createrawtransaction inputs='''[]''' outputs='''{ "data": "'$op_return_data'"}''')
	rawtxhex=$(bitcoin-cli $2 fundrawtransaction $unfinishedtx '{"changePosition": 1}' | jq -r '.hex')
#	rawtxhex=$(curl --silent --user bchrpc:pass --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "fundrawtransaction", "params": ["'$unfinishedtx'"]}' -H 'content-type: text/plain;' http://127.0.0.1:8332/ | jq -r '.result | .hex')
#	bitcoin-cli $2 decoderawtransaction $rawtxhex
	rawsignedtxhex=$(bitcoin-cli $2 signrawtransactionwithwallet $rawtxhex | jq -r '.hex')
#	rawsignedtxhex=$(curl --silent --user bchprc:pass --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "signrawtransactionwithwallet", "params": ["'$rawtxhex'"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/ | jq -r '.result | .hex')
	tx=$(bitcoin-cli $2 sendrawtransaction $rawsignedtxhex | tr -d '\n')
#	tx=$(curl --silent --user bchrpc:pass --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "sendrawtransaction", "params": ["'$rawsignedtxhex'"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/ | jq -r '.result')
#	echo "$op_return_data" >&2
#	echo >&2
	tx_chars=$(echo -n $tx | wc -c)
	chunk_high_pos=$(($chunk_low_pos - 1))
	chunk_low_pos=$(($chunk_low_pos - $chunk_chars + $tx_chars))
	echo -n "." >&2
done
echo >&2
echo "/${1#*/}" >&2
echo -n $tx # tx to stdout
