export default class ProviderException extends Error {
    message: string;
    name: string;
    constructor(message: string);
}
