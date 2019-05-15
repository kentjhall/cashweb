#!/bin/bash

if [ "$#" -ne 1 ]; then
	echo "usage: ./send_data_tx.sh <op-return-data>"
	exit 1
fi

change_address=$(bitcoin-cli getrawchangeaddress)
unfinishedtx=$(bitcoin-cli -named createrawtransaction inputs='''[]''' outputs='''{ "data": "'$1'"}''')
rawtxhex=$(bitcoin-cli fundrawtransaction $unfinishedtx '{"changePosition": 1}' | jq -r '.hex' 2>&1)
if [[ $? != 0 ]]; then exit $?; fi
rawsignedtxhex=$(bitcoin-cli signrawtransactionwithwallet $rawtxhex | jq -r '.hex' 2>&1)
if [ $? -eq 0 ]; then 
	echo -n $(bitcoin-cli sendrawtransaction $rawsignedtxhex | tr -d '\n' 2>&1)
else
	echo "problem"
fi
