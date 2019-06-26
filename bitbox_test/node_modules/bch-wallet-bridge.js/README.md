# bch-wallet-bridge.js - Bridge between Bitcoin Cash application and wallet
[![Build Status](https://travis-ci.org/web3bch/bch-wallet-bridge.js.svg?branch=master)](https://travis-ci.org/web3bch/bch-wallet-bridge.js)
[![codecov](https://codecov.io/gh/web3bch/bch-wallet-bridge.js/branch/master/graph/badge.svg)](https://codecov.io/gh/web3bch/bch-wallet-bridge.js)

## About
Bitcoin Cash applications don't have to be castodial wallet anymore.
With `bch-wallet-bridge.js`, they can request flexible actions to their users' wallet.

## Installation
`yarn add bch-wallet-bridge`

## Usage
```ts
import BCHWalletBridge from "bch-wallet-bridge"
const injected = window.bitcoincash
if (!injected || !injected.wallet) {
  console.log("BCHWalletBridge wallet isn't injected!")
  return
}
const bchWalletBridge = new BCHWalletBridge(injected.wallet)
```

## What is DApp ID?
DApp ID is a unique identifiers for a single DApp, and it's a txid of Bitcoin transaction.
Each DApp writes its protocol specification in the tranasction's OP_RETURN output.

It is defined in [BDIP-2](https://github.com/web3bch/BDIPs/blob/master/BDIPs/bdip-2.md).

## Documentation

Documentation can be found at [GitHub Pages][docs].

[docs]: https://web3bch.github.io/bch-wallet-bridge.js/

## Building
### Requirements
- Node.js
- npm
- yarn

### Build (tsc)
1. `$ yarn`
2. `$ yarn build`