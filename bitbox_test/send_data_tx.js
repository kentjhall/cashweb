#!/usr/bin/env node

const args = process.argv.slice(2);

const TransactionBuilder = require("bitbox-sdk").TransactionBuilder
let transactionBuilder = new TransactionBuilder('mainnet')

console.log(args);
