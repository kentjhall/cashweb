"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
// consts
var Bitcoin = require("bitcoincashjs-lib");
var opcodes = require("bitcoincash-ops");
var Script = /** @class */ (function () {
    function Script() {
        this.opcodes = opcodes;
        this.nullData = Bitcoin.script.nullData;
        this.multisig = {
            input: {
                encode: function (signatures) {
                    var sigs = [];
                    signatures.forEach(function (sig) {
                        sigs.push(sig);
                    });
                    return Bitcoin.script.multisig.input.encode(sigs);
                },
                decode: Bitcoin.script.multisig.input.decode,
                check: Bitcoin.script.multisig.input.check
            },
            output: {
                encode: function (m, pubKeys) {
                    var pks = [];
                    pubKeys.forEach(function (pubKey) {
                        pks.push(pubKey);
                    });
                    return Bitcoin.script.multisig.output.encode(m, pks);
                },
                decode: Bitcoin.script.multisig.output.decode,
                check: Bitcoin.script.multisig.output.check
            }
        };
        this.pubKey = Bitcoin.script.pubKey;
        this.pubKeyHash = Bitcoin.script.pubKeyHash;
        this.scriptHash = Bitcoin.script.scriptHash;
        this.number = Bitcoin.script.number;
    }
    Script.prototype.encode = function (scriptChunks) {
        var arr = [];
        scriptChunks.forEach(function (chunk) {
            arr.push(chunk);
        });
        return Bitcoin.script.compile(arr);
    };
    Script.prototype.decode = function (scriptBuffer) {
        return Bitcoin.script.decompile(scriptBuffer);
    };
    Script.prototype.toASM = function (buffer) {
        return Bitcoin.script.toASM(buffer);
    };
    Script.prototype.fromASM = function (asm) {
        return Bitcoin.script.fromASM(asm);
    };
    Script.prototype.classifyInput = function (script) {
        return Bitcoin.script.classifyInput(script);
    };
    Script.prototype.classifyOutput = function (script) {
        return Bitcoin.script.classifyOutput(script);
    };
    Script.prototype.encodeNullDataOutput = function (data) {
        return this.nullData.output.encode(data);
    };
    Script.prototype.decodeNullDataOutput = function (output) {
        return this.nullData.output.decode(output);
    };
    Script.prototype.checkNullDataOutput = function (output) {
        return this.nullData.output.check(output);
    };
    Script.prototype.encodeP2PKInput = function (signature) {
        return this.pubKey.input.encode(signature);
    };
    Script.prototype.decodeP2PKInput = function (input) {
        return this.pubKey.input.decode(input);
    };
    Script.prototype.checkP2PKInput = function (input) {
        return this.pubKey.input.check(input);
    };
    Script.prototype.encodeP2PKOutput = function (pubKey) {
        return this.pubKey.output.encode(pubKey);
    };
    Script.prototype.decodeP2PKOutput = function (output) {
        return this.pubKey.output.decode(output);
    };
    Script.prototype.checkP2PKOutput = function (output) {
        return this.pubKey.output.check(output);
    };
    Script.prototype.encodeP2PKHInput = function (signature, pubKey) {
        return this.pubKeyHash.input.encode(signature, pubKey);
    };
    Script.prototype.decodeP2PKHInput = function (input) {
        return this.pubKeyHash.input.decode(input);
    };
    Script.prototype.checkP2PKHInput = function (input) {
        return this.pubKeyHash.input.check(input);
    };
    Script.prototype.encodeP2PKHOutput = function (identifier) {
        return this.pubKeyHash.output.encode(identifier);
    };
    Script.prototype.decodeP2PKHOutput = function (output) {
        return this.pubKeyHash.output.decode(output);
    };
    Script.prototype.checkP2PKHOutput = function (output) {
        return this.pubKeyHash.output.check(output);
    };
    Script.prototype.encodeP2MSInput = function (signatures) {
        return this.multisig.input.encode(signatures);
    };
    Script.prototype.decodeP2MSInput = function (input) {
        return this.multisig.input.decode(input);
    };
    Script.prototype.checkP2MSInput = function (input) {
        return this.multisig.input.check(input);
    };
    Script.prototype.encodeP2MSOutput = function (m, pubKeys) {
        return this.multisig.output.encode(m, pubKeys);
    };
    Script.prototype.decodeP2MSOutput = function (output) {
        return this.multisig.output.decode(output);
    };
    Script.prototype.checkP2MSOutput = function (output) {
        return this.multisig.output.check(output);
    };
    Script.prototype.encodeP2SHInput = function (redeemScriptSig, redeemScript) {
        return this.scriptHash.input.encode(redeemScriptSig, redeemScript);
    };
    Script.prototype.decodeP2SHInput = function (input) {
        return this.scriptHash.input.decode(input);
    };
    Script.prototype.checkP2SHInput = function (input) {
        return this.scriptHash.input.check(input);
    };
    Script.prototype.encodeP2SHOutput = function (scriptHash) {
        return this.scriptHash.output.encode(scriptHash);
    };
    Script.prototype.decodeP2SHOutput = function (output) {
        return this.scriptHash.output.decode(output);
    };
    Script.prototype.checkP2SHOutput = function (output) {
        return this.scriptHash.output.check(output);
    };
    Script.prototype.encodeNumber = function (number) {
        return this.number.encode(number);
    };
    Script.prototype.decodeNumber = function (buffer, maxLength, minimal) {
        return this.number.decode(buffer, maxLength, minimal);
    };
    return Script;
}());
exports.Script = Script;
//# sourceMappingURL=Script.js.map