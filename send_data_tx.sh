#!/bin/bash

if [ "$#" -ne 1 ]; then
	echo "usage: ./send_data_tx.sh <op-return-data>"
	exit 1
fi

utxos_sorted=$(bitcoin-cli listunspent 0 | jq -c 'sort_by(-.confirmations) | .[]')

change_addr=$(bitcoin-cli getrawchangeaddress)
inputs=

total_amnt="0"
fee="0.00000001"
txi=0
while [ "$(bc -l <<< "$total_amnt < $fee")" = 1 ] && ; do
	unfinishedtx=$(bitcoin-cli -named createrawtransaction inputs='''[]''' outputs='''{"data": "'$1'", "'$change_addr'"}''')
done

rawtxhex=$(bitcoin-cli fundrawtransaction $unfinishedtx '{"changePosition": 1}' | jq -r '.hex' 2>&1)
if [[ $rawtxhex == *" "* ]]; then echo -n $rawtxhex; exit 1; fi
rawsignedtxhex=$(bitcoin-cli signrawtransactionwithwallet $rawtxhex | jq -r '.hex')
bitcoin-cli sendrawtransaction $rawsignedtxhex 2>&1
