export declare enum NetworkType {
    MAINNET = "Mainnet",
    TESTNET3 = "Testnet3",
    UNKNOWN = "Unknown"
}
export default class Network {
    /**
     * Network magic bytes
     */
    magic: number;
    /**
     * Network name
     */
    name: NetworkType;
    constructor(magic: number, name: NetworkType);
}
