#!/bin/bash

if [ "$#" -ne 1 ]; then
	echo "usage: ./send_data_tx.sh <op-return-data>"
	exit 1
fi

extrachangeamnt=0.00001
if [ $(echo $extrachangeamnt | head -c 1) = "." ]; then
	extrachangeamnt="0$extrachangeamnt"
fi
unfinishedtx=$(bitcoin-cli -named createrawtransaction inputs='''[]''' outputs='''{"data": "'$1'","'$(bitcoin-cli getrawchangeaddress)'":'$extrachangeamnt', "'$(bitcoin-cli getrawchangeaddress)'":'$extrachangeamnt', "'$(bitcoin-cli getrawchangeaddress)'":'$extrachangeamnt', "'$(bitcoin-cli getrawchangeaddress)'":'$extrachangeamnt', "'$(bitcoin-cli getrawchangeaddress)'":'$extrachangeamnt'}''')
rawtxhex=$(bitcoin-cli fundrawtransaction $unfinishedtx '{"changePosition": 2}' | jq -r '.hex')
rawsignedtxhex=$(bitcoin-cli signrawtransactionwithwallet $rawtxhex | jq -r '.hex')
echo -n $(bitcoin-cli sendrawtransaction $rawsignedtxhex 2>&1)
