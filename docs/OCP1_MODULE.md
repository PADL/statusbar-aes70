[← back to module index](README.md)

# ocp1

The AES70-3 (OCP.1) encoding: AES70 values written and read through statusbar's buffer
builders, and PDUs holding one Command, Response or Notification2, framed off a byte stream,
parsed and serialized.

## Overview

`ocp1` is the wire half of AES70 and nothing more: no object model, device, controller,
subscriptions, keep-alives or transport. A program that speaks OCP.1 over a channel of its own
(a TCP or Unix stream, say) frames the stream with `next_pdu()`, parses each PDU with
`parse_pdu()`, decodes the parameters with a `Decoder`, encodes its answer with an `Encoder` and
writes it with `serialize_pdu()`. Errors are values: `Ocp1Error` (category `padl.ocp1`) for
what OCP.1 refuses, statusbar's `BufferError` for a value that runs past its input or does not
fit its buffer. Encoders and decoders are sticky: after the first error nothing more is written
or read, and `status()` reports it.

## Key types

- `Encoder`, `Decoder` — write values into a caller's buffer, read them from a span; `put()` / `get()` chain.
- `encode_values()`, `decode_value<T>()` — values into a buffer; a `T` that is the whole of its input.
- `Blob`, `LongBlob`, `Map<K, V>`, `Interval<T>`, `interval_bounds` — OcaBlob, OcaLongBlob, OcaMap and OcaMultiMap, OcaInterval.
- `Record`, `Scalar`, `Enumeration` — the concepts the codec dispatches on.
- `MessageType`, `OcaStatus`, `NotificationType`, `MethodId`, `EventId`, `Event`.
- `Command`, `Response`, `Notification`, `Message`, `Parameters` — the messages, viewing the PDU they were parsed from.
- `PduHeader`, `CommandHeader`, `ResponseHeader`, `Notification2Header` — the packed wire headers.
- `pdu_size()`, `next_pdu()`, `command_handle()`, `parse_pdu()`, `serialized_pdu_size()`, `serialize_pdu()`.
- `status_for()` — the OcaStatus answering a command that failed with an `error_code`.
- `Ocp1Error`, `ocp1_error_category()`.

## Quick example

```cpp
#include "padl/ocp1/ocp1.hpp"

#include <array>
#include <string>
#include <tuple>

// A command to method {3, 2} of ONo 4096 whose parameters are an OcaUint32 and an OcaString,
// written into `out` as a PDU.
auto command_pdu(std::span<uint8_t> out) -> statusbar::StatusValue<size_t>
{
    std::array<uint8_t, 64> parameters{};
    auto const length = padl::ocp1::encode_values(parameters, std::tuple{uint32_t{1001}, std::string{"Stage left"}});
    if (!length) {
        return statusbar::forward_failure(length);
    }
    padl::ocp1::Command const command{
        .handle = 1,
        .target = 4096,
        .method = {.def_level = 3, .index = 2},
        .parameters = {.count = 2, .data = std::span{parameters}.first(*length)}};
    return padl::ocp1::serialize_pdu(command, out);
}

// The parameters of the command at the front of `stream`, once it has all arrived.
auto parameters_of_next(std::span<uint8_t const> stream) -> statusbar::StatusValue<std::tuple<uint32_t, std::string>>
{
    auto const pdu = padl::ocp1::next_pdu(stream, 65536);
    if (!pdu || pdu->empty()) {
        return statusbar::failure(std::errc::resource_unavailable_try_again);
    }
    auto const message = padl::ocp1::parse_pdu(*pdu);
    auto const* command = message ? std::get_if<padl::ocp1::Command>(&*message) : nullptr;
    if (command == nullptr) {
        return statusbar::failure(std::errc::bad_message);
    }
    return padl::ocp1::decode_value<std::tuple<uint32_t, std::string>>(command->parameters.data);
}
```

## Headers

- `padl/ocp1/ocp1.hpp` — the umbrella: everything below.
- `padl/ocp1/ocp1_codec.hpp` — `Encoder`, `Decoder`, the value types and their `encode()` / `decode()`.
- `padl/ocp1/ocp1_pdu.hpp` — the messages, the wire headers, framing, parsing, serializing, `status_for()`.
- `padl/ocp1/ocp1_error.hpp` — `Ocp1Error` and its category.

## Dependencies

- **Statusbar modules:** `buffer` (builders, `BufferError`), `ieee` (wire integers), `status`.
- **System / external:** `<span>`, `<string>`, `<vector>`, `<tuple>`, `<variant>`.

## Notes & caveats

- **Values.** Big-endian and untagged, one after another. An integer or enumeration is its
  fixed-width bits (an enumeration its declared base, OcaUint8 or OcaUint16; an enum without
  one is refused at compile time), a boolean one octet (any nonzero octet reads as true), a
  float its IEEE 754 bits. An OcaString is a 16-bit count of Unicode scalars, then its UTF-8:
  the byte length is known only once the scalars are walked, and a string that is not
  well-formed UTF-8 (RFC 3629: no overlong form, surrogate or value past U+10FFFF) is
  `invalid_utf8`. An OcaBlob is a 16-bit length and its bytes, an OcaLongBlob a 32-bit one. An
  OcaList is a 16-bit count and its elements; an OcaMap or OcaMultiMap a 16-bit count of
  key-value pairs, kept in wire order (a repeated key in an OcaMap is not refused). A count past
  65535 is `too_long`. An OcaInterval keeps an omitted bound's place on the wire.
- **Records.** A struct is a record by naming its fields in a `fields()` member returning
  `std::tie(...)`, and is then its fields in order. A `std::tuple` or `std::pair` is a record of
  its elements, which is what a method's parameters are: `encode_values(buffer, tuple)` and
  `decode_value<Tuple>()` need nothing more. Any other type provides `encode(Encoder&, T const&)`
  and `decode(Decoder&, T&)` in its own namespace, using `writer()`, `reader()`, `consume()` and
  `fail()`.
- **Decoding** reserves no more list elements than the input could hold (every element is at
  least a byte), so a count of 65535 over a short input costs nothing. `decode_value()` refuses
  trailing bytes (`trailing_data`).
- **PDUs.** A PDU is syncVal 0x3B, then Ocp1Header: protocolVersion (OcaUint16), pduSize
  (OcaUint32, the bytes after syncVal), pduType (OcaMessageType) and messageCount (OcaUint16),
  then the messages:

  | Message       | Header                                                                                         | Then       |
  |---------------|------------------------------------------------------------------------------------------------|------------|
  | Command       | commandSize u32, handle u32, targetONo u32, methodID {defLevel u16, methodIndex u16}, parameterCount u8 | parameters |
  | Response      | responseSize u32, handle u32, statusCode u8, parameterCount u8                                  | parameters |
  | Notification2 | notificationSize u32, emitterONo u32, eventID {defLevel u16, eventIndex u16}, notificationType u8 | data       |

  A message's size counts its own bytes, its size field included. PDUs are written with
  protocolVersion 1; any version from 1 is read.
- **What is refused.** Only PDUs holding exactly one Command, Response or Notification2 are
  read or written: keep-alives and Notification1 belong to a full OCP.1 connection, which this
  module is not. A status code or notification type AES70 does not define, a parameter count
  the parameters cannot hold (every value is at least a byte), and a Notification2 with no data
  (an event carries its parameters, an exception its Ocp1Notification2ExceptionData) are errors
  both ways. Parsed parameters and data view the PDU; `serialize_pdu()` refuses parameters lying
  in its own output buffer (`aliased_parameters`), since the headers are written over them.
- **Framing.** `next_pdu()` returns the PDU at the front of a stream once all of it has arrived
  and fails when the stream cannot be framed (a sync other than 0x3B, a size below the header or
  above the reader's maximum); the reader then closes the stream. `command_handle()` reads the
  handle of a command wanting a response whose headers parse though its parameters do not, so a
  device can answer it with badFormat.
- **Status codes.** `status_for()` answers an `Ocp1Error` or a short input with badFormat, with
  three exceptions: unsupported_version answers protocolVersionError, too_long bufferOverflow (as
  does a value too long for its buffer), and aliased_parameters, the writer's own fault,
  deviceError. Anything else answers deviceError.
- The headers are packed structs of `ieee` integers with `static_assert`s on their size and
  every offset, parsed and written through `buffer`'s builders.

## Further reading

- AES70-3, OCP.1: Protocol for IP networks — Ocp1Header, Ocp1Command, Ocp1Response,
  Ocp1Notification2 and the marshalling of the AES70 data types.
- AES70-2, Class structure — OcaStatus, OcaMethodID, OcaEventID.
- RFC 3629 — UTF-8.
- [OCA_MODULE.md](OCA_MODULE.md) — the data types built on this encoding.
