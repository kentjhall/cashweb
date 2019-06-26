"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const Network_1 = require("./entities/Network");
const Network_2 = require("./entities/Network");
const networks = new Map([
    [0xE3E1F3E8, Network_2.NetworkType.MAINNET],
    [0x0B110907, Network_2.NetworkType.TESTNET3]
]);
exports.findNetwork = (magic) => {
    const type = networks.get(magic);
    if (type) {
        return new Network_1.default(magic, type);
    }
    return new Network_1.default(magic, Network_2.NetworkType.UNKNOWN);
};
