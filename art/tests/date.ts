import { toISOString, toDateString, toTimeString, parseISOString, addDays, addHours, timeDiff, isLeapYear, daysInMonth, getDayOfWeek } from "art/date";

function testToISOString(): number {
  let timestamp: number = 0;
  let iso: string = toISOString(timestamp);
  if (iso.length == 0) { return 1; }
  return 0;
}

function testToDateString(): number {
  let timestamp: number = 0;
  let dateStr: string = toDateString(timestamp);
  if (dateStr.length == 0) { return 1; }
  return 0;
}

function testToTimeString(): number {
  let timestamp: number = 0;
  let timeStr: string = toTimeString(timestamp);
  if (timeStr.length == 0) { return 1; }
  return 0;
}

function testParseISOString(): number {
  let timestamp: number = parseISOString("2024-01-15");
  if (timestamp == 0) { return 1; }
  return 0;
}

function testAddDays(): number {
  let timestamp: number = 0;
  let future: number = addDays(timestamp, 1);
  if (future <= timestamp) { return 1; }
  return 0;
}

function testAddHours(): number {
  let timestamp: number = 0;
  let later: number = addHours(timestamp, 1);
  if (later <= timestamp) { return 1; }
  return 0;
}

function testTimeDiff(): number {
  let t1: number = 1000;
  let t2: number = 2000;
  let diff: number = timeDiff(t1, t2);
  if (diff != 1000) { return 1; }
  return 0;
}

function testIsLeapYear(): number {
  if (!isLeapYear(2000)) { return 1; }
  if (isLeapYear(1900)) { return 2; }
  if (!isLeapYear(2004)) { return 3; }
  if (isLeapYear(2001)) { return 4; }
  return 0;
}

function testDaysInMonth(): number {
  if (daysInMonth(2024, 2) != 29) { return 1; }  // Leap year
  if (daysInMonth(2023, 2) != 28) { return 2; }
  if (daysInMonth(2024, 4) != 30) { return 3; }
  if (daysInMonth(2024, 1) != 31) { return 4; }
  return 0;
}

function testGetDayOfWeek(): number {
  let timestamp: number = 0;
  let dow: number = getDayOfWeek(timestamp);
  if (dow < 0 || dow > 6) { return 1; }
  return 0;
}
