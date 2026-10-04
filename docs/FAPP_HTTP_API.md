# fScript HTTP API – Bytecode 1.5

fOS 4.0.0 exposes bounded HTTP/HTTPS GET access to fScript applications. Every
app using it must declare `network` in the `permissions` array of `app.json`.
Without that permission, the VM terminates only the requesting app with a
permission error.

## Functions

| Function | Result | Purpose |
| --- | --- | --- |
| `http_get(url)` | Boolean | Downloads one HTTP/HTTPS response |
| `http_status()` | Integer | Returns the last HTTP status or client error code |
| `http_json(path)` | String | Reads a scalar through paths such as `results[0].name` |
| `http_text(offset)` | String | Returns a response chunk beginning at a byte offset |
| `url_encode(text)` | String | Percent-encodes text for query parameters |
| `date_weekday(date)` | String | Converts `YYYY-MM-DD` to `Sun` through `Sat` |

Only the most recent response is retained per runtime. Missing JSON paths
return an empty string, while malformed non-scalar values raise a contained app
error. JSON strings, numbers, booleans and `null` are supported as scalar
results.

## Limits and security

* HTTP responses: 32768 bytes
* fScript string value: 255 bytes plus terminator
* connection timeout: 5 seconds
* read timeout: 7 seconds
* schemes: `http://` and `https://`
* HTTPS currently uses the same compatibility mode as the previous native
  weather implementation (`setInsecure`); certificate-chain verification is
  therefore not yet enforced
* HTTP calls are synchronous and may pause the UI until the bounded timeout
* OTA listing and installation have network priority; fScript HTTP returns
  `false` with `OTA network operation is active` instead of opening a second
  simultaneous TLS connection

An app that starts during the short OTA listing window should retry later (for
example from `app.timer`). The weather example retries once per minute until
its first successful response, then returns to its normal 15-minute interval.

Native calls have their own `CALL_NATIVE` bytecode instruction. The compiler
checks names, arity and argument types; the verifier checks function IDs and
operand stack behavior; the runtime checks permissions again before dispatch.

`app.timer` is available in bytecode 1.5. fOS enqueues it once per minute
when a handler exists. The source weather app counts 15 events before refreshing
Open-Meteo data.

Bytecode 1.5 can change this compatible default with
`timer.start(milliseconds)`. The additional native APIs and their permission
rules are documented in `FOS_API_STEP19.md`.

Bytecode 1.5 additionally exposes keyboard `ready` and `cancel` events. They
let a script react separately to the checkmark and cancel keys without changing
the behavior of ordinary button events.
