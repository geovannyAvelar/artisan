// URL utilities for ART. Import with: `import { parseURL, buildURL, ... } from "art/url";`
// Provides URL parsing and manipulation utilities.

// Parses a URL string into components. Returns array: [protocol, host, path, query, fragment]
export function parseURL(url: string): string[] {
  let protocol: string = "";
  let host: string = "";
  let path: string = "";
  let query: string = "";
  let fragment: string = "";

  let idx: number = url.indexOf("://");
  if (idx >= 0) {
    protocol = url.substring(0, idx);
    let rest: string = url.substring(idx + 3);

    idx = rest.indexOf("/");
    if (idx < 0) {
      host = rest;
      idx = host.indexOf("?");
      if (idx >= 0) {
        let hostPart: string = host.substring(0, idx);
        query = host.substring(idx + 1);
        host = hostPart;
      }
    } else {
      host = rest.substring(0, idx);
      path = rest.substring(idx);
    }
  } else {
    path = url;
  }

  idx = path.indexOf("?");
  if (idx >= 0) {
    let queryPart: string = path.substring(idx + 1);
    path = path.substring(0, idx);

    let fragmentIdx: number = queryPart.indexOf("#");
    if (fragmentIdx >= 0) {
      fragment = queryPart.substring(fragmentIdx + 1);
      query = queryPart.substring(0, fragmentIdx);
    } else {
      query = queryPart;
    }
  } else {
    idx = path.indexOf("#");
    if (idx >= 0) {
      fragment = path.substring(idx + 1);
      path = path.substring(0, idx);
    }
  }

  return [protocol, host, path, query, fragment];
}

// Builds a URL from components. Input: [protocol, host, path, query, fragment]
export function buildURL(components: string[]): string {
  if (components.length < 5) { return ""; }

  let url: string = "";
  if (components[0] != "") {
    url = components[0] + "://";
  }
  url = url + components[1];
  url = url + components[2];

  if (components[3] != "") {
    url = url + "?" + components[3];
  }
  if (components[4] != "") {
    url = url + "#" + components[4];
  }

  return url;
}

// Gets the protocol from URL.
export function getProtocol(url: string): string {
  let parts: string[] = parseURL(url);
  return parts[0];
}

// Gets the hostname from URL.
export function getHost(url: string): string {
  let parts: string[] = parseURL(url);
  return parts[1];
}

// Gets the path from URL.
export function getPath(url: string): string {
  let parts: string[] = parseURL(url);
  return parts[2];
}

// Gets the query string from URL.
export function getQuery(url: string): string {
  let parts: string[] = parseURL(url);
  return parts[3];
}

// Gets the fragment from URL.
export function getFragment(url: string): string {
  let parts: string[] = parseURL(url);
  return parts[4];
}

// Parses query string into key-value pairs. Returns array of [key, value] pairs.
export function parseQuery(queryString: string): string[][] {
  let result: string[][] = [];
  if (queryString == "") { return result; }

  let pairs: string[] = [];
  let current: string = "";
  let i: number = 0;

  while (i < queryString.length) {
    let c: string = queryString.substring(i, i + 1);
    if (c == "&") {
      if (current != "") {
        pairs = pairs + [current];
      }
      current = "";
    } else {
      current = current + c;
    }
    i = i + 1;
  }
  if (current != "") {
    pairs = pairs + [current];
  }

  i = 0;
  while (i < pairs.length) {
    let eqIdx: number = pairs[i].indexOf("=");
    if (eqIdx >= 0) {
      let key: string = pairs[i].substring(0, eqIdx);
      let value: string = pairs[i].substring(eqIdx + 1);
      result = result + [[key, value]];
    } else {
      result = result + [[pairs[i], ""]];
    }
    i = i + 1;
  }

  return result;
}

// Builds query string from key-value pairs.
export function buildQuery(params: string[][]): string {
  if (params.length == 0) { return ""; }

  let result: string = "";
  let i: number = 0;

  while (i < params.length) {
    if (i > 0) { result = result + "&"; }
    result = result + params[i][0] + "=" + params[i][1];
    i = i + 1;
  }

  return result;
}

// Checks if URL is absolute (has protocol).
export function isAbsolute(url: string): boolean {
  return url.indexOf("://") >= 0;
}

// Checks if URL is relative.
export function isRelative(url: string): boolean {
  return !isAbsolute(url);
}

// Joins base URL with relative path.
export function joinURL(base: string, relative: string): string {
  if (isAbsolute(relative)) { return relative; }

  let lastSlash: number = base.lastIndexOf("/");
  if (lastSlash < 0) { return base + relative; }

  return base.substring(0, lastSlash + 1) + relative;
}

// Gets the domain from URL.
export function getDomain(url: string): string {
  let host: string = getHost(url);
  let colonIdx: number = host.indexOf(":");
  if (colonIdx >= 0) {
    return host.substring(0, colonIdx);
  }
  return host;
}

// Gets the port from URL (or empty string if not specified).
export function getPort(url: string): string {
  let host: string = getHost(url);
  let colonIdx: number = host.indexOf(":");
  if (colonIdx >= 0) {
    return host.substring(colonIdx + 1);
  }
  return "";
}
