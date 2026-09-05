function danger(x: number | null): number {
  if (x != null) {
    x = null;
    return x + 1; // if narrowing isn't invalidated by the reassignment, this unboxes a null ptr
  }
  return -1;
}

function main(): number {
  return danger(notNull::<number>(5));
}
