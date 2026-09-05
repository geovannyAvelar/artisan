// Expected error: circular inheritance - A extends B extends A.
class A extends B {
  x: number;
}

class B extends A {
  y: number;
}

function main(): number {
  return 0;
}
