import { log, error, warn, info, getLogs, getLogCount, clear, setTime, getTime } from "art/console";

function testLogBasic(): number {
  clear();
  log("test");
  if (getLogCount() != 1) { return 1; }
  return 0;
}

function testErrorBasic(): number {
  clear();
  error("error");
  if (getLogCount() != 1) { return 1; }
  return 0;
}

function testWarnBasic(): number {
  clear();
  warn("warning");
  if (getLogCount() != 1) { return 1; }
  return 0;
}

function testInfoBasic(): number {
  clear();
  info("info");
  if (getLogCount() != 1) { return 1; }
  return 0;
}

function testMultipleLogs(): number {
  clear();
  log("a");
  log("b");
  log("c");
  if (getLogCount() != 3) { return 1; }
  return 0;
}

function testClear(): number {
  clear();
  log("test");
  if (getLogCount() != 1) { return 1; }
  clear();
  if (getLogCount() != 0) { return 2; }
  return 0;
}

function testGetLogs(): number {
  clear();
  log("test1");
  log("test2");
  let logs: [string, string, number][] = getLogs();
  if (logs.length != 2) { return 1; }
  return 0;
}

function testTimeTracking(): number {
  clear();
  setTime(0);
  if (getTime() != 0) { return 1; }
  
  setTime(100);
  if (getTime() != 100) { return 2; }
  
  return 0;
}

function testLogTime(): number {
  clear();
  setTime(42);
  log("msg");
  let logs: [string, string, number][] = getLogs();
  if (logs[0][2] != 42) { return 1; }
  return 0;
}
