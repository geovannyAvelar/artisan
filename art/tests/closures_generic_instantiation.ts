// A closure declared inside a generic function's body, called through
// two different instantiations - each instantiation's own clone (see
// ast.cpp's CloneFunctionDecl) gets its own independent closure/thunk
// and its own independent capture analysis. Also captures a Handler-
// typed parameter (`sink`) itself, exercising a boxed Handler read plus
// an indirect call through it.

function assertEq(actual: number, expected: number, failCount: number): number {
  if (actual != expected) { return failCount + 1; }
  return failCount;
}

function runTwice<T>(value: T, sink: (v: T) => void): void {
  let captured: T = value;
  let emit: () => void = function(): void { sink(captured); };
  emit();
  emit();
}

let numCalls: number = 0;
let numSum: number = 0;
function sinkNumber(v: number): void {
  numCalls = numCalls + 1;
  numSum = numSum + v;
}

let strCalls: number = 0;
let strLast: string = "";
function sinkString(v: string): void {
  strCalls = strCalls + 1;
  strLast = v;
}

function main(): number {
  let fails: number = 0;

  runTwice::<number>(5, sinkNumber);
  fails = assertEq(numCalls, 2, fails);
  fails = assertEq(numSum, 10, fails);

  runTwice::<string>("hi", sinkString);
  fails = assertEq(strCalls, 2, fails);
  if (strLast != "hi") { fails = fails + 1; }

  return fails;
}
