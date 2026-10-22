# statusbar-aes70

The OCP.1 wire encoding of [AES70](https://www.aes.org/standards/) (AES70-3), for the
[statusbar](https://codeberg.org/statusbar) stack. It is a package of the statusbar umbrella
and depends on `statusbar-core`.

**This is not a full AES70 implementation.** There is no object model, no device or
controller, no subscription manager, no keep-alive handling and no OCP.1 transport. It
provides only the encoding, for programs that speak OCP.1 messages over a channel of their
own, such as a private control socket. For a complete AES70 stack, see
[SwiftOCA](https://github.com/PADL/SwiftOCA).

| Module | What it is |
| --- | --- |
| `ocp1` | AES70-3 values (integers, floats, booleans, strings counted in Unicode scalars, blobs, lists, maps, intervals, and records through `fields()`) with an `Encoder` and `Decoder` over statusbar's buffer builders; PDU framing and the Command, Response and Notification2 messages |

Status: early development.
