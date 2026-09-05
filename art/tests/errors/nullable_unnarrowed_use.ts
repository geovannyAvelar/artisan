function useIt(x: number | null): number {
  return x + 1;
}

function main(): number {
  return useIt(null);
}
