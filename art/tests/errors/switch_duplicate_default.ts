// Expected error: a switch can have at most one `default:` arm.
function main(): number {
  switch (1) {
    default:
      return 0;
    default:
      return 1;
  }
}
