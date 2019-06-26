"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var Address_1 = require("./Address");
var Bitcoin = require("bitcoincashjs-lib");
var coininfo = require("coininfo");
var ECPair = /** @class */ (function () {
    function ECPair(address) {
        if (address === void 0) { address = new Address_1.Address(); }
        this._address = address;
    }
    ECPair.prototype.fromWIF = function (privateKeyWIF) {
        var network = "mainnet";
        if (privateKeyWIF[0] === "L" || privateKeyWIF[0] === "K")
            network = "mainnet";
        else if (privateKeyWIF[0] === "c")
            network = "testnet";
        var bitcoincash;
        if (network === "mainnet")
            bitcoincash = coininfo.bitcoincash.main;
        else
            bitcoincash = coininfo.bitcoincash.test;
        var bitcoincashBitcoinJSLib = bitcoincash.toBitcoinJS();
        return Bitcoin.ECPair.fromWIF(privateKeyWIF, bitcoincashBitcoinJSLib);
    };
    ECPair.prototype.toWIF = function (ecpair) {
        return ecpair.toWIF();
    };
    ECPair.prototype.sign = function (ecpair, buffer, signatureAlgorithm) {
        return ecpair.sign(buffer, signatureAlgorithm);
    };
    ECPair.prototype.verify = function (ecpair, buffer, signature) {
        return ecpair.verify(buffer, signature);
    };
    ECPair.prototype.fromPublicKey = function (pubkeyBuffer) {
        return Bitcoin.ECPair.fromPublicKeyBuffer(pubkeyBuffer);
    };
    ECPair.prototype.toPublicKey = function (ecpair) {
        return ecpair.getPublicKeyBuffer();
    };
    ECPair.prototype.toLegacyAddress = function (ecpair) {
        return ecpair.getAddress();
    };
    ECPair.prototype.toCashAddress = function (ecpair, regtest) {
        if (regtest === void 0) { regtest = false; }
        return this._address.toCashAddress(ecpair.getAddress(), true, regtest);
    };
    return ECPair;
}());
exports.ECPair = ECPair;
//# sourceMappingURL=ECPair.js.map