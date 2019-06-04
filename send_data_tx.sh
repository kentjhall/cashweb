#!/bin/bash

if [ "$#" -lt 1 ]; then
	echo "usage: ./send_data_tx.sh <hex-data-1> <hex-data-2> ..."
	exit 1
fi

# constants
dust_amnt_satoshis=545
base_size=10
size_per_in=148
size_per_out=34
data_base_size=10
num_outputs=1
push_data_1="4c"
utxo_limit=10

utxo_lower_bound=0
utxos=$(bitcoin-cli listunspent 0 | jq -c 'reverse | .['$utxo_lower_bound':'$(($utxo_lower_bound+$utxo_limit))']')
dust_amnt=$(bc -l <<< "$dust_amnt_satoshis/100000000")
fee_per_byte=$(bc -l <<< "$(bitcoin-cli estimatefee)/1000")
change_addr=$(bitcoin-cli getrawchangeaddress)
inputs=""
num_inputs=0

data=""
for hex_data in "$@"; do
	data_size=$(($(echo -n $hex_data | wc -c)/2))
	if [ $data_size -gt 255 ]; then echo "send_data_tx.sh does not currently support data >255 bytes, may need revision"; exit 1; fi
	if [ $data_size -gt 75 ]; then data="$data$push_data_1"; fi
	data_size_hex=$(echo "obase=16; ibase=10; $data_size" | bc | tr '[:upper:]' '[:lower:]')
	if [ $(echo -n $data_size_hex | wc -c) -lt 2 ]; then data_size_hex="0$data_size_hex"; fi
	data="$data$data_size_hex$hex_data"
done
data_len=$(echo -n $data | wc -c)
data_out_size=$(($data_len/2))

total_amnt=0
fee=0.00000001 # arbitrary >0
txi=0
change_amnt=0
while [ "$(bc -l <<< "$total_amnt < $fee")" = 1 ] || [ "$(bc -l <<< "$change_amnt <= $dust_amnt")" = 1 ]; do
	if [ $txi -ge $utxo_limit ]; then
		utxo_lower_bound=$(($utxo_lower_bound+$txi))
		utxos=$(bitcoin-cli listunspent 0 | jq -c 'reverse | .['$utxo_lower_bound':'$(($utxo_lower_bound+$utxo_limit))']')
		txi=$(($txi-$utxo_limit)); 
	fi
	cur_utxo=$(echo -n $utxos | jq -c '.['$txi']')
	if [ "$cur_utxo" = "null" ]; then break; fi
	utxo_txid=$(echo -n $cur_utxo | jq -r '.txid')
	utxo_vout=$(echo -n $cur_utxo | jq -r '.vout')
	utxo_amnt=$(echo -n $cur_utxo | jq -r '.amount' | sed -E 's/([+-]?[0-9.]+)[eE]\+?(-?)([0-9]+)/(\1*10^\2\3)/g')
	
	if [ "$inputs" != "" ]; then inputs="$inputs,"; fi
	inputs="$inputs{\"txid\":\"$utxo_txid\",\"vout\":$utxo_vout}"
	num_inputs=$(($num_inputs+1))

	total_amnt=$(bc -l <<< "$total_amnt + $utxo_amnt")

	size_bytes=$(($base_size+($size_per_in*$num_inputs)+($size_per_out*$num_outputs)+$data_base_size+$data_out_size))
	fee=$(bc -l <<< "$fee_per_byte * $size_bytes")
	change_amnt=$(bc -l <<< "$total_amnt - $fee")

	txi=$((txi + 1))
done

if [ "$(bc -l <<< "$total_amnt >= $fee")" = 1 ]; then
	if [ $(echo $change_amnt | head -c 1) = "." ]; then change_amnt="0$change_amnt"; fi
	if [ "$(bc -l <<< "$change_amnt > $dust_amnt")" = 1 ]; then
		rawtx=$(bitcoin-cli -named createrawtransaction inputs='''['$inputs']''' outputs='''{"data": "'$data'", "'$change_addr'": '$change_amnt'}''')
	else
		rawtx=$(bitcoin-cli -named createrawtransaction inputs='''['$inputs']''' outputs='''{"data": "'$data'"}''')
	fi

	data_out_size_hex=$(echo "obase=16; ibase=10; $(($data_out_size+1))" | bc | tr '[:upper:]' '[:lower:]')
	if [ $(echo -n $data_out_size_hex | wc -c) -lt 2 ]; then data_out_size_hex="0$data_out_size_hex"; fi
	for (( i=0; i<${#rawtx}; i+=2 )) do
		if [ "${rawtx:$i:1}" = "6" ] && [ "${rawtx:$(($i+1)):1}" = "a" ]; then
			rawtx="${rawtx:0:$(($i-2))}$data_out_size_hex${rawtx:$i}"

			data_pos=$(($i+2))
			while [ "$(echo -n $rawtx | cut -c $(($data_pos+1))-$(($data_pos+$data_len)))" != "$data" ]; do
				rawtx="${rawtx:0:$data_pos}${rawtx:$(($data_pos+2))}"
			done
			break
		fi
	done	
	signedtx=$(bitcoin-cli signrawtransactionwithwallet $rawtx | jq -r '.hex')
	echo -n $(bitcoin-cli sendrawtransaction $signedtx 2>&1 | tr -d '\n')
else
	echo "Insufficient funds"
fi
