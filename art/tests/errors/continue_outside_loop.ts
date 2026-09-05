// Expected error: `continue` isn't inside any loop (a switch alone
// doesn't count - continue always means "the nearest enclosing loop").
function main(): number {
  switch (1) {
    case 1:
      continue;
  }
  return 0;
}
