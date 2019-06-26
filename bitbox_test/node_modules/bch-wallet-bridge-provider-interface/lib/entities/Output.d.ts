export default class Output {
    /**
     * The hex format of lock script.
     */
    lockScript: string;
    /**
     * The value transferred to the lock script in satoshi.
     */
    amount: number;
    constructor(lockScript: string, amount: number);
}
