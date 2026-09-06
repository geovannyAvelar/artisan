// A closure can never declare 'throws' itself - rejected at the
// grammar-accepts-but-Sema-rejects boundary.

function main(): void {
  let h: () => void = function(): void throws string {
    throw "boom";
  };
}
