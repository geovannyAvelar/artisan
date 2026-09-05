function pick(useNumber: boolean): any {
  return useNumber ? 5 : "text";
}

function main(): number {
  if (typeof pick(true) != "number") { return 1; }
  if (typeof pick(false) != "string") { return 1; }
  return 0;
}
