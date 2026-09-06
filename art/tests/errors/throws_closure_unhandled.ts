// A closure containing an unhandled throw - rejected even though the
// ENCLOSING function declares a matching 'throws' - a closure can
// never let an exception escape uncaught (see ExprKind::FunctionExpr's
// own Sema case).

function outer(): void throws string {
  let h: () => void = function(): void {
    throw "boom";
  };
  h();
}

function main(): void throws string {
  outer();
}
