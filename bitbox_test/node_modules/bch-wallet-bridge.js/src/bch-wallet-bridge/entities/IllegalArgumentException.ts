export default class IllegalArgumentException extends Error {
  public name = "IllegalArgumentException"

  constructor(public message: string) {
    super(message)
  }
}
