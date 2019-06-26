export enum NetworkType {
  MAINNET = "Mainnet",
  TESTNET3 = "Testnet3",
  UNKNOWN = "Unknown"
}

export default class Network {
  /**
   * Network magic bytes
   */
  public magic: number

  /**
   * Network name
   */
  public name: NetworkType

  constructor(magic: number, name: NetworkType) {
    this.magic = magic
    this.name = name
  }
}
