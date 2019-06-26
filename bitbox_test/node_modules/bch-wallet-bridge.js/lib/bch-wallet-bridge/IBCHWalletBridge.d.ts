import Network from "./entities/Network";
import IWalletProvider from "bch-wallet-bridge-provider-interface/lib/IWalletProvider";
import ChangeType from "bch-wallet-bridge-provider-interface/lib/entities/ChangeType";
import Output from "bch-wallet-bridge-provider-interface/lib/entities/Output";
import Utxo from "bch-wallet-bridge-provider-interface/lib/entities/Utxo";
export default interface IBCHWalletBridge {
    /**
     * The current provider set.
     */
    walletProvider?: IWalletProvider;
    /**
     * Returns the current wallet address.
     * @example
     * const address = await bchWalletBridge.getAddress(
     *   "receive",
     *   3,
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(address)
     * > "bitcoincash:qrsy0xwugcajsqa99c9nf05pz7ndckj55ctlsztu2p"
     * @param changeType The BIP44 change path type.
     * @param index The BIP44 address_index path.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The current wallet address.
     */
    getAddress(changeType: ChangeType, index?: number, dAppId?: string): Promise<string>;
    /**
     * Returns the current wallet address index.
     * @example
     * const addrIdx = await bchWalletBridge.getAddressIndex(
     *   "change",
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(addrIdx)
     * > 3
     * @param changeType The BIP44 change path type.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The current wallet address index.
     */
    getAddressIndex(changeType: ChangeType, dAppId?: string): Promise<number>;
    /**
     * Returns the wallet address list.
     * @example
     * const addresses = await bchWalletBridge.getAddresses(
     *   "receive",
     *   3,
     *   2,
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(addresses)
     * > ["bitcoincash:qrsy0xwugcajsqa...", "bitcoincash:qrsfpepw3egqq4k..."]
     * @param changeType The BIP44 change path type.
     * @param startIndex The BIP44 address_index path.
     * @param size The address amount you want.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The wallet address list.
     */
    getAddresses(changeType: ChangeType, startIndex?: number, size?: number, dAppId?: string): Promise<string[]>;
    /**
     * Returns the stored redeem script.
     * @example
     * const redeemScript = await bchWalletBridge.getRedeemScript(
     *   "bitcoincash:prr7qqutastjmc9dn7nwkv2vcc58nn82uqwzq563hg",
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(redeemScript)
     * > "03424f587e06424954424f5887"
     * @param p2shAddress The P2SH Address.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The stored redeem script.
     */
    getRedeemScript(p2shAddress: string, dAppId?: string): Promise<string | undefined>;
    /**
     * Returns the stored redeem scripts belong to the DApp ID.
     * @example
     * const redeemScripts = await bchWalletBridge.getRedeemScript(
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(redeemScript)
     * > ["03424f587e06424954424f5887", "789787a72c21452a1c98ff"]
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The stored redeem script list.
     */
    getRedeemScripts(dAppId?: string): Promise<string[]>;
    /**
     * Add the redeem script into the wallet.
     * @example
     * await bchWalletBridge.addRedeemScript(
     *   "03424f587e064249..."
     * )
     * @param redeemScript The redeem script you want to add.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     */
    addRedeemScript(redeemScript: string, dAppId?: string): Promise<void>;
    /**
     * Returns the unspent transaction outputs.
     * @example
     * const utxos = await bchWalletBridge.getUtxos(
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(utxos)
     * > [
     *     {
     *       'txId' : '115e8f72f39fad874cfab0deed11a80f24f967a84079fb56ddf53ea02e308986',
     *       'outputIndex' : 0,
     *       'address' : 'bitcoincash:qrsy0xwugcajsqa99c9nf05pz7ndckj55ctlsztu2p',
     *       'script' : '76a91447862fe165e6121af80d5dde1ecb478ed170565b88ac',
     *       'satoshis' : 50000
     *     }
     *   ]
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The unspent transaction output object list.
     */
    getUtxos(dAppId?: string): Promise<Utxo[]>;
    /**
     * Returns the balance of the addresses.
     * @example
     * const balance = await bchWalletBridge.getBalance(
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(balance)
     * > 500000
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The current balance for the addresses in satoshi.
     */
    getBalance(dAppId?: string): Promise<number>;
    /**
     * Signs data from a specific account. This account needs to be unlocked.
     * @example
     * const result = await bchWalletBridge.sign(
     *   "bchtest:qq28xgrzkdyeg5vf7tp2s3mvx8u95zes5cf7wpwgux",
     *   "af4c61ddcc5e8a2d..." // second argument is SHA1("hello")
     * )
     * console.log(result)
     * > "30440220227e0973..."
     * @param address Address to sign with.
     * @param dataToSign Data to sign in hex format.
     * @returns The signed data. Bitcoin signatures are serialised in the DER format over the wire.
     */
    sign(address: string, dataToSign: string): Promise<string>;
    /**
     * Create a transaction with specified outputs and return the signed raw transaction.
     * The provider will not add any outputs. The ordering of outputs remains as is.
     * @example
     * const rawTx = await bchWalletBridge.buildTransaction([
     *   {
     *     lockScript: "76a91467b2e55ada06c869547e93288a4cf7377211f1f088ac",
     *     amount: 10000
     *   }
     * ])
     * console.log(rawtx)
     * > "..."
     * @param outputs The Array of TransactionOutput objects. Throws an error, when the array is empty.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The signed raw transaction.
     */
    buildTransaction(outputs: Output[], dAppId?: string): Promise<string>;
    /**
     * Returns the bitcoin protocol version.
     * @example
     * const version = await bchWalletBridge.getProtocolVersion()
     * console.log(version)
     * > 70015
     * @returns The protocol version. The value is Int32.
     */
    getProtocolVersion(): Promise<number>;
    /**
     * Returns the current network.
     * @example
     * const network = await bchWalletBridge.getNetwork()
     * console.log(network)
     * > {
     *     magicBytes: "e3e1f3e8",
     *     name: "Mainnet"
     *   }
     * @returns The network object.
     */
    getNetwork(): Promise<Network>;
    /**
     * Returns the transaction fee per byte.
     * @example
     * const fee = await bchWalletBridge.getFeePerByte()
     * console.log(fee)
     * > 1
     * @returns Transaction fee per byte in satoshi.
     */
    getFeePerByte(): Promise<number>;
    /**
     * Returns the default DApp ID the provider uses.
     * The default value is undefined.
     * @example
     * const dAppId = await bchWalletBridge.defaultDAppId()
     * console.log(dAppId)
     * > "53212266f7994100..."
     * @returns The DApp ID
     */
    getDefaultDAppId(): Promise<string | undefined>;
    /**
     * Changes the default DApp ID for the provider.
     * @example
     * const result = await bchWalletBridge.setDefaultDAppId("53212266f7994100...")
     * console.log(result)
     * > true
     * @param The DApp ID.
     */
    setDefaultDAppId(dAppId?: string): Promise<void>;
}
