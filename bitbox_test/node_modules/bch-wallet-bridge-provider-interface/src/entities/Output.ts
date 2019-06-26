export default class Output {
  /**
   * The hex format of lock script.
   */
  public lockScript: string

  /**
   * The value transferred to the lock script in satoshi.
   */
  public amount: number

  constructor(lockScript: string, amount: number) {
    this.lockScript = lockScript
    this.amount = amount
  }
}
