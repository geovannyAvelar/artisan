// Expected error: a JSX child must be string/number/Node/Node[], not
// boolean.
import { Node } from "art";

function main(): number {
  let flag: boolean = true;
  let x: Node = <div>{flag}</div>;
  return 0;
}
