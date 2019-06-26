export default class Destination {
    /**
     * The destination address.
     */
    address: string;
    /**
     * The value transferred to the destination address in satoshi.
     */
    amount: number;
    constructor(address: string, amount: number);
}
