import { log, info, warn, error, debug, clear, getOutput, setLogLevel, assert, LOG_ALL, LOG_WARN } from "art/console";

function testLog(): number {
  clear();
  log("test message");
  let output: string = getOutput();
  if (output.length == 0) { return 1; }
  if (output.indexOf("test message") == -1) { return 2; }
  return 0;
}

function testInfo(): number {
  clear();
  info("info message");
  let output: string = getOutput();
  if (output.indexOf("info message") == -1) { return 1; }
  return 0;
}

function testWarn(): number {
  clear();
  warn("warning");
  let output: string = getOutput();
  if (output.length == 0) { return 1; }
  return 0;
}

function testError(): number {
  clear();
  error("error message");
  let output: string = getOutput();
  if (output.length == 0) { return 1; }
  return 0;
}

function testDebug(): number {
  clear();
  debug("debug info");
  let output: string = getOutput();
  if (output.length == 0) { return 1; }
  return 0;
}

function testClear(): number {
  log("message");
  clear();
  let output: string = getOutput();
  if (output.length != 0) { return 1; }
  return 0;
}

function testLogLevel(): number {
  clear();
  setLogLevel(LOG_WARN);
  log("should not appear");
  let output: string = getOutput();
  if (output.indexOf("should not appear") >= 0) { return 1; }

  setLogLevel(LOG_ALL);
  log("should appear");
  output = getOutput();
  if (output.indexOf("should appear") == -1) { return 2; }
  return 0;
}

function testAssert(): number {
  clear();
  assert(true, "pass");
  let output: string = getOutput();
  if (output.indexOf("Assertion") >= 0) { return 1; }

  assert(false, "fail");
  output = getOutput();
  if (output.indexOf("fail") == -1) { return 2; }
  return 0;
}
