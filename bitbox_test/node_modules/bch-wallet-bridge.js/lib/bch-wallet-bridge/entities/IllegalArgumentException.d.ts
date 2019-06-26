export default class IllegalArgumentException extends Error {
    message: string;
    name: string;
    constructor(message: string);
}
