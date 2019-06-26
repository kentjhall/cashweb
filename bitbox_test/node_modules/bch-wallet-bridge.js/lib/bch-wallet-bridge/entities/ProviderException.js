"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
class ProviderException extends Error {
    constructor(message) {
        super(message);
        this.message = message;
        this.name = "ProviderException";
    }
}
exports.default = ProviderException;
