#!/bin/bash

if [ "$#" -ne 1 ]; then
	echo "usage: ./send_data_tx.sh <op-return-data>"
	exit 1
fi

change_address=$(bitcoin-cli getrawchangeaddress)
unfinishedtx=$(bitcoin-cli -named createrawtransaction inputs='''[]''' outputs='''{ "data": "'$1'"}''')
rawtxhex=$(bitcoin-cli fundrawtransaction $unfinishedtx '{"changePosition": 1}' | jq -r '.hex')
rawsignedtxhex=$(bitcoin-cli signrawtransactionwithwallet $rawtxhex | jq -r '.hex')
echo -n $(bitcoin-cli sendrawtransaction $rawsignedtxhex | tr -d '\n')
