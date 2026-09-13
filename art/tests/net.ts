import { Response, URL, parseUrl, formatUrl, get, post, put, deleteUrl, patch, request, getStatus, getBody, getHeaders, getHeader, getResponseUrl, getResponseTime, isSuccess, isClientError, isServerError, isRedirect, encodeURIComponent, decodeURIComponent, parseQuery, formatQuery, getRequests, requestCount, responseCount, clearNetworkLog, setNetworkTime, getNetworkTime, getMethodName, getStatusName, METHOD_GET, METHOD_POST, METHOD_PUT, METHOD_DELETE, METHOD_PATCH, STATUS_OK, STATUS_CREATED, STATUS_BAD_REQUEST, STATUS_NOT_FOUND, STATUS_SERVER_ERROR } from "art/net";

function testParseUrlSimple(): number {
  let url: URL = parseUrl("http://example.com/path");
  if (url[0] != "http") { return 1; }
  if (url[1] != "example.com") { return 2; }
  if (url[2] != 80) { return 3; }
  if (url[3] != "/path") { return 4; }
  return 0;
}

function testParseUrlWithPort(): number {
  let url: URL = parseUrl("http://example.com:8080/path");
  if (url[1] != "example.com") { return 1; }
  if (url[2] != 8080) { return 2; }
  return 0;
}

function testParseUrlWithQuery(): number {
  let url: URL = parseUrl("http://example.com/path?key=value");
  if (url[3] != "/path") { return 1; }
  if (url[4] != "key=value") { return 2; }
  return 0;
}

function testParseUrlHttps(): number {
  let url: URL = parseUrl("https://example.com/");
  if (url[0] != "https") { return 1; }
  if (url[2] != 443) { return 2; }
  return 0;
}

function testFormatUrl(): number {
  let url: URL = ["http", "example.com", 80, "/path", ""];
  let formatted: string = formatUrl(url);
  if (formatted != "http://example.com/path") { return 1; }
  return 0;
}

function testFormatUrlWithQuery(): number {
  let url: URL = ["http", "example.com", 80, "/path", "key=value"];
  let formatted: string = formatUrl(url);
  if (formatted != "http://example.com/path?key=value") { return 1; }
  return 0;
}

function testGetRequest(): number {
  clearNetworkLog();
  let response: Response = get("http://example.com/api");
  if (getStatus(response) != STATUS_OK) { return 1; }
  return 0;
}

function testPostRequest(): number {
  clearNetworkLog();
  let response: Response = post("http://example.com/api", "{\"key\":\"value\"}");
  if (getStatus(response) != STATUS_OK) { return 1; }
  return 0;
}

function testPutRequest(): number {
  clearNetworkLog();
  let response: Response = put("http://example.com/api/1", "{\"updated\":true}");
  if (getStatus(response) != STATUS_OK) { return 1; }
  return 0;
}

function testDeleteRequest(): number {
  clearNetworkLog();
  let response: Response = deleteUrl("http://example.com/api/1");
  if (getStatus(response) != STATUS_OK) { return 1; }
  return 0;
}

function testPatchRequest(): number {
  clearNetworkLog();
  let response: Response = patch("http://example.com/api/1", "{\"field\":\"value\"}");
  if (getStatus(response) != STATUS_OK) { return 1; }
  return 0;
}

function testGetResponseBody(): number {
  clearNetworkLog();
  let response: Response = get("http://example.com/api");
  let body: string = getBody(response);
  if (body != "{}") { return 1; }
  return 0;
}

function testIsSuccess(): number {
  clearNetworkLog();
  let response: Response = get("http://example.com/api");
  if (!isSuccess(response)) { return 1; }
  return 0;
}

function testIsClientError(): number {
  clearNetworkLog();
  let response: Response = request(METHOD_GET, "http://example.com/api", [], "");
  if (isClientError(response)) { return 1; }
  return 0;
}

function testIsServerError(): number {
  clearNetworkLog();
  let response: Response = request(METHOD_GET, "http://example.com/api", [], "");
  if (isServerError(response)) { return 1; }
  return 0;
}

function testEncodeURIComponent(): number {
  let encoded: string = encodeURIComponent("hello world");
  if (encoded != "hello%20world") { return 1; }

  let encoded2: string = encodeURIComponent("a&b=c");
  if (encoded2 != "a%26b%3Dc") { return 2; }

  return 0;
}

function testDecodeURIComponent(): number {
  let decoded: string = decodeURIComponent("hello%20world");
  if (decoded != "hello world") { return 1; }

  let decoded2: string = decodeURIComponent("a%26b%3Dc");
  if (decoded2 != "a&b=c") { return 2; }

  return 0;
}

function testParseQuerySimple(): number {
  let params: [string, string][] = parseQuery("key1=value1&key2=value2");
  if (params.length != 2) { return 1; }
  if (params[0][0] != "key1" || params[0][1] != "value1") { return 2; }
  if (params[1][0] != "key2" || params[1][1] != "value2") { return 3; }
  return 0;
}

function testParseQueryEmpty(): number {
  let params: [string, string][] = parseQuery("");
  if (params.length != 0) { return 1; }
  return 0;
}

function testFormatQuery(): number {
  let params: [string, string][] = [["key1", "value1"], ["key2", "value2"]];
  let query: string = formatQuery(params);
  if (query != "key1=value1&key2=value2") { return 1; }
  return 0;
}

function testRequestCount(): number {
  clearNetworkLog();
  if (requestCount() != 0) { return 1; }

  get("http://example.com");
  if (requestCount() != 1) { return 2; }

  get("http://example.com");
  if (requestCount() != 2) { return 3; }

  return 0;
}

function testResponseCount(): number {
  clearNetworkLog();
  if (responseCount() != 0) { return 1; }

  get("http://example.com");
  if (responseCount() != 1) { return 2; }

  return 0;
}

function testGetResponseUrl(): number {
  clearNetworkLog();
  let url: string = "http://example.com/api/test";
  let response: Response = get(url);
  if (getResponseUrl(response) != url) { return 1; }
  return 0;
}

function testNetworkTime(): number {
  clearNetworkLog();
  setNetworkTime(0);
  if (getNetworkTime() != 0) { return 1; }

  setNetworkTime(1000);
  if (getNetworkTime() != 1000) { return 2; }

  return 0;
}

function testGetMethodName(): number {
  if (getMethodName(METHOD_GET) != "GET") { return 1; }
  if (getMethodName(METHOD_POST) != "POST") { return 2; }
  if (getMethodName(METHOD_PUT) != "PUT") { return 3; }
  if (getMethodName(METHOD_DELETE) != "DELETE") { return 4; }
  if (getMethodName(METHOD_PATCH) != "PATCH") { return 5; }
  return 0;
}

function testGetStatusName(): number {
  if (getStatusName(STATUS_OK) != "OK") { return 1; }
  if (getStatusName(STATUS_CREATED) != "Created") { return 2; }
  if (getStatusName(STATUS_BAD_REQUEST) != "Bad Request") { return 3; }
  if (getStatusName(STATUS_NOT_FOUND) != "Not Found") { return 4; }
  if (getStatusName(STATUS_SERVER_ERROR) != "Internal Server Error") { return 5; }
  return 0;
}

function testMultipleRequests(): number {
  clearNetworkLog();

  let r1: Response = get("http://example.com/1");
  let r2: Response = post("http://example.com/2", "body");
  let r3: Response = put("http://example.com/3", "body");

  if (requestCount() != 3) { return 1; }
  if (responseCount() != 3) { return 2; }

  if (getStatus(r1) != STATUS_OK) { return 3; }
  if (getStatus(r2) != STATUS_OK) { return 4; }
  if (getStatus(r3) != STATUS_OK) { return 5; }

  return 0;
}

function testParseQuerySingle(): number {
  let params: [string, string][] = parseQuery("only=value");
  if (params.length != 1) { return 1; }
  if (params[0][0] != "only") { return 2; }
  if (params[0][1] != "value") { return 3; }
  return 0;
}

function testParseUrlNoQuery(): number {
  let url: URL = parseUrl("http://example.com/path");
  if (url[4] != "") { return 1; }
  return 0;
}

function testEncodeSpecialChars(): number {
  let encoded: string = encodeURIComponent("test@example.com");
  if (encoded.length == 0) { return 1; }
  return 0;
}

function testDecodePercentage(): number {
  let decoded: string = decodeURIComponent("100%25");
  if (decoded != "100%") { return 1; }
  return 0;
}

function testGetHeaders(): number {
  clearNetworkLog();
  let response: Response = post("http://example.com/api", "body");
  let headers: string[] = getHeaders(response);
  if (headers.length == 0) { return 1; }
  return 0;
}

function testInvalidResponse(): number {
  clearNetworkLog();
  let invalidResponse: Response = 999;
  if (getStatus(invalidResponse) != -1) { return 1; }
  if (getBody(invalidResponse) != "") { return 2; }
  if (getResponseUrl(invalidResponse) != "") { return 3; }
  return 0;
}

function testResponseTime(): number {
  clearNetworkLog();
  setNetworkTime(100);
  let response: Response = get("http://example.com/api");
  if (getResponseTime(response) != 100) { return 1; }
  return 0;
}

function testParseUrlRootPath(): number {
  let url: URL = parseUrl("http://example.com");
  if (url[3] != "" && url[3] != "/") { return 1; }
  return 0;
}

function testCustomRequest(): number {
  clearNetworkLog();
  let headers: string[] = ["Authorization: Bearer token", "Content-Type: application/json"];
  let response: Response = request(METHOD_GET, "http://example.com/api", headers, "");
  if (getStatus(response) != STATUS_OK) { return 1; }
  return 0;
}
