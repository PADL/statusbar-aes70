# statusbar-aes70

The OCP.1 wire encoding of [AES70](https://www.aes.org/standards/) (AES70-3) and the AES70
data types built on it, for the [statusbar](https://codeberg.org/statusbar) stack. It is a
package of the statusbar umbrella and depends on `statusbar-core`.

**This is not a full AES70 implementation.** There is no object model, no device or
controller, no subscription manager, no keep-alive handling and no OCP.1 transport. It
provides only the encoding, for programs that speak OCP.1 messages over a channel of their
own, such as a private control socket. For a complete AES70 stack, see
[SwiftOCA](https://github.com/PADL/SwiftOCA).

| Module | What it is |
| --- | --- |
| `ocp1` | AES70-3 values (integers, floats, booleans, strings counted in Unicode scalars, blobs, lists, maps, intervals, and records through `fields()`, tuples and pairs) with an `Encoder` and `Decoder` over statusbar's buffer builders; PDU framing (`next_pdu`), parsing and serializing of the Command, Response and Notification2 messages; `status_for` |
| `oca` | The AES70-2 data types of the connection management classes (media stream endpoints and modes, ports, counters, network interfaces and IPv4 settings, media clock rates, time sources) as OCP.1 values; the AES70-21 AES67 endpoint adaptation data (`Aes67EndpointAdaptationData`) |

Each module is described in [docs](docs/README.md).

Status: early development.
