export default class Destination {
  /**
   * The destination address.
   */
  public address: string

  /**
   * The value transferred to the destination address in satoshi.
   */
  public amount: number

  constructor(address: string, amount: number) {
    this.address = address
    this.amount = amount
  }
}
