// Expected error: a class can't extend a generic class - it isn't a
// plain, non-generic class.
class Box<T> {
  value: T;
}

class IntBox extends Box {
  extra: number;
}

function main(): number {
  return 0;
}
