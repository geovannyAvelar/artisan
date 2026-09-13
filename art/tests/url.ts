import { parseURL, buildURL, getProtocol, getHost, getPath, getQuery, getFragment, parseQuery, buildQuery, isAbsolute, isRelative, joinURL, getDomain, getPort } from "art/url";

function testParseURL(): number {
  let parts: string[] = parseURL("https://example.com/path?query=1#frag");
  if (parts.length != 5) { return 1; }
  if (parts[0] != "https") { return 2; }
  if (parts[1] != "example.com") { return 3; }
  if (parts[2] != "/path") { return 4; }
  return 0;
}

function testBuildURL(): number {
  let parts: string[] = ["https", "example.com", "/path", "q=1", "frag"];
  let url: string = buildURL(parts);
  if (url.length == 0) { return 1; }
  if (url.indexOf("https") == -1) { return 2; }
  return 0;
}

function testGetProtocol(): number {
  if (getProtocol("https://example.com") != "https") { return 1; }
  if (getProtocol("http://test.com") != "http") { return 2; }
  return 0;
}

function testGetHost(): number {
  if (getHost("https://example.com/path") != "example.com") { return 1; }
  return 0;
}

function testGetPath(): number {
  if (getPath("https://example.com/path/to/resource") != "/path/to/resource") { return 1; }
  return 0;
}

function testGetQuery(): number {
  if (getQuery("https://example.com?key=value") != "key=value") { return 1; }
  return 0;
}

function testGetFragment(): number {
  if (getFragment("https://example.com#section") != "section") { return 1; }
  return 0;
}

function testParseQuery(): number {
  let pairs: string[][] = parseQuery("a=1&b=2");
  if (pairs.length != 2) { return 1; }
  if (pairs[0][0] != "a" || pairs[0][1] != "1") { return 2; }
  return 0;
}

function testBuildQuery(): number {
  let pairs: string[][] = [["a", "1"], ["b", "2"]];
  let query: string = buildQuery(pairs);
  if (query.length == 0) { return 1; }
  if (query.indexOf("a=1") == -1) { return 2; }
  return 0;
}

function testIsAbsolute(): number {
  if (!isAbsolute("https://example.com")) { return 1; }
  if (isAbsolute("/path")) { return 2; }
  return 0;
}

function testIsRelative(): number {
  if (!isRelative("/path")) { return 1; }
  if (isRelative("https://example.com")) { return 2; }
  return 0;
}

function testGetDomain(): number {
  if (getDomain("https://example.com:8080") != "example.com") { return 1; }
  return 0;
}

function testGetPort(): number {
  if (getPort("https://example.com:8080") != "8080") { return 1; }
  if (getPort("https://example.com") != "") { return 2; }
  return 0;
}
