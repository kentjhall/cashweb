export default class ProviderException extends Error {
  public name = "ProviderException"

  constructor(public message: string) {
    super(message)
  }
}
