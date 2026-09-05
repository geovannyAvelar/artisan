function danger(x: any): number {
  if (typeof x == "number") {
    x = "surprise";
    return x + 1; // if narrowing isn't invalidated, this misreads a boxed string as a boxed double
  }
  return -1;
}

function main(): number {
  return danger(5);
}
