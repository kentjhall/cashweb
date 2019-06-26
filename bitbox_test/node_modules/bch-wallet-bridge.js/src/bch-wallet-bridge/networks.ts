import Network from "./entities/Network"
import { NetworkType } from "./entities/Network"

const networks = new Map<number, NetworkType>([
  [0xE3E1F3E8, NetworkType.MAINNET],
  [0x0B110907, NetworkType.TESTNET3]
])

export const findNetwork = (magic: number) => {
  const type = networks.get(magic)
  if (type) {
    return new Network(magic, type)
  }
  return new Network(magic, NetworkType.UNKNOWN)
}
