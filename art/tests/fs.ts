import { FileHandle, FileResult, readFile, writeFile, appendFile, exists, deleteFile, rename, copyFile, fileSize, getFileType, isFile, isDirectory, createDirectory, deleteDirectory, listDirectory, openFile, closeFile, readFromFile, writeToFile, seek, tell, getFileSize, isEOF, readFileSync, writeFileSync, appendFileSync, readLines, writeLines, withFile, fileCount, clearFileSystem, MODE_READ, MODE_WRITE, MODE_APPEND, FILE_TYPE_FILE, FILE_TYPE_DIRECTORY } from "art/fs";

function testWriteAndRead(): number {
  clearFileSystem();

  if (!writeFile("/test.txt", "Hello, World!")) { return 1; }

  let result: FileResult = readFile("/test.txt");
  if (!result[0]) { return 2; }
  if (result[1] != "Hello, World!") { return 3; }

  return 0;
}

function testExists(): number {
  clearFileSystem();

  if (exists("/nonexistent.txt")) { return 1; }

  writeFile("/test.txt", "content");
  if (!exists("/test.txt")) { return 2; }

  return 0;
}

function testDeleteFile(): number {
  clearFileSystem();

  writeFile("/test.txt", "content");
  if (!exists("/test.txt")) { return 1; }

  if (!deleteFile("/test.txt")) { return 2; }
  if (exists("/test.txt")) { return 3; }

  return 0;
}

function testAppendFile(): number {
  clearFileSystem();

  writeFile("/test.txt", "Hello");
  if (!appendFile("/test.txt", " World")) { return 1; }

  let result: FileResult = readFile("/test.txt");
  if (result[1] != "Hello World") { return 2; }

  return 0;
}

function testRenameFile(): number {
  clearFileSystem();

  writeFile("/old.txt", "content");
  if (!rename("/old.txt", "/new.txt")) { return 1; }
  if (exists("/old.txt")) { return 2; }
  if (!exists("/new.txt")) { return 3; }

  return 0;
}

function testCopyFile(): number {
  clearFileSystem();

  writeFile("/source.txt", "original");
  if (!copyFile("/source.txt", "/dest.txt")) { return 1; }

  let result: FileResult = readFile("/dest.txt");
  if (result[1] != "original") { return 2; }

  return 0;
}

function testFileSize(): number {
  clearFileSystem();

  writeFile("/test.txt", "12345");
  if (fileSize("/test.txt") != 5) { return 1; }

  if (fileSize("/nonexistent") != -1) { return 2; }

  return 0;
}

function testIsFile(): number {
  clearFileSystem();

  writeFile("/file.txt", "content");
  createDirectory("/dir");

  if (!isFile("/file.txt")) { return 1; }
  if (isFile("/dir")) { return 2; }

  return 0;
}

function testIsDirectory(): number {
  clearFileSystem();

  writeFile("/file.txt", "content");
  createDirectory("/dir");

  if (isDirectory("/file.txt")) { return 1; }
  if (!isDirectory("/dir")) { return 2; }

  return 0;
}

function testCreateDirectory(): number {
  clearFileSystem();

  if (!createDirectory("/mydir")) { return 1; }
  if (!exists("/mydir")) { return 2; }
  if (!isDirectory("/mydir")) { return 3; }

  if (createDirectory("/mydir")) { return 4; }

  return 0;
}

function testDeleteDirectory(): number {
  clearFileSystem();

  createDirectory("/dir");
  if (!exists("/dir")) { return 1; }

  if (!deleteDirectory("/dir")) { return 2; }
  if (exists("/dir")) { return 3; }

  return 0;
}

function testListDirectory(): number {
  clearFileSystem();

  createDirectory("/dir");
  writeFile("/dir/file1.txt", "a");
  writeFile("/dir/file2.txt", "b");

  let entries: string[] = listDirectory("/dir");
  if (entries.length != 2) { return 1; }

  return 0;
}

function testOpenAndCloseFile(): number {
  clearFileSystem();

  writeFile("/test.txt", "Hello");
  let handle: FileHandle = openFile("/test.txt", MODE_READ);
  if (handle < 0) { return 1; }

  if (!closeFile(handle)) { return 2; }

  return 0;
}

function testReadFromFile(): number {
  clearFileSystem();

  writeFile("/test.txt", "Hello World");
  let handle: FileHandle = openFile("/test.txt", MODE_READ);
  if (handle < 0) { return 1; }

  let data: string = readFromFile(handle, 5);
  if (data != "Hello") { return 2; }

  closeFile(handle);
  return 0;
}

function testWriteToFile(): number {
  clearFileSystem();

  let handle: FileHandle = openFile("/test.txt", MODE_WRITE);
  if (handle < 0) { return 1; }

  if (!writeToFile(handle, "Test data")) { return 2; }
  if (!closeFile(handle)) { return 3; }

  let result: FileResult = readFile("/test.txt");
  if (result[1] != "Test data") { return 4; }

  return 0;
}

function testSeekTell(): number {
  clearFileSystem();

  writeFile("/test.txt", "0123456789");
  let handle: FileHandle = openFile("/test.txt", MODE_READ);

  if (tell(handle) != 0) { return 1; }

  readFromFile(handle, 3);
  if (tell(handle) != 3) { return 2; }

  if (!seek(handle, 5)) { return 3; }
  if (tell(handle) != 5) { return 4; }

  closeFile(handle);
  return 0;
}

function testIsEOF(): number {
  clearFileSystem();

  writeFile("/test.txt", "abc");
  let handle: FileHandle = openFile("/test.txt", MODE_READ);

  if (isEOF(handle)) { return 1; }

  readFromFile(handle, 10);
  if (!isEOF(handle)) { return 2; }

  closeFile(handle);
  return 0;
}

function testReadFileSync(): number {
  clearFileSystem();

  writeFile("/test.txt", "Hello");
  let content: string = readFileSync("/test.txt");
  if (content != "Hello") { return 1; }

  return 0;
}

function testWriteFileSync(): number {
  clearFileSystem();

  if (!writeFileSync("/test.txt", "Sync write")) { return 1; }

  let result: FileResult = readFile("/test.txt");
  if (result[1] != "Sync write") { return 2; }

  return 0;
}

function testAppendFileSync(): number {
  clearFileSystem();

  writeFile("/test.txt", "Start");
  if (!appendFileSync("/test.txt", " End")) { return 1; }

  let result: FileResult = readFile("/test.txt");
  if (result[1] != "Start End") { return 2; }

  return 0;
}

function testReadLines(): number {
  clearFileSystem();

  writeFile("/test.txt", "line1\nline2\nline3");
  let lines: string[] = readLines("/test.txt");

  if (lines.length != 3) { return 1; }
  if (lines[0] != "line1") { return 2; }
  if (lines[1] != "line2") { return 3; }
  if (lines[2] != "line3") { return 4; }

  return 0;
}

function testWriteLines(): number {
  clearFileSystem();

  let lines: string[] = ["first", "second", "third"];
  if (!writeLines("/test.txt", lines)) { return 1; }

  let result: FileResult = readFile("/test.txt");
  if (result[1] != "first\nsecond\nthird") { return 2; }

  return 0;
}

function testFileCount(): number {
  clearFileSystem();

  if (fileCount() != 0) { return 1; }

  writeFile("/file1.txt", "a");
  if (fileCount() != 1) { return 2; }

  writeFile("/file2.txt", "b");
  if (fileCount() != 2) { return 3; }

  return 0;
}

function testMultipleFiles(): number {
  clearFileSystem();

  writeFile("/file1.txt", "content1");
  writeFile("/file2.txt", "content2");
  writeFile("/file3.txt", "content3");

  let r1: FileResult = readFile("/file1.txt");
  let r2: FileResult = readFile("/file2.txt");
  let r3: FileResult = readFile("/file3.txt");

  if (r1[1] != "content1") { return 1; }
  if (r2[1] != "content2") { return 2; }
  if (r3[1] != "content3") { return 3; }

  return 0;
}

function testGetFileSize(): number {
  clearFileSystem();

  writeFile("/test.txt", "12345678");
  let handle: FileHandle = openFile("/test.txt", MODE_READ);

  if (getFileSize(handle) != 8) { return 1; }

  closeFile(handle);
  return 0;
}

function testAppendMode(): number {
  clearFileSystem();

  writeFile("/test.txt", "Start");
  let handle: FileHandle = openFile("/test.txt", MODE_APPEND);

  if (!writeToFile(handle, " Appended")) { return 1; }
  if (!closeFile(handle)) { return 2; }

  let result: FileResult = readFile("/test.txt");
  if (result[1] != "Start Appended") { return 3; }

  return 0;
}

function testSequentialRead(): number {
  clearFileSystem();

  writeFile("/test.txt", "ABCDEFGHIJ");
  let handle: FileHandle = openFile("/test.txt", MODE_READ);

  let p1: string = readFromFile(handle, 2);
  if (p1 != "AB") { return 1; }

  let p2: string = readFromFile(handle, 3);
  if (p2 != "CDE") { return 2; }

  let p3: string = readFromFile(handle, 4);
  if (p3 != "FGHI") { return 3; }

  closeFile(handle);
  return 0;
}

function testInvalidOperations(): number {
  clearFileSystem();

  let invalidHandle: FileHandle = 999;
  if (readFromFile(invalidHandle, 10) != "") { return 1; }
  if (tell(invalidHandle) != -1) { return 2; }
  if (getFileSize(invalidHandle) != -1) { return 3; }

  return 0;
}

function testFileOverwrite(): number {
  clearFileSystem();

  writeFile("/test.txt", "original");
  writeFile("/test.txt", "overwritten");

  let result: FileResult = readFile("/test.txt");
  if (result[1] != "overwritten") { return 1; }

  return 0;
}

function testEmptyFile(): number {
  clearFileSystem();

  writeFile("/empty.txt", "");
  if (fileSize("/empty.txt") != 0) { return 1; }

  let result: FileResult = readFile("/empty.txt");
  if (result[1] != "") { return 2; }

  return 0;
}

function testLargeContent(): number {
  clearFileSystem();

  let large: string = "";
  let i: number = 0;
  while (i < 100) {
    large = large + "x";
    i = i + 1;
  }

  writeFile("/large.txt", large);
  if (fileSize("/large.txt") != 100) { return 1; }

  return 0;
}

function testGetFileType(): number {
  clearFileSystem();

  writeFile("/file.txt", "content");
  createDirectory("/mydir");

  if (getFileType("/file.txt") != FILE_TYPE_FILE) { return 1; }
  if (getFileType("/mydir") != FILE_TYPE_DIRECTORY) { return 2; }
  if (getFileType("/nonexistent") != -1) { return 3; }

  return 0;
}
