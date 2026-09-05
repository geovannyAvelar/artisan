// Expected error: `static readonly` reuses a static field's own
// pre-existing 'const' semantics - assignable once, at initialization,
// never again.
class Config {
  static readonly version: number = 1;
}

function main(): number {
  Config.version = 2;
  return 0;
}
