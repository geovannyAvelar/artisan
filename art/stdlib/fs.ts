// File System module for ART. Import with: `import { readFile, writeFile, ... } from "art/fs";`
// Provides file I/O operations, directory management, and file utilities.

// File handle type
export type FileHandle = number;

// File operation result type
export type FileResult = [success: boolean, data: string];

// Directory entry type
export type DirEntry = [name: string, isDir: boolean];

// File information type
export type FileInfo = [size: number, type: number, timestamp: number];

// File type constants
export const FILE_TYPE_FILE: number = 0;
export const FILE_TYPE_DIRECTORY: number = 1;
export const FILE_TYPE_SYMLINK: number = 2;

// Open mode constants
export const MODE_READ: number = 0;
export const MODE_WRITE: number = 1;
export const MODE_APPEND: number = 2;

// File storage
type FileData = [path: string, content: string, mode: number, position: number, timestamp: number];
let _files: FileData[] = [];
let _fileHandles: number = 0;
let _fileSystem: [path: string, isDir: boolean, content: string][] = [];

// Reads entire file contents.
export function readFile(path: string): FileResult {
  let i: number = 0;
  while (i < _fileSystem.length) {
    if (_fileSystem[i][0] == path && !_fileSystem[i][1]) {
      return [true, _fileSystem[i][2]];
    }
    i = i + 1;
  }
  return [false, ""];
}

// Writes content to a file (overwrites if exists).
export function writeFile(path: string, content: string): boolean {
  let i: number = 0;
  while (i < _fileSystem.length) {
    if (_fileSystem[i][0] == path && !_fileSystem[i][1]) {
      _fileSystem[i] = [path, false, content];
      return true;
    }
    i = i + 1;
  }

  _fileSystem = _fileSystem + [[path, false, content]];
  return true;
}

// Appends content to a file.
export function appendFile(path: string, content: string): boolean {
  let i: number = 0;
  while (i < _fileSystem.length) {
    if (_fileSystem[i][0] == path && !_fileSystem[i][1]) {
      let current: string = _fileSystem[i][2];
      _fileSystem[i] = [path, false, current + content];
      return true;
    }
    i = i + 1;
  }

  _fileSystem = _fileSystem + [[path, false, content]];
  return true;
}

// Checks if a file or directory exists.
export function exists(path: string): boolean {
  let i: number = 0;
  while (i < _fileSystem.length) {
    if (_fileSystem[i][0] == path) {
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Deletes a file.
export function deleteFile(path: string): boolean {
  let i: number = 0;
  while (i < _fileSystem.length) {
    if (_fileSystem[i][0] == path && !_fileSystem[i][1]) {
      _fileSystem = _fileSystem.slice(0, i) + _fileSystem.slice(i + 1, _fileSystem.length);
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Renames or moves a file.
export function rename(oldPath: string, newPath: string): boolean {
  let i: number = 0;
  while (i < _fileSystem.length) {
    if (_fileSystem[i][0] == oldPath) {
      let isDir: boolean = _fileSystem[i][1];
      let content: string = _fileSystem[i][2];
      _fileSystem[i] = [newPath, isDir, content];
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Copies a file.
export function copyFile(sourcePath: string, destPath: string): boolean {
  let result: FileResult = readFile(sourcePath);
  if (!result[0]) { return false; }
  return writeFile(destPath, result[1]);
}

// Gets file size in bytes.
export function fileSize(path: string): number {
  let result: FileResult = readFile(path);
  if (!result[0]) { return -1; }
  return result[1].length;
}

// Gets file type.
export function getFileType(path: string): number {
  let i: number = 0;
  while (i < _fileSystem.length) {
    if (_fileSystem[i][0] == path) {
      if (_fileSystem[i][1]) { return FILE_TYPE_DIRECTORY; }
      return FILE_TYPE_FILE;
    }
    i = i + 1;
  }
  return -1;
}

// Checks if path is a file.
export function isFile(path: string): boolean {
  return getFileType(path) == FILE_TYPE_FILE;
}

// Checks if path is a directory.
export function isDirectory(path: string): boolean {
  return getFileType(path) == FILE_TYPE_DIRECTORY;
}

// Creates a directory.
export function createDirectory(path: string): boolean {
  if (exists(path)) { return false; }
  _fileSystem = _fileSystem + [[path, true, ""]];
  return true;
}

// Deletes a directory (must be empty).
export function deleteDirectory(path: string): boolean {
  let i: number = 0;
  while (i < _fileSystem.length) {
    if (_fileSystem[i][0] == path && _fileSystem[i][1]) {
      _fileSystem = _fileSystem.slice(0, i) + _fileSystem.slice(i + 1, _fileSystem.length);
      return true;
    }
    i = i + 1;
  }
  return false;
}

// Lists files and directories in a directory.
export function listDirectory(path: string): string[] {
  let result: string[] = [];
  let pathWithSep: string = path + "/";

  let i: number = 0;
  while (i < _fileSystem.length) {
    let filePath: string = _fileSystem[i][0];
    if (filePath.length > pathWithSep.length) {
      let prefix: string = filePath.substring(0, pathWithSep.length);
      if (prefix == pathWithSep) {
        let name: string = filePath.substring(pathWithSep.length);
        let slashIdx: number = indexOf(name, "/");
        if (slashIdx == -1) {
          result = result + [name];
        } else {
          let dirName: string = name.substring(0, slashIdx);
          if (!contains(result, dirName)) {
            result = result + [dirName];
          }
        }
      }
    }
    i = i + 1;
  }

  return result;
}

// Opens a file for reading or writing.
export function openFile(path: string, mode: number): FileHandle {
  let handle: FileHandle = _fileHandles;
  _fileHandles = _fileHandles + 1;

  let result: FileResult = readFile(path);
  if (mode == MODE_READ && result[0]) {
    _files = _files + [[path, result[1], mode, 0, 0]];
    return handle;
  } else if (mode == MODE_WRITE || mode == MODE_APPEND) {
    _files = _files + [[path, "", mode, 0, 0]];
    return handle;
  }

  return -1;
}

// Closes a file handle.
export function closeFile(handle: FileHandle): boolean {
  let i: number = 0;
  while (i < _files.length) {
    if (i == handle) {
      let path: string = _files[i][0];
      let content: string = _files[i][1];
      let mode: number = _files[i][2];

      if (mode == MODE_WRITE) {
        writeFile(path, content);
      } else if (mode == MODE_APPEND) {
        appendFile(path, content);
      }

      return true;
    }
    i = i + 1;
  }
  return false;
}

// Reads from an open file.
export function readFromFile(handle: FileHandle, bytes: number): string {
  if (handle < 0 || handle >= _files.length) { return ""; }

  let fileData: FileData = _files[handle];
  let content: string = fileData[1];
  let position: number = fileData[3];

  if (position >= content.length) { return ""; }

  let end: number = position + bytes;
  if (end > content.length) { end = content.length; }

  let result: string = content.substring(position, end);
  _files[handle] = [fileData[0], content, fileData[2], end, fileData[4]];

  return result;
}

// Writes to an open file.
export function writeToFile(handle: FileHandle, data: string): boolean {
  if (handle < 0 || handle >= _files.length) { return false; }

  let fileData: FileData = _files[handle];
  let content: string = fileData[1];
  let position: number = fileData[3];

  if (fileData[2] == MODE_READ) { return false; }

  let before: string = content.substring(0, position);
  let after: string = content.substring(position + data.length, content.length);
  let newContent: string = before + data + after;

  _files[handle] = [fileData[0], newContent, fileData[2], position + data.length, fileData[4]];
  return true;
}

// Seeks to a position in an open file.
export function seek(handle: FileHandle, position: number): boolean {
  if (handle < 0 || handle >= _files.length) { return false; }

  let fileData: FileData = _files[handle];
  if (position < 0 || position > fileData[1].length) { return false; }

  _files[handle] = [fileData[0], fileData[1], fileData[2], position, fileData[4]];
  return true;
}

// Gets current position in an open file.
export function tell(handle: FileHandle): number {
  if (handle < 0 || handle >= _files.length) { return -1; }
  return _files[handle][3];
}

// Gets file size from an open file handle.
export function getFileSize(handle: FileHandle): number {
  if (handle < 0 || handle >= _files.length) { return -1; }
  return _files[handle][1].length;
}

// Checks if at end of file.
export function isEOF(handle: FileHandle): boolean {
  if (handle < 0 || handle >= _files.length) { return true; }
  let fileData: FileData = _files[handle];
  return fileData[3] >= fileData[1].length;
}

// Reads entire file into memory at once.
export function readFileSync(path: string): string {
  let result: FileResult = readFile(path);
  if (!result[0]) { return ""; }
  return result[1];
}

// Writes entire file at once.
export function writeFileSync(path: string, content: string): boolean {
  return writeFile(path, content);
}

// Appends to file at once.
export function appendFileSync(path: string, content: string): boolean {
  return appendFile(path, content);
}

// Reads file as lines (split by newline).
export function readLines(path: string): string[] {
  let result: FileResult = readFile(path);
  if (!result[0]) { return []; }
  return splitLines(result[1]);
}

// Writes lines to file.
export function writeLines(path: string, lines: string[]): boolean {
  let content: string = "";
  let i: number = 0;
  while (i < lines.length) {
    content = content + lines[i];
    if (i < lines.length - 1) {
      content = content + "\n";
    }
    i = i + 1;
  }
  return writeFile(path, content);
}

// Gets file modification timestamp.
export function getModificationTime(path: string): number {
  let i: number = 0;
  while (i < _fileSystem.length) {
    if (_fileSystem[i][0] == path) {
      return 0;
    }
    i = i + 1;
  }
  return -1;
}

// Clears the virtual file system (for testing).
export function clearFileSystem(): void {
  _fileSystem = [];
  _files = [];
  _fileHandles = 0;
}

// Helper: Find index of substring.
function indexOf(str: string, search: string): number {
  let i: number = 0;
  while (i <= str.length - search.length) {
    let match: boolean = true;
    let j: number = 0;
    while (j < search.length) {
      if (str.substring(i + j, i + j + 1) != search.substring(j, j + 1)) {
        match = false;
      }
      j = j + 1;
    }
    if (match) { return i; }
    i = i + 1;
  }
  return -1;
}

// Helper: Check if array contains value.
function contains(arr: string[], value: string): boolean {
  let i: number = 0;
  while (i < arr.length) {
    if (arr[i] == value) { return true; }
    i = i + 1;
  }
  return false;
}

// Helper: Split string by newlines.
function splitLines(content: string): string[] {
  let result: string[] = [];
  let current: string = "";
  let i: number = 0;

  while (i < content.length) {
    let c: string = content.substring(i, i + 1);
    if (c == "\n") {
      result = result + [current];
      current = "";
    } else if (c != "\r") {
      current = current + c;
    }
    i = i + 1;
  }

  if (current.length > 0) {
    result = result + [current];
  }

  return result;
}

// Executes a function with a file handle, automatically closing it.
export function withFile(path: string, mode: number, fn: (handle: FileHandle) => void): boolean {
  let handle: FileHandle = openFile(path, mode);
  if (handle < 0) { return false; }
  fn(handle);
  return closeFile(handle);
}

// Gets total file count in virtual filesystem.
export function fileCount(): number {
  return _fileSystem.length;
}
