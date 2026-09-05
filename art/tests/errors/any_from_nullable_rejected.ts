function useIt(x: number | null): number {
  let a: any = x; // Nullable(T) can't be widened into `any` directly (see art/README.md)
  return 0;
}

function main(): number {
  return useIt(null);
}
