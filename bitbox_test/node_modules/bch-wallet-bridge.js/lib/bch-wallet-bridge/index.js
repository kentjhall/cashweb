"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const IllegalArgumentException_1 = require("./entities/IllegalArgumentException");
const ProviderException_1 = require("./entities/ProviderException");
const networks_1 = require("./networks");
const bchaddrjs_1 = require("bchaddrjs");
const bitcoincashjs = require("bitcoincashjs");
const Utxo_1 = require("bch-wallet-bridge-provider-interface/lib/entities/Utxo");
class BCHWalletBridge {
    constructor(walletProvider) {
        this.walletProvider = walletProvider;
        // TODO: TEMP
        this.checkWalletProvider = () => {
            if (!this.walletProvider) {
                throw new ProviderException_1.default("");
            }
            return this.walletProvider;
        };
        this.isP2SHCashAddress = (address) => {
            try {
                if (!this.isCashAddress(address) || !bchaddrjs_1.isP2SHAddress(address)) {
                    return false;
                }
            }
            catch (e) {
                return false;
            }
            return true;
        };
        this.isCashAddress = (address) => {
            try {
                if (!bchaddrjs_1.isCashAddress(address)) {
                    return false;
                }
            }
            catch (e) {
                return false;
            }
            return true;
        };
        this.toAddressFromScript = (script) => {
            const buf = Buffer.from(script, "hex");
            const hashed = bitcoincashjs.crypto.Hash.sha256ripemd160(buf);
            const legacy = bitcoincashjs.Address.fromScriptHash(hashed).toString();
            return bchaddrjs_1.toCashAddress(legacy);
        };
    }
    getAddress(changeType, index, dAppId) {
        return this.getAddresses(changeType, index, 1, dAppId)
            .then((addresses) => {
            const address = addresses[0];
            if (typeof address !== "string") {
                throw new ProviderException_1.default("The return value is invalid.");
            }
            return address;
        })
            .catch((e) => { throw new ProviderException_1.default(e); });
    }
    getAddressIndex(changeType, dAppId) {
        const walletProvider = this.checkWalletProvider();
        return walletProvider.getAddressIndex(changeType, dAppId || this.defaultDAppId)
            .then((index) => {
            if (!Number.isInteger(index) || index < 0 || index > 2147483647) {
                throw new ProviderException_1.default("The return value is invalid.");
            }
            return index;
        })
            .catch((e) => { throw new ProviderException_1.default(e); });
    }
    getAddresses(changeType, startIndex, size, dAppId) {
        if (startIndex) {
            if (!Number.isInteger(startIndex) || startIndex < 0 || startIndex > 2147483647) {
                throw new IllegalArgumentException_1.default("startIndex is an invalid value.");
            }
        }
        if (size !== undefined) {
            if (!Number.isInteger(size) || size < 1) {
                throw new IllegalArgumentException_1.default("size is an invalid value");
            }
        }
        if (startIndex && size) {
            if (startIndex + size > 2147483647) {
                throw new IllegalArgumentException_1.default("the max index must be <= 2147483647");
            }
        }
        const walletProvider = this.checkWalletProvider();
        return walletProvider.getAddresses(changeType, size || 1, startIndex, dAppId || this.defaultDAppId)
            .then((addresses) => {
            if (!(addresses instanceof Array) || addresses.length === 0 || typeof addresses[0] !== "string") {
                throw new ProviderException_1.default("The return value is invalid.");
            }
            return addresses;
        })
            .catch((e) => { throw new ProviderException_1.default(e); });
    }
    async getRedeemScript(p2shAddress, dAppId) {
        if (!this.isP2SHCashAddress(p2shAddress)) {
            throw new IllegalArgumentException_1.default("The address is not P2SH Address or Cash Address.");
        }
        const redeemScripts = await this.getRedeemScripts(dAppId);
        return redeemScripts.find((script) => this.toAddressFromScript(script) === p2shAddress);
    }
    async getRedeemScripts(dAppId) {
        const walletProvider = this.checkWalletProvider();
        const redeemScripts = await walletProvider.getRedeemScripts(dAppId || this.defaultDAppId)
            .catch((e) => { throw new ProviderException_1.default(e); });
        if (!Array.isArray(redeemScripts) || (redeemScripts.length > 0 && typeof redeemScripts[0] !== "string")) {
            throw new ProviderException_1.default("The WalletProvider provides invalid type.");
        }
        return redeemScripts;
    }
    async addRedeemScript(redeemScript, dAppId) {
        if (redeemScript.length < 1) {
            throw new IllegalArgumentException_1.default("The redeemScript cannot be empty.");
        }
        const walletProvider = this.checkWalletProvider();
        const result = await walletProvider.addRedeemScript(redeemScript, dAppId || this.defaultDAppId)
            .catch((e) => { throw new ProviderException_1.default(e); });
        if (typeof result !== "undefined") {
            throw new ProviderException_1.default("The provider returns illegal value.");
        }
    }
    async getUtxos(dAppId) {
        const walletProvider = this.checkWalletProvider();
        const utxos = [];
        if (dAppId) {
            const unspendableUtxos = await walletProvider.getUnspendableUtxos(dAppId)
                .catch((e) => {
                throw new ProviderException_1.default(e);
            });
            const spendableUtxos = await walletProvider.getSpendableUtxos(dAppId)
                .catch((e) => {
                throw new ProviderException_1.default(e);
            });
            if (!Array.isArray(unspendableUtxos) || !Array.isArray(spendableUtxos)) {
                throw new ProviderException_1.default("The provider returns illegal value.");
            }
            if ((unspendableUtxos.length !== 0 && !(unspendableUtxos[0] instanceof Utxo_1.default)) ||
                (spendableUtxos.length !== 0 && !(spendableUtxos[0] instanceof Utxo_1.default))) {
                throw new ProviderException_1.default("The provider returns illegal value.");
            }
            utxos.push(...unspendableUtxos);
            utxos.push(...spendableUtxos);
        }
        else {
            const spendableUtxos = await walletProvider.getSpendableUtxos()
                .catch((e) => {
                throw new ProviderException_1.default(e);
            });
            if (!Array.isArray(spendableUtxos)) {
                throw new ProviderException_1.default("The provider returns illegal value.");
            }
            if (spendableUtxos.length !== 0 && !(spendableUtxos[0] instanceof Utxo_1.default)) {
                throw new ProviderException_1.default("The provider returns illegal value.");
            }
            utxos.push(...spendableUtxos);
        }
        return utxos;
    }
    async getBalance(dAppId) {
        const utxos = await this.getUtxos(dAppId);
        return utxos.reduce((balance, utxo) => balance + utxo.satoshis, 0);
    }
    async sign(address, dataToSign) {
        if (!this.isCashAddress(address)) {
            throw new IllegalArgumentException_1.default("The address is not Cash Address format.");
        }
        if (dataToSign.length === 0) {
            throw new IllegalArgumentException_1.default("The dataToSign cannot be empty.");
        }
        const walletProvider = this.checkWalletProvider();
        const signature = await walletProvider.sign(address, dataToSign)
            .catch((e) => {
            throw new ProviderException_1.default(e);
        });
        if (typeof signature !== "string") {
            throw new ProviderException_1.default("The wallet provider provides invalid value.");
        }
        return signature;
    }
    async buildTransaction(outputs, dAppId) {
        if (outputs.length === 0) {
            throw new IllegalArgumentException_1.default("The outputs cannot be empty.");
        }
        return this.createSignedTx(outputs, dAppId || this.defaultDAppId);
    }
    async getProtocolVersion() {
        const walletProvider = this.checkWalletProvider();
        const version = await walletProvider.getProtocolVersion()
            .catch((e) => { throw new ProviderException_1.default(e); });
        if (typeof version !== "number") {
            throw new ProviderException_1.default(`The wallet provider provides invalid type.`);
        }
        return version;
    }
    async getNetwork() {
        const walletProvider = this.checkWalletProvider();
        const magic = await walletProvider.getNetworkMagic()
            .catch((e) => {
            throw new ProviderException_1.default(e);
        });
        if (typeof magic !== "number") {
            throw new ProviderException_1.default("The wallet provider provides invalid type.");
        }
        return networks_1.findNetwork(magic);
    }
    getFeePerByte() {
        const walletProvider = this.checkWalletProvider();
        return walletProvider.getFeePerByte()
            .then((fee) => {
            if (!Number.isInteger(fee) || fee < 1) {
                throw new ProviderException_1.default("The return value is invalid.");
            }
            return fee;
        })
            .catch((e) => { throw new ProviderException_1.default(e); });
    }
    getDefaultDAppId() {
        return Promise.resolve(this.defaultDAppId);
    }
    setDefaultDAppId(dAppId) {
        return new Promise((resolve) => {
            if (dAppId && !this.isTxHash(dAppId)) {
                throw new IllegalArgumentException_1.default("The dAppId is invalid.");
            }
            this.defaultDAppId = dAppId;
            resolve();
        });
    }
    isTxHash(target) {
        const re = /[0-9A-Ffa-f]{64}/g;
        return re.test(target);
    }
    async createSignedTx(outputs, dAppId) {
        const walletProvider = this.checkWalletProvider();
        const rawtx = await walletProvider.createSignedTx(outputs, dAppId || this.defaultDAppId)
            .catch((e) => { throw new ProviderException_1.default(e); });
        if (typeof rawtx !== "string") {
            throw new ProviderException_1.default("The return value is invalid.");
        }
        return rawtx;
    }
}
exports.default = BCHWalletBridge;
