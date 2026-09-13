import { normalize, join, resolve, dirname, basename, basenameWithoutExt, extname, isAbsolute, isRelative, split, isChild, commondir, relative, append, removeTrailingSeparator, addTrailingSeparator, equals, depth, up } from "art/path";

function testNormalize(): number {
  if (normalize("/foo/bar") != "/foo/bar") { return 1; }
  if (normalize("/foo//bar") != "/foo/bar") { return 2; }
  if (normalize("./foo") != "foo") { return 3; }
  return 0;
}

function testNormalizeEmpty(): number {
  if (normalize("") != ".") { return 1; }
  return 0;
}

function testJoin(): number {
  let result: string = join(["/foo", "bar", "baz"]);
  if (result != "/foo/bar/baz") { return 1; }
  return 0;
}

function testJoinEmpty(): number {
  let result: string = join([]);
  if (result != ".") { return 1; }
  return 0;
}

function testJoinWithDots(): number {
  let result: string = join(["/foo", ".", "bar"]);
  if (result != "/foo/bar") { return 1; }
  return 0;
}

function testResolve(): number {
  let result: string = resolve("/foo/bar", "baz");
  if (result != "/foo/bar/baz") { return 1; }
  return 0;
}

function testResolveAbsolute(): number {
  let result: string = resolve("/foo/bar", "/baz");
  if (result != "/baz") { return 1; }
  return 0;
}

function testDirname(): number {
  if (dirname("/foo/bar/baz") != "/foo/bar") { return 1; }
  if (dirname("/foo/bar") != "/foo") { return 2; }
  if (dirname("/foo") != "/") { return 3; }
  if (dirname("foo") != ".") { return 4; }
  return 0;
}

function testBasename(): number {
  if (basename("/foo/bar/baz.txt") != "baz.txt") { return 1; }
  if (basename("foo.txt") != "foo.txt") { return 2; }
  if (basename("/foo") != "foo") { return 3; }
  return 0;
}

function testBasenameWithoutExt(): number {
  if (basenameWithoutExt("/foo/bar/baz.txt") != "baz") { return 1; }
  if (basenameWithoutExt("foo.tar.gz") != "foo.tar") { return 2; }
  if (basenameWithoutExt("foo") != "foo") { return 3; }
  return 0;
}

function testExtname(): number {
  if (extname("/foo/bar/baz.txt") != ".txt") { return 1; }
  if (extname("foo.tar.gz") != ".gz") { return 2; }
  if (extname("foo") != "") { return 3; }
  return 0;
}

function testIsAbsolute(): number {
  if (!isAbsolute("/foo/bar")) { return 1; }
  if (isAbsolute("foo/bar")) { return 2; }
  if (isAbsolute("")) { return 3; }
  return 0;
}

function testIsRelative(): number {
  if (!isRelative("foo/bar")) { return 1; }
  if (isRelative("/foo/bar")) { return 2; }
  return 0;
}

function testSplit(): number {
  let parts: string[] = split("/foo/bar/baz");
  if (parts.length != 3) { return 1; }
  if (parts[0] != "foo") { return 2; }
  if (parts[1] != "bar") { return 3; }
  if (parts[2] != "baz") { return 4; }
  return 0;
}

function testSplitEmpty(): number {
  let parts: string[] = split("");
  if (parts.length != 0) { return 1; }
  return 0;
}

function testIsChild(): number {
  if (!isChild("/foo", "/foo/bar")) { return 1; }
  if (isChild("/foo", "/foobar")) { return 2; }
  if (isChild("/foo/bar", "/foo")) { return 3; }
  return 0;
}

function testCommondir(): number {
  let result: string = commondir("/foo/bar/baz", "/foo/bar/qux");
  if (result != "/foo/bar") { return 1; }

  let result2: string = commondir("/foo", "/bar");
  if (result2 != ".") { return 2; }
  return 0;
}

function testRelative(): number {
  let result: string = relative("/foo/bar", "/foo/bar/baz");
  if (result != "baz") { return 1; }

  let result2: string = relative("/foo/bar/baz", "/foo/bar");
  if (result2 != "..") { return 2; }
  return 0;
}

function testAppend(): number {
  let result: string = append("/foo/bar", "baz.txt");
  if (result != "/foo/bar/baz.txt") { return 1; }
  return 0;
}

function testRemoveTrailingSeparator(): number {
  if (removeTrailingSeparator("/foo/bar/") != "/foo/bar") { return 1; }
  if (removeTrailingSeparator("/foo/bar") != "/foo/bar") { return 2; }
  if (removeTrailingSeparator("/") != "/") { return 3; }
  return 0;
}

function testAddTrailingSeparator(): number {
  if (addTrailingSeparator("/foo/bar") != "/foo/bar/") { return 1; }
  if (addTrailingSeparator("/foo/bar/") != "/foo/bar/") { return 2; }
  return 0;
}

function testEquals(): number {
  if (!equals("/foo/bar", "/foo/bar")) { return 1; }
  if (!equals("/foo/bar/", "/foo/bar")) { return 2; }
  if (equals("/foo/bar", "/foo/baz")) { return 3; }
  return 0;
}

function testDepth(): number {
  if (depth("") != 0) { return 1; }
  if (depth("/foo") != 1) { return 2; }
  if (depth("/foo/bar") != 2) { return 3; }
  if (depth("/foo/bar/baz") != 3) { return 4; }
  return 0;
}

function testUp(): number {
  if (up("/foo/bar/baz", 1) != "/foo/bar") { return 1; }
  if (up("/foo/bar/baz", 2) != "/foo") { return 2; }
  if (up("/foo/bar/baz", 0) != "/foo/bar/baz") { return 3; }
  return 0;
}

function testUpNegative(): number {
  if (up("/foo/bar", -1) != "/foo/bar") { return 1; }
  return 0;
}
