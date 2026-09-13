// Networking module for ART. Import with: `import { fetch, parseUrl, ... } from "art/net";`
// Provides HTTP client, URL parsing, and network utilities.

// HTTP Response type
export type Response = number;

// URL components type
export type URL = [protocol: string, host: string, port: number, path: string, query: string];

// HTTP method constants
export const METHOD_GET: number = 0;
export const METHOD_POST: number = 1;
export const METHOD_PUT: number = 2;
export const METHOD_DELETE: number = 3;
export const METHOD_PATCH: number = 4;
export const METHOD_HEAD: number = 5;
export const METHOD_OPTIONS: number = 6;

// HTTP status codes
export const STATUS_OK: number = 200;
export const STATUS_CREATED: number = 201;
export const STATUS_ACCEPTED: number = 202;
export const STATUS_NO_CONTENT: number = 204;
export const STATUS_BAD_REQUEST: number = 400;
export const STATUS_UNAUTHORIZED: number = 401;
export const STATUS_FORBIDDEN: number = 403;
export const STATUS_NOT_FOUND: number = 404;
export const STATUS_SERVER_ERROR: number = 500;
export const STATUS_SERVICE_UNAVAILABLE: number = 503;

// Response storage
type ResponseData = [status: number, headers: string[], body: string, url: string, timestamp: number];
let _responses: ResponseData[] = [];
let _responseCounter: number = 0;

// Mock network request database for testing
type RequestRecord = [method: number, url: string, headers: string[], body: string, timestamp: number];
let _requests: RequestRecord[] = [];
let _currentTime: number = 0;

// Parses a URL string into components.
export function parseUrl(urlString: string): URL {
  let protocol: string = "";
  let host: string = "";
  let port: number = 80;
  let path: string = "";
  let query: string = "";

  let idx: number = indexOfString(urlString, "://");
  if (idx >= 0) {
    protocol = urlString.substring(0, idx);
    let rest: string = urlString.substring(idx + 3, urlString.length);

    let slashIdx: number = indexOfString(rest, "/");
    if (slashIdx < 0) {
      host = rest;
      path = "/";
    } else {
      host = rest.substring(0, slashIdx);
      let pathPart: string = rest.substring(slashIdx, rest.length);

      let queryIdx: number = indexOfString(pathPart, "?");
      if (queryIdx >= 0) {
        path = pathPart.substring(0, queryIdx);
        query = pathPart.substring(queryIdx + 1, pathPart.length);
      } else {
        path = pathPart;
      }
    }

    let colonIdx: number = indexOfString(host, ":");
    if (colonIdx >= 0) {
      let portStr: string = host.substring(colonIdx + 1, host.length);
      host = host.substring(0, colonIdx);
      port = stringToNumber(portStr);
    } else {
      if (protocol == "https") {
        port = 443;
      } else {
        port = 80;
      }
    }
  }

  return [protocol, host, port, path, query];
}

// Formats a URL from components.
export function formatUrl(url: URL): string {
  let result: string = url[0] + "://" + url[1];

  if ((url[0] == "http" && url[2] != 80) || (url[0] == "https" && url[2] != 443)) {
    result = result + ":" + (url[2] as string);
  }

  result = result + url[3];

  if (url[4].length > 0) {
    result = result + "?" + url[4];
  }

  return result;
}

// Performs an HTTP GET request.
export function get(url: string): Response {
  return request(METHOD_GET, url, [], "");
}

// Performs an HTTP POST request.
export function post(url: string, body: string): Response {
  return request(METHOD_POST, url, ["Content-Type: application/json"], body);
}

// Performs an HTTP PUT request.
export function put(url: string, body: string): Response {
  return request(METHOD_PUT, url, ["Content-Type: application/json"], body);
}

// Performs an HTTP DELETE request.
export function deleteUrl(url: string): Response {
  return request(METHOD_DELETE, url, [], "");
}

// Performs an HTTP PATCH request.
export function patch(url: string, body: string): Response {
  return request(METHOD_PATCH, url, ["Content-Type: application/json"], body);
}

// Performs an HTTP request with custom method and headers.
export function request(method: number, url: string, headers: string[], body: string): Response {
  let responseId: Response = _responseCounter;
  _responseCounter = _responseCounter + 1;

  _requests = _requests + [[method, url, headers, body, _currentTime]];

  let status: number = STATUS_OK;
  let responseBody: string = "{}";
  let responseHeaders: string[] = ["Content-Type: application/json"];

  _responses = _responses + [[status, responseHeaders, responseBody, url, _currentTime]];

  return responseId;
}

// Gets response status code.
export function getStatus(response: Response): number {
  if (response < 0 || response >= _responses.length) { return -1; }
  return _responses[response][0];
}

// Gets response body.
export function getBody(response: Response): string {
  if (response < 0 || response >= _responses.length) { return ""; }
  return _responses[response][2];
}

// Gets response headers.
export function getHeaders(response: Response): string[] {
  if (response < 0 || response >= _responses.length) { return []; }

  let headers: string[] = [];
  let responseHeaders: string[] = _responses[response][1];
  let i: number = 0;
  while (i < responseHeaders.length) {
    headers = headers + [responseHeaders[i]];
    i = i + 1;
  }

  return headers;
}

// Gets a specific header value.
export function getHeader(response: Response, name: string): string {
  let headers: string[] = getHeaders(response);
  let i: number = 0;

  while (i < headers.length) {
    let header: string = headers[i];
    let colonIdx: number = indexOfString(header, ":");
    if (colonIdx >= 0) {
      let headerName: string = header.substring(0, colonIdx);
      if (headerName == name) {
        let value: string = header.substring(colonIdx + 1, header.length);
        if (value.length > 0 && value.substring(0, 1) == " ") {
          return value.substring(1, value.length);
        }
        return value;
      }
    }
    i = i + 1;
  }

  return "";
}

// Gets response URL.
export function getResponseUrl(response: Response): string {
  if (response < 0 || response >= _responses.length) { return ""; }
  return _responses[response][3];
}

// Gets response timestamp.
export function getResponseTime(response: Response): number {
  if (response < 0 || response >= _responses.length) { return -1; }
  return _responses[response][4];
}

// Checks if response status indicates success (2xx).
export function isSuccess(response: Response): boolean {
  let status: number = getStatus(response);
  return status >= 200 && status < 300;
}

// Checks if response status indicates client error (4xx).
export function isClientError(response: Response): boolean {
  let status: number = getStatus(response);
  return status >= 400 && status < 500;
}

// Checks if response status indicates server error (5xx).
export function isServerError(response: Response): boolean {
  let status: number = getStatus(response);
  return status >= 500 && status < 600;
}

// Checks if response is a redirect (3xx).
export function isRedirect(response: Response): boolean {
  let status: number = getStatus(response);
  return status >= 300 && status < 400;
}

// Encodes a string for URL use (percent encoding).
export function encodeURIComponent(str: string): string {
  let result: string = "";
  let i: number = 0;

  while (i < str.length) {
    let c: string = str.substring(i, i + 1);

    if (isAlphanumeric(c) || c == "-" || c == "_" || c == "." || c == "~") {
      result = result + c;
    } else if (c == " ") {
      result = result + "%20";
    } else if (c == "&") {
      result = result + "%26";
    } else if (c == "=") {
      result = result + "%3D";
    } else if (c == "+") {
      result = result + "%2B";
    } else if (c == "%") {
      result = result + "%25";
    } else {
      result = result + "%" + charToHex(c);
    }

    i = i + 1;
  }

  return result;
}

// Decodes a URL-encoded string.
export function decodeURIComponent(str: string): string {
  let result: string = "";
  let i: number = 0;

  while (i < str.length) {
    let c: string = str.substring(i, i + 1);

    if (c == "%") {
      if (i + 2 < str.length) {
        let hex: string = str.substring(i + 1, i + 3);
        result = result + hexToChar(hex);
        i = i + 3;
      } else {
        result = result + c;
        i = i + 1;
      }
    } else if (c == "+") {
      result = result + " ";
      i = i + 1;
    } else {
      result = result + c;
      i = i + 1;
    }
  }

  return result;
}

// Parses query string into key-value pairs.
export function parseQuery(query: string): [key: string, value: string][] {
  let result: [string, string][] = [];

  if (query.length == 0) { return result; }

  let i: number = 0;
  let current: string = "";
  let key: string = "";
  let inValue: boolean = false;

  while (i < query.length) {
    let c: string = query.substring(i, i + 1);

    if (c == "=") {
      key = current;
      current = "";
      inValue = true;
    } else if (c == "&") {
      if (key.length > 0) {
        result = result + [[key, current]];
      }
      key = "";
      current = "";
      inValue = false;
    } else {
      current = current + c;
    }

    i = i + 1;
  }

  if (key.length > 0 || current.length > 0) {
    result = result + [[key, current]];
  }

  return result;
}

// Formats query parameters into a query string.
export function formatQuery(params: [key: string, value: string][]): string {
  let result: string = "";
  let i: number = 0;

  while (i < params.length) {
    if (i > 0) {
      result = result + "&";
    }
    result = result + params[i][0] + "=" + params[i][1];
    i = i + 1;
  }

  return result;
}

// Gets all recorded requests (for testing).
export function getRequests(): RequestRecord[] {
  let result: RequestRecord[] = [];
  let i: number = 0;
  while (i < _requests.length) {
    result = result + [_requests[i]];
    i = i + 1;
  }
  return result;
}

// Gets request count.
export function requestCount(): number {
  return _requests.length;
}

// Gets response count.
export function responseCount(): number {
  return _responses.length;
}

// Clears all recorded requests and responses.
export function clearNetworkLog(): void {
  _requests = [];
  _responses = [];
  _responseCounter = 0;
}

// Sets simulated time for network operations.
export function setNetworkTime(time: number): void {
  _currentTime = time;
}

// Gets current network time.
export function getNetworkTime(): number {
  return _currentTime;
}

// Helper: Find index of substring.
function indexOfString(str: string, search: string): number {
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

// Helper: Convert string to number.
function stringToNumber(str: string): number {
  let result: number = 0;
  let i: number = 0;

  while (i < str.length) {
    let c: string = str.substring(i, i + 1);
    if (c >= "0" && c <= "9") {
      result = result * 10;
      result = result + (c.charCodeAt(0) - "0".charCodeAt(0));
    } else {
      return 0;
    }
    i = i + 1;
  }

  return result;
}

// Helper: Check if character is alphanumeric.
function isAlphanumeric(c: string): boolean {
  if (c >= "0" && c <= "9") { return true; }
  if (c >= "a" && c <= "z") { return true; }
  if (c >= "A" && c <= "Z") { return true; }
  return false;
}

// Helper: Convert character to hex.
function charToHex(c: string): string {
  let code: number = c.charCodeAt(0);
  let hex: string = "";

  let highNibble: number = code / 16;
  let lowNibble: number = code % 16;

  hex = hex + nibbleToHexChar(highNibble);
  hex = hex + nibbleToHexChar(lowNibble);

  return hex;
}

// Helper: Convert hex string to character.
function hexToChar(hex: string): string {
  if (hex.length != 2) { return ""; }

  let high: number = hexCharToNibble(hex.substring(0, 1));
  let low: number = hexCharToNibble(hex.substring(1, 2));

  let code: number = high * 16 + low;
  return String.fromCharCode(code);
}

// Helper: Convert nibble to hex character.
function nibbleToHexChar(nibble: number): string {
  if (nibble < 10) {
    return String.fromCharCode(48 + nibble);
  } else {
    return String.fromCharCode(55 + nibble);
  }
}

// Helper: Convert hex character to nibble.
function hexCharToNibble(c: string): number {
  if (c >= "0" && c <= "9") {
    return c.charCodeAt(0) - "0".charCodeAt(0);
  } else if (c >= "a" && c <= "f") {
    return c.charCodeAt(0) - "a".charCodeAt(0) + 10;
  } else if (c >= "A" && c <= "F") {
    return c.charCodeAt(0) - "A".charCodeAt(0) + 10;
  }
  return 0;
}

// Gets method name as string.
export function getMethodName(method: number): string {
  if (method == METHOD_GET) { return "GET"; }
  if (method == METHOD_POST) { return "POST"; }
  if (method == METHOD_PUT) { return "PUT"; }
  if (method == METHOD_DELETE) { return "DELETE"; }
  if (method == METHOD_PATCH) { return "PATCH"; }
  if (method == METHOD_HEAD) { return "HEAD"; }
  if (method == METHOD_OPTIONS) { return "OPTIONS"; }
  return "UNKNOWN";
}

// Gets status code name as string.
export function getStatusName(status: number): string {
  if (status == STATUS_OK) { return "OK"; }
  if (status == STATUS_CREATED) { return "Created"; }
  if (status == STATUS_ACCEPTED) { return "Accepted"; }
  if (status == STATUS_NO_CONTENT) { return "No Content"; }
  if (status == STATUS_BAD_REQUEST) { return "Bad Request"; }
  if (status == STATUS_UNAUTHORIZED) { return "Unauthorized"; }
  if (status == STATUS_FORBIDDEN) { return "Forbidden"; }
  if (status == STATUS_NOT_FOUND) { return "Not Found"; }
  if (status == STATUS_SERVER_ERROR) { return "Internal Server Error"; }
  if (status == STATUS_SERVICE_UNAVAILABLE) { return "Service Unavailable"; }
  return "Unknown";
}
