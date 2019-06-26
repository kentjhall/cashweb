// see https://github.com/bitcoin/bips/blob/master/bip-0068.mediawiki#compatibility

const SEQUENCE_FINAL = 0xffffffff
const SEQUENCE_LOCKTIME_DISABLE_FLAG = (1 << 31)
const SEQUENCE_LOCKTIME_GRANULARITY = 9
const SEQUENCE_LOCKTIME_MASK = 0x0000ffff
const SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22)

const BLOCKS_MAX = SEQUENCE_LOCKTIME_MASK
const SECONDS_MOD = 1 << SEQUENCE_LOCKTIME_GRANULARITY
const SECONDS_MAX = SEQUENCE_LOCKTIME_MASK << SEQUENCE_LOCKTIME_GRANULARITY

function decode (sequence) {
  if (sequence & SEQUENCE_LOCKTIME_DISABLE_FLAG) return {}
  if (sequence & SEQUENCE_LOCKTIME_TYPE_FLAG) {
    return {
      seconds: (sequence & SEQUENCE_LOCKTIME_MASK) << SEQUENCE_LOCKTIME_GRANULARITY
    }
  }

  return {
    blocks: sequence & SEQUENCE_LOCKTIME_MASK
  }
}

function encode (blockSeconds) {
  const blocks = blockSeconds.blocks
  const seconds = blockSeconds.seconds
  if (blocks !== undefined && seconds !== undefined) throw new TypeError('Cannot encode blocks AND seconds')
  if (blocks === undefined && seconds === undefined) return SEQUENCE_FINAL // neither? assume final

  if (seconds !== undefined) {
    if (!Number.isFinite(seconds)) throw new TypeError('Expected Number seconds')
    if (seconds > SECONDS_MAX) throw new TypeError('Expected Number seconds <= ' + SECONDS_MAX)
    if (seconds % SECONDS_MOD !== 0) throw new TypeError('Expected Number seconds as a multiple of ' + SECONDS_MOD)

    return SEQUENCE_LOCKTIME_TYPE_FLAG | (seconds >> SEQUENCE_LOCKTIME_GRANULARITY)
  }

  if (!Number.isFinite(blocks)) throw new TypeError('Expected Number blocks')
  if (blocks > SEQUENCE_LOCKTIME_MASK) throw new TypeError('Expected Number blocks <= ' + BLOCKS_MAX)

  return blocks
}

module.exports = { decode: decode, encode: encode }