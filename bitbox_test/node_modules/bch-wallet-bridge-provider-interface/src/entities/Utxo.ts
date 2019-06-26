export default class Utxo {
  /**
   * Txid of the utxo
   */
  public txid: string

  /**
   * Txout index number of the utxo
   */
  public outputIndex: number

  /**
   * Address
   */
  public address: string

  /**
   * ScriptPubKey
   */
  public script: string

  /**
   * Satoshis
   */
  public satoshis: number

  constructor(txid: string, outputIndex: number, address: string, script: string, satoshis: number) {
    this.txid = txid
    this.outputIndex = outputIndex
    this.address = address
    this.script = script
    this.satoshis = satoshis
  }
}
