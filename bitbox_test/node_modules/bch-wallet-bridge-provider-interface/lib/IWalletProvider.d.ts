import Utxo from "./entities/Utxo";
import Output from "./entities/Output";
export default interface IWalletProvider {
    /**
     * Returns the current provider interface version.
     * @example
     * const version = await provider.getVersion()
     * console.log(version)
     * > "0.0.1"
     * @returns The current provider interface version
     */
    getVersion(): Promise<string>;
    /**
     * Returns the wallet address list. Address format is CashAddr.
     * @example
     * const addresses = await provider.getAddresses(
     *   0,
     *   2,
     *   3,
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(addresses)
     * > ["bitcoincash:qrsy0xwugcajsqa...", "bitcoincash:qrsfpepw3egqq4k..."]
     * @param change The BIP44 change path.
     * @param size The number of addresses you want.
     * @param startIndex The BIP44 address_index path.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The wallet address list.
     */
    getAddresses(changeType: number, size: number, startIndex?: number, dAppId?: string): Promise<string[]>;
    /**
     * Returns the current wallet address index.
     * @example
     * const addrIdx = await provider.getAddressIndex(
     *   1,
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(addrIdx)
     * > 3
     * @param change The BIP44 change path.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The current wallet address index.
     */
    getAddressIndex(change: number, dAppId?: string): Promise<number>;
    /**
     * Returns the stored redeem scripts related to the DApp ID.
     * @example
     * const redeemScripts = await provider.getRedeemScripts(
     *   "53212266f7994100e442f6dff10fbdb50a93121d25c196ce0597517d35d42e68"
     * )
     * console.log(redeemScripts)
     * > ["03424f587e06424954424f5887", "789787a72c21452a1c98ff"]
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The list of redeem scripts stored.
     */
    getRedeemScripts(dAppId?: string): Promise<string[]>;
    /**
     * Adds the redeem script into the wallet.
     * @example
     * await provider.addRedeemScript(
     *   "03424f587e064249..."
     * )
     * @param redeemScript The redeem script you want to add.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     */
    addRedeemScript(redeemScript: string, dAppId?: string): Promise<void>;
    /**
     * Returns the transaction outputs which addresses are spendable.
     * @example
     * const utxos = await provider.getSpendableUtxos(
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
     * @param dAppId The DApp ID.
     * @returns The unspent transaction output object list.
     */
    getSpendableUtxos(dAppId?: string): Promise<Utxo[]>;
    /**
     * Returns the unspent transaction outputs belonging to the DApp which addresses are unspendable.
     * @example
     * const utxos = await provider.getUnspendableUtxos(
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
     * @param dAppId The DApp ID.
     * @returns The unspent transaction output object list.
     * If the addresses of the DApp are spendable, returns an empty array.
     */
    getUnspendableUtxos(dAppId: string): Promise<Utxo[]>;
    /**
     * Signs a message with the private key of an address.
     * @example
     * const result = await provider.sign(
     *   "bchtest:qq28xgrzkdyeg5vf7tp2s3mvx8u95zes5cf7wpwgux",
     *   "af4c61ddcc5e8a2d..." // second argument is SHA1("hello")
     * )
     * console.log(result)
     * > "30440220227e0973..."
     * @param address A P2PKH address whose private key belongs to the provider.
     * @param dataToSign Data to sign in hex format.
     * @returns The signed data. Bitcoin signatures are serialized in the DER format over the wire.
     * The provider should throw an error when
     * - private key for the address is missing
     * - the user does not allow to sign
     * - etc...
     */
    sign(address: string, dataToSign: string): Promise<string>;
    /**
     * Create a signed transaction with specified outputs. The provider will not add any outputs.
     * @example
     * const txid = await provider.advancedSend([
     *   {
     *     lockScript: "76a91467b2e55ada06c869547e93288a4cf7377211f1f088ac",
     *     amount: 10000
     *   }
     * ])
     * console.log(txid)
     * > "9591fdf10b16d4de6f65bcc49aadadc21d7a3a9169a13815e59011b426fe494f"
     * @param outputs The Array of TransactionOutput objects.
     * @param dAppId The DApp ID. If no dAppId is set the default DApp ID will be set.
     * @returns The signed raw transaction in serialized transaction format encoded as hex.
     * The provider should throw an error when the transaction is not generated.
     */
    createSignedTx(outputs: Output[], dAppId?: string): Promise<string>;
    /**
     * Returns the bitcoin protocol version.
     * @example
     * const version = await provider.getProtocolVersion()
     * console.log(version)
     * > 70015
     * @returns The protocol version.
     */
    getProtocolVersion(): Promise<number>;
    /**
     * Returns the current network.
     * @example
     * const network = await provider.getNetwork()
     * console.log(network)
     * > 3823236072
     * @returns Network magic bytes
     */
    getNetworkMagic(): Promise<number>;
    /**
     * Returns the transaction fee per byte.
     * @example
     * const fee = await provider.getFeePerByte()
     * console.log(fee)
     * > 1
     * @returns Transaction fee per byte in satoshi.
     */
    getFeePerByte(): Promise<number>;
}
