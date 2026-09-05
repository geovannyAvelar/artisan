// Expected error: a catch clause can only catch 'Error' right now -
// there's no runtime type tag yet to tell two different thrown types
// apart (see StmtKind::Try's own doc comment).
interface NotAnError {
  code: number;
}

function main(): number {
  try {
    throw { message: "x" };
  } catch (e: NotAnError) {
    return 0;
  }
}
