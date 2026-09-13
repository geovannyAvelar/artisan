// Path utilities for ART. Import with: `import { join, dirname, basename, ... } from "art/path";`
// Provides path manipulation utilities similar to Node.js path module.

// Normalizes a path by removing redundant separators and ./ references.
export function normalize(path: string): string {
  if (path.length == 0) { return "."; }

  let result: string = "";
  let i: number = 0;
  let lastWasSeparator: boolean = false;

  while (i < path.length) {
    let c: string = path.substring(i, i + 1);

    if (c == "/") {
      if (!lastWasSeparator) {
        result = result + "/";
        lastWasSeparator = true;
      }
    } else {
      result = result + c;
      lastWasSeparator = false;
    }

    i = i + 1;
  }

  if (result.length > 1 && result.substring(result.length - 1) == "/") {
    result = result.substring(0, result.length - 1);
  }

  return result;
}

// Joins path segments with the appropriate separator.
export function join(paths: string[]): string {
  if (paths.length == 0) { return "."; }

  let result: string = "";
  let i: number = 0;

  while (i < paths.length) {
    if (i > 0 && result.length > 0 && result.substring(result.length - 1) != "/") {
      result = result + "/";
    }

    let segment: string = paths[i];
    if (segment != "" && segment != ".") {
      result = result + segment;
    }

    i = i + 1;
  }

  return normalize(result);
}

// Resolves a relative path against a base path.
export function resolve(base: string, relative: string): string {
  if (relative.substring(0, 1) == "/") {
    return normalize(relative);
  }

  return join([base, relative]);
}

// Gets the directory name of a path.
export function dirname(path: string): string {
  let lastSlash: number = path.lastIndexOf("/");

  if (lastSlash < 0) { return "."; }
  if (lastSlash == 0) { return "/"; }

  return path.substring(0, lastSlash);
}

// Gets the base name of a path (filename with extension).
export function basename(path: string): string {
  let lastSlash: number = path.lastIndexOf("/");

  if (lastSlash < 0) { return path; }
  return path.substring(lastSlash + 1);
}

// Gets the base name without extension.
export function basenameWithoutExt(path: string): string {
  let base: string = basename(path);
  let lastDot: number = base.lastIndexOf(".");

  if (lastDot < 0) { return base; }
  return base.substring(0, lastDot);
}

// Gets the file extension of a path.
export function extname(path: string): string {
  let base: string = basename(path);
  let lastDot: number = base.lastIndexOf(".");

  if (lastDot < 0) { return ""; }
  return base.substring(lastDot);
}

// Checks if a path is absolute.
export function isAbsolute(path: string): boolean {
  if (path.length == 0) { return false; }
  return path.substring(0, 1) == "/";
}

// Checks if a path is relative.
export function isRelative(path: string): boolean {
  return !isAbsolute(path);
}

// Splits a path into its segments.
export function split(path: string): string[] {
  if (path.length == 0) { return []; }

  let parts: string[] = [];
  let current: string = "";
  let i: number = 0;

  while (i < path.length) {
    let c: string = path.substring(i, i + 1);

    if (c == "/") {
      if (current != "") {
        parts = parts + [current];
        current = "";
      }
    } else {
      current = current + c;
    }

    i = i + 1;
  }

  if (current != "") {
    parts = parts + [current];
  }

  return parts;
}

// Checks if a path is a child of another path.
export function isChild(parent: string, child: string): boolean {
  let normalizedParent: string = normalize(parent);
  let normalizedChild: string = normalize(child);

  if (normalizedParent.length >= normalizedChild.length) { return false; }
  if (normalizedChild.substring(0, normalizedParent.length) != normalizedParent) { return false; }

  let nextChar: string = normalizedChild.substring(normalizedParent.length, normalizedParent.length + 1);
  return nextChar == "/" || normalizedParent.substring(normalizedParent.length - 1) == "/";
}

// Gets the common parent directory of two paths.
export function commondir(path1: string, path2: string): string {
  if (path1.length == 0) { return path2; }
  if (path2.length == 0) { return path1; }

  let parts1: string[] = split(normalize(path1));
  let parts2: string[] = split(normalize(path2));

  let commonParts: string[] = [];
  let i: number = 0;

  while (i < parts1.length && i < parts2.length && parts1[i] == parts2[i]) {
    commonParts = commonParts + [parts1[i]];
    i = i + 1;
  }

  if (commonParts.length == 0) { return "."; }

  return join(commonParts);
}

// Gets the relative path from one path to another.
export function relative(from: string, to: string): string {
  let fromParts: string[] = split(normalize(from));
  let toParts: string[] = split(normalize(to));

  let commonIndex: number = 0;
  let i: number = 0;

  while (i < fromParts.length && i < toParts.length && fromParts[i] == toParts[i]) {
    commonIndex = i + 1;
    i = i + 1;
  }

  let upCount: number = fromParts.length - commonIndex;
  let result: string = "";

  i = 0;
  while (i < upCount) {
    if (i > 0) { result = result + "/"; }
    result = result + "..";
    i = i + 1;
  }

  i = commonIndex;
  while (i < toParts.length) {
    if (result.length > 0) { result = result + "/"; }
    result = result + toParts[i];
    i = i + 1;
  }

  if (result.length == 0) { return "."; }
  return result;
}

// Appends a file name to a directory path.
export function append(dir: string, file: string): string {
  return join([dir, file]);
}

// Removes the trailing separator from a path.
export function removeTrailingSeparator(path: string): string {
  if (path.length <= 1) { return path; }
  if (path.substring(path.length - 1) == "/") {
    return path.substring(0, path.length - 1);
  }
  return path;
}

// Adds a trailing separator to a path.
export function addTrailingSeparator(path: string): string {
  if (path.length == 0) { return "/"; }
  if (path.substring(path.length - 1) != "/") {
    return path + "/";
  }
  return path;
}

// Compares two paths for equality (normalized).
export function equals(path1: string, path2: string): boolean {
  return normalize(path1) == normalize(path2);
}

// Gets the depth of a path (number of segments).
export function depth(path: string): number {
  if (path.length == 0) { return 0; }
  let parts: string[] = split(normalize(path));
  return parts.length;
}

// Goes up N directories from a path.
export function up(path: string, levels: number): string {
  if (levels <= 0) { return path; }

  let i: number = 0;
  let result: string = path;

  while (i < levels) {
    result = dirname(result);
    i = i + 1;
  }

  return result;
}
