declare module "bchaddrjs" {
  function isCashAddress(address: string): boolean
  function isP2SHAddress(address: string): boolean
  function toCashAddress(address: string): string
  function toLegacyAddress(address: string): string
}
