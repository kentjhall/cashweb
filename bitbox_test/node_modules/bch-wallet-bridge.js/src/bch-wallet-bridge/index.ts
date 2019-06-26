import IBCHWalletBridge from "./IBCHWalletBridge"
import Network from "./entities/Network"
import IllegalArgumentException from "./entities/IllegalArgumentException"
import ProviderException from "./entities/ProviderException"
import { findNetwork } from "./networks"
import { isCashAddress, isP2SHAddress, toCashAddress} from "bchaddrjs"
import * as bitcoincashjs from "bitcoincashjs"
import IWalletProvider from "bch-wallet-bridge-provider-interface/lib/IWalletProvider"
import ChangeType from "bch-wallet-bridge-provider-interface/lib/entities/ChangeType"
import Utxo from "bch-wallet-bridge-provider-interface/lib/entities/Utxo"
import Output from "bch-wallet-bridge-provider-interface/lib/entities/Output"

export default class BCHWalletBridge implements IBCHWalletBridge {
  private defaultDAppId?: string

  constructor(public walletProvider?: IWalletProvider) {}

  public getAddress(
    changeType: ChangeType,
    index?: number,
    dAppId?: string
  ): Promise<string> {
    return this.getAddresses(changeType, index, 1, dAppId)
      .then((addresses) => {
        const address = addresses[0]
        if (typeof address !== "string") {
          throw new ProviderException("The return value is invalid.")
        }
        return address
      })
      .catch((e) => { throw new ProviderException(e) })
  }

  public getAddressIndex(
    changeType: ChangeType,
    dAppId?: string
  ): Promise<number> {
    const walletProvider = this.checkWalletProvider()
    return walletProvider.getAddressIndex(changeType, dAppId || this.defaultDAppId)
      .then((index) => {
        if (!Number.isInteger(index) || index < 0 || index > 2147483647) {
          throw new ProviderException("The return value is invalid.")
        }
        return index
      })
      .catch((e) => { throw new ProviderException(e) })
  }

  public getAddresses(
    changeType: ChangeType,
    startIndex?: number,
    size?: number,
    dAppId?: string
  ): Promise<string[]> {
    if (startIndex) {
      if (!Number.isInteger(startIndex) || startIndex < 0 || startIndex > 2147483647) {
        throw new IllegalArgumentException("startIndex is an invalid value.")
      }
    }
    if (size !== undefined) {
      if (!Number.isInteger(size) || size < 1) {
        throw new IllegalArgumentException("size is an invalid value")
      }
    }
    if (startIndex && size) {
      if (startIndex + size > 2147483647) {
        throw new IllegalArgumentException("the max index must be <= 2147483647")
      }
    }
    const walletProvider = this.checkWalletProvider()
    return walletProvider.getAddresses(changeType, size || 1, startIndex, dAppId || this.defaultDAppId)
      .then((addresses) => {
        if (!(addresses instanceof Array) || addresses.length === 0 || typeof addresses[0] !== "string") {
          throw new ProviderException("The return value is invalid.")
        }
        return addresses
      })
      .catch((e) => { throw new ProviderException(e) })
  }

  public async getRedeemScript(
    p2shAddress: string,
    dAppId?: string
  ): Promise<string | undefined> {
    if (!this.isP2SHCashAddress(p2shAddress)) {
      throw new IllegalArgumentException("The address is not P2SH Address or Cash Address.")
    }
    const redeemScripts = await this.getRedeemScripts(dAppId)
    return redeemScripts.find((script) => this.toAddressFromScript(script) === p2shAddress)
  }

  public async getRedeemScripts(
    dAppId?: string
  ): Promise<string[]> {
    const walletProvider = this.checkWalletProvider()
    const redeemScripts = await walletProvider.getRedeemScripts(dAppId || this.defaultDAppId)
      .catch((e) => { throw new ProviderException(e) })
    if (!Array.isArray(redeemScripts) || (redeemScripts.length > 0 && typeof redeemScripts[0] !== "string")) {
      throw new ProviderException("The WalletProvider provides invalid type.")
    }
    return redeemScripts
  }

  public async addRedeemScript(
    redeemScript: string,
    dAppId: string
  ): Promise<void> {
    if (redeemScript.length < 1) {
      throw new IllegalArgumentException("The redeemScript cannot be empty.")
    }

    const walletProvider = this.checkWalletProvider()
    const result = await walletProvider.addRedeemScript(redeemScript, dAppId || this.defaultDAppId)
      .catch((e) => { throw new ProviderException(e) })

    if (typeof result !== "undefined") {
      throw new ProviderException("The provider returns illegal value.")
    }
  }

  public async getUtxos(
    dAppId?: string
  ): Promise<Utxo[]> {
    const walletProvider = this.checkWalletProvider()
    const utxos: Utxo[] = []
    if (dAppId) {
      const unspendableUtxos = await walletProvider.getUnspendableUtxos(dAppId)
        .catch((e) => {
          throw new ProviderException(e)
        })
      const spendableUtxos = await walletProvider.getSpendableUtxos(dAppId)
        .catch((e) => {
          throw new ProviderException(e)
        })
      if (!Array.isArray(unspendableUtxos) || !Array.isArray(spendableUtxos)) {
        throw new ProviderException("The provider returns illegal value.")
      }
      if ((unspendableUtxos.length !== 0 && !(unspendableUtxos[0] instanceof Utxo)) ||
        (spendableUtxos.length !== 0 && !(spendableUtxos[0] instanceof Utxo))) {
        throw new ProviderException("The provider returns illegal value.")
      }
      utxos.push(...unspendableUtxos)
      utxos.push(...spendableUtxos)
    } else {
      const spendableUtxos = await walletProvider.getSpendableUtxos()
        .catch((e) => {
          throw new ProviderException(e)
        })
      if (!Array.isArray(spendableUtxos)) {
        throw new ProviderException("The provider returns illegal value.")
      }
      if (spendableUtxos.length !== 0 && !(spendableUtxos[0] instanceof Utxo)) {
        throw new ProviderException("The provider returns illegal value.")
      }
      utxos.push(...spendableUtxos)
    }
    return utxos
  }

  public async getBalance(
    dAppId?: string
  ): Promise<number> {
    const utxos = await this.getUtxos(dAppId)
    return utxos.reduce((balance, utxo) => balance + utxo.satoshis, 0)
  }

  public async sign(
    address: string,
    dataToSign: string
  ): Promise<string> {
    if (!this.isCashAddress(address)) {
      throw new IllegalArgumentException("The address is not Cash Address format.")
    }

    if (dataToSign.length === 0) {
      throw new IllegalArgumentException("The dataToSign cannot be empty.")
    }

    const walletProvider = this.checkWalletProvider()
    const signature = await walletProvider.sign(address, dataToSign)
      .catch((e) => {
        throw new ProviderException(e)
      })

    if (typeof signature !== "string") {
      throw new ProviderException("The wallet provider provides invalid value.")
    }

    return signature
  }

  public async buildTransaction(
    outputs: Output[],
    dAppId?: string
  ): Promise<string> {
    if (outputs.length === 0) {
      throw new IllegalArgumentException("The outputs cannot be empty.")
    }

    return this.createSignedTx(outputs, dAppId || this.defaultDAppId)
  }

  public async getProtocolVersion(): Promise<number> {
    const walletProvider = this.checkWalletProvider()
    const version = await walletProvider.getProtocolVersion()
      .catch((e) => { throw new ProviderException(e) })

    if (typeof version !== "number") {
      throw new ProviderException(`The wallet provider provides invalid type.`)
    }
    return version
  }

  public async getNetwork(): Promise<Network> {
    const walletProvider = this.checkWalletProvider()

    const magic = await walletProvider.getNetworkMagic()
      .catch((e) => {
        throw new ProviderException(e)
      })
    if (typeof magic !== "number") {
      throw new ProviderException("The wallet provider provides invalid type.")
    }

    return findNetwork(magic)
  }

  public getFeePerByte(): Promise<number> {
    const walletProvider = this.checkWalletProvider()
    return walletProvider.getFeePerByte()
      .then((fee) => {
        if (!Number.isInteger(fee) || fee < 1) {
          throw new ProviderException("The return value is invalid.")
        }
        return fee
      })
      .catch((e) => { throw new ProviderException(e) })
  }

  public getDefaultDAppId(): Promise<string | undefined> {
    return Promise.resolve(this.defaultDAppId)
  }

  public setDefaultDAppId(
    dAppId?: string
  ): Promise<void> {
    return new Promise((resolve) => {
      if (dAppId && !this.isTxHash(dAppId)) {
        throw new IllegalArgumentException("The dAppId is invalid.")
      }
      this.defaultDAppId = dAppId
      resolve()
    })
  }

  private isTxHash(target: string): boolean {
    const re = /[0-9A-Ffa-f]{64}/g
    return re.test(target)
  }

  // TODO: TEMP
  private checkWalletProvider = (): IWalletProvider => {
    if (!this.walletProvider) {
      throw new ProviderException("")
    }
    return this.walletProvider
  }

  private isP2SHCashAddress = (address: string): boolean => {
    try {
      if (!this.isCashAddress(address) || !isP2SHAddress(address)) {
        return false
      }
    } catch (e) {
      return false
    }
    return true
  }

  private isCashAddress = (address: string): boolean => {
    try {
      if (!isCashAddress(address)) {
        return false
      }
    } catch (e) {
      return false
    }
    return true
  }

  private toAddressFromScript = (script: string) => {
    const buf = Buffer.from(script, "hex")
    const hashed = bitcoincashjs.crypto.Hash.sha256ripemd160(buf)
    const legacy = bitcoincashjs.Address.fromScriptHash(hashed).toString()
    return toCashAddress(legacy)
  }

  private async createSignedTx(
    outputs: Output[],
    dAppId?: string
  ): Promise<string> {
    const walletProvider = this.checkWalletProvider()
    const rawtx = await walletProvider.createSignedTx(outputs, dAppId || this.defaultDAppId)
      .catch((e) => { throw new ProviderException(e) })
    if (typeof rawtx !== "string") {
      throw new ProviderException("The return value is invalid.")
    }
    return rawtx
  }
}
