declare module "bitcoincashjs" {
  const Address: Address
  const crypto: Crypto
  const Script: Script
}

interface Address {
  fromScriptHash(hash: Buffer): Address
}

interface Crypto {
  Hash: Hash
}

interface Hash {
  sha256ripemd160(buf: Buffer): Buffer
}

interface Script {
  toBuffer(): Buffer
  fromAddress(address: string): Script
  buildDataOut(data: string | Buffer, encoding?: string): Script
}