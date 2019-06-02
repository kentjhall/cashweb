#!/bin/bash

if [ "$#" -ne 1 ]; then
	echo "usage: ./fragment_utxos.sh <frag-amnt-satoshis>"
	exit 1
fi

# constants
dust_amnt_satoshis=600
max_outputs=2000
base_size_bytes=158
out_size_bytes=34

max_amnt=$(bc -l <<< "$1/100000000")
dust_amnt=$(bc -l <<< "$dust_amnt_satoshis/100000000")
fee_per_byte=$(bc -l <<< "$(bitcoin-cli estimatefee)/1000")
balance_start=$(bc -l <<< "$(bitcoin-cli getbalance) + $(bitcoin-cli getunconfirmedbalance)")

if [ $(bitcoin-cli listunspent 0 | jq -r 'length') -gt 0 ]; then
	for (( ; ; )) do
		biggest_utxo=$(bitcoin-cli listunspent 0 | jq -c 'sort_by(-.amount) | .[0]')
		utxo_amnt=$(echo -n $biggest_utxo | jq -r ".amount" | sed -E 's/([+-]?[0-9.]+)[eE]\+?(-?)([0-9]+)/(\1*10^\2\3)/g')
		if [ "$(bc -l <<< "$utxo_amnt <= $max_amnt")" = 1 ]; then break; fi
		echo -n "Fragmenting..." 1>&2
		utxo_txid=$(echo -n $biggest_utxo | jq -r ".txid")
		utxo_vout=$(echo -n $biggest_utxo | jq -r ".vout")
		outputs=""
		total_outputs=$(bc <<< "if ($utxo_amnt%$max_amnt) $utxo_amnt/$max_amnt+1 else $utxo_amnt/$max_amnt") # ceiling
		if [ "$(bc <<< "$total_outputs <= $max_outputs")" = 1 ]; then num_outputs=$total_outputs; else num_outputs=$max_outputs; fi
		size_bytes=$(bc -l <<< "$base_size_bytes+($out_size_bytes*$num_outputs)+1")
		fee=$(bc -l <<< "$fee_per_byte * $size_bytes")
		utxo_amnt=$(bc -l <<< "$utxo_amnt - $fee")
		count=0
		while [ "$(bc -l <<< "$utxo_amnt > 0")" = 1 ]; do
			if [ "$(bc -l <<< "$utxo_amnt > $max_amnt")" = 1 ]; then send_amnt=$max_amnt; else send_amnt=$utxo_amnt; fi
			if [ $count -ge $(($num_outputs - 1)) ]; then send_amnt=$utxo_amnt; fi
			if [ $(echo $send_amnt | head -c 1) = "." ]; then send_amnt="0$send_amnt"; fi
			if [ "$(bc -l <<< "$send_amnt > $dust_amnt")" = 1 ]; then					
				if [ "$outputs" != "" ]; then outputs="$outputs,"; fi
				outputs="$outputs\"$(bitcoin-cli getrawchangeaddress)\":$send_amnt"
			fi
			utxo_amnt=$(bc -l <<< "$utxo_amnt - $send_amnt")
			count=$(($count + 1))
			echo -n "." 1>&2
		done
		rawtx=$(bitcoin-cli createrawtransaction '''[ { "txid": "'$utxo_txid'", "vout": '$utxo_vout' } ]''' '''{ '$outputs' }''')
		signedtx=$(bitcoin-cli signrawtransactionwithwallet $rawtx | jq -r '.hex')
		bitcoin-cli sendrawtransaction $signedtx > /dev/null
		echo "" 1>&2
	done
fi
balance_end=$(bc -l <<< "$(bitcoin-cli getbalance) + $(bitcoin-cli getunconfirmedbalance)")
balance_diff=$(bc -l <<< "$balance_start - $balance_end")
if [ "$(bc -l <<< "$balance_diff == 0")" = 1 ]; then echo "Nothing to fragment." 2>&1; fi
echo -n "Fragmentation cost: " 2>&1
echo $balance_diff # cost to stdout
