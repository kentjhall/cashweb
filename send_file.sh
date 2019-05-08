#!/bin/bash

if [ "$#" -lt 2 ]; then
	echo "usage: ./send_file.sh <file-path> <fee-satoshis-per-chunk> [bitcoin-cli flags]"
	exit 1
fi

prefix=$(echo -n "cw" | xxd -p | tr -d '\n')
prefix_chars=$(echo -n $prefix | wc -c)
file_body=$(cat $1 | xxd -p | tr -d '\n')
file_chars=$(echo -n $file_body | wc -c)
chunk_bytes=219
chunk_chars=$(($chunk_bytes*2))
first_done=false
file_done=false
at_end=false

tx=""
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

	fee=$(bc -l <<< "$2/100000000")
	balance=$(bitcoin-cli $3 getbalance)	
	printed=false
	while [ "$(bc -l <<< "$fee > $(bitcoin-cli -regtest getbalance)")" = 1 ]; do
		if [ "$printed" = false ]; then
			address=$(bitcoin-cli $3 getnewaddress)
			echo "Insufficient balance; to continue, send to: $address" >&2
			printed=true
		fi
	done
	utxo_amnt="null"
	printed1=false
	printed2=false
	while [ "$utxo_amnt" = "null" ]; do
		num_txs=$(bitcoin-cli $3 listunspent | jq -r '. | length')
		txi=$(($num_txs - 1))
		while [ "$txi" -ge 0 ]; do
			utxo_amnt=$(bitcoin-cli $3 listunspent | jq -r ".[$txi] | .amount" | sed -E 's/([+-]?[0-9.]+)[eE]\+?(-?)([0-9]+)/(\1*10^\2\3)/g')
			if [ "$(bc -l <<< "$utxo_amnt > $fee")" = 1 -a "$(bitcoin-cli $3 listunspent | jq -r ".[$txi] | .safe")" = "true" ]; then
				break
			fi
			txi=$(($txi - 1))
		done
		if [ "$utxo_amnt" = "null" ]; then
			num_txs_all=$(bitcoin-cli $3 listunspent 0 | jq -r '. | length')
			if [ "$(bc -l <<< "$num_txs < $num_txs_all")" = 1 ]; then
				if [ "$printed1" = false ]; then
					echo "Waiting for confirmations..." >&2
					printed1=true
				fi
			elif [ "$printed2" = false ]; then
				address=$(bitcoin-cli $3 getnewaddress)
				echo "No single tx has enough funds; to continue, try sending more: $address" >&2
				printed2=true
			fi
		fi
	done
	utxo_txid=$(bitcoin-cli $3 listunspent | jq -r ".[$txi] | .txid")
	utxo_vout=$(bitcoin-cli $3 listunspent | jq -r ".[$txi] | .vout")
	change_address=$(bitcoin-cli $3 getrawchangeaddress)
	amount_to_keep=$(bc -l <<< "$utxo_amnt - $fee")
	if [ "$(echo "${amount_to_keep:0:1}")" = "." ]; then
		amount_to_keep="0$amount_to_keep" # bc won't prepend decimal with zero,
						  # for values less than 1
	fi
	unfinishedtx=$(bitcoin-cli $3 -named createrawtransaction inputs='''[]''' outputs='''{ "data": "'$op_return_data'", "'$change_address'": 0.00001 }''')
	rawtxhex=$(bitcoin-cli $3 fundrawtransaction $unfinishedtx | jq -r '.hex')
#	bitcoin-cli $3 decoderawtransaction $rawtxhex
	rawsignedtxhex=$(bitcoin-cli $3 signrawtransactionwithwallet $rawtxhex | jq -r '.hex')
	tx=$(bitcoin-cli $3 sendrawtransaction $rawsignedtxhex | tr -d '\n')
#	echo "$op_return_data" >&2
#	echo >&2
	tx_chars=$(echo -n $tx | wc -c)
	chunk_high_pos=$(($chunk_low_pos - 1))
	chunk_low_pos=$(($chunk_low_pos - $chunk_chars + $tx_chars))

done
echo "/${1#*/}" >&2
echo -n $tx # starting tx to stdout for piping
