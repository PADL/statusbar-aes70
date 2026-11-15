[← back to module index](README.md)

# oca

AES70 data types as OCP.1 values: the AES70-2 types of the connection management classes
(media stream endpoints, network interfaces, counters, media clocks, time sources), and the
AES70-21 AES67 endpoint adaptation data an endpoint's AdaptationData blob holds.

## Overview

Each type is a plain struct or enumeration that [`ocp1`](OCP1_MODULE.md) encodes and decodes:
a record names its fields in `fields()` in AES70's order, an enumeration declares its AES70
width (OcaUint8 or OcaUint16), and the AES70 base types are aliases (`OcaONo` is a `uint32_t`,
`OcaTimeInterval` a `float` of seconds, `OcaBlob` an `ocp1::Blob`). A program passes them to
`ocp1::encode_values()` and reads them with `ocp1::decode_value<T>()`, alone or inside a
method's parameter tuple. The module holds only types and their encoding: no classes, objects
or behaviour. Members are snake_case; type names are AES70's.

## Key types

- `OcaONo`, `OcaID16`, `OcaFrequency`, `OcaTimeInterval`, `OcaDBFS`, `OcaMimeType`, `OcaBlob`, `OCA_INVALID_ONO` — base types.
- `OcaIODirection`, `OcaPortMode`, `OcaPortID`, `OcaSecurityType`.
- `OcaMediaStreamEndpoint`, `OcaMediaStreamEndpointID`, `OcaMediaStreamEndpointState`, `OcaMediaStreamEndpointCommand`, `OcaMediaStreamEndpointStatus`, `OcaMediaStreamMode`, `OcaMediaFrameFormat`, `OcaMediaStreamCastMode`, `OcaAdaptationData` — media stream endpoints.
- `OcaCounter`, `OcaCounterSet`, `OcaCounterSetID` — counters.
- `OcaNetworkInterfaceState`, `OcaNetworkInterfaceStatus`, `OcaIP4AutoconfigMode`, `OcaIP4Gateway`, `OcaIP4NetworkSettings` — network interfaces.
- `OcaMediaClockAvailability`, `OcaMediaClockRate` — media clocks.
- `OcaTimeSourceAvailability`, `OcaTimeSourceSyncStatus`, `OcaTimeProtocol`, `OcaTimeDeliveryMechanism`, `OcaTimeReferenceType` — time sources.
- `Aes67EndpointAdaptationData`, `Aes67EndpointIPParameters`, `Aes67MediaIDType`, `AES67_*_CAPABLE`, `encode_adaptation_data()`, `decode_adaptation_data()` — AES70-21's AES67 adaptation data.

## Quick example

```cpp
#include "padl/oca/oca.hpp"

// An AES67 output's AdaptationData: one destination on network assignment 1, payload type 98.
auto output_adaptation_data() -> statusbar::StatusValue<padl::oca::OcaAdaptationData>
{
    padl::oca::Aes67EndpointAdaptationData data{.payload_type = 98};
    data.ip_parameters.push_back({.network_assignment_id = 1, .destination_address = "239.69.5.1", .destination_port = 5004});
    return padl::oca::encode_adaptation_data(data);
}

// The stream mode of an encoded OcaMediaStreamEndpoint.
auto stream_mode(std::span<uint8_t const> endpoint) -> statusbar::StatusValue<padl::oca::OcaMediaStreamMode>
{
    auto const decoded = padl::ocp1::decode_value<padl::oca::OcaMediaStreamEndpoint>(endpoint);
    if (!decoded) {
        return statusbar::forward_failure(decoded);
    }
    return decoded->current_stream_mode;
}
```

## Headers

- `padl/oca/oca.hpp` — the umbrella: everything below.
- `padl/oca/oca_types.hpp` — the AES70-2 data types.
- `padl/oca/oca_aes67.hpp` — the AES70-21 AES67 adaptation data and its blob encoding.

## Dependencies

- **Statusbar modules:** [`ocp1`](OCP1_MODULE.md) (the encoding), `buffer` (error codes), `status`.
- **System / external:** `<string>`, `<vector>`, `<tuple>`.

## Notes & caveats

- Only types a published AES70 standard defines are here. The enumerations' values and the
  records' field orders are AES70-2023's: `OcaTimeDeliveryMechanism` has AES11 at 11 and the
  GNSS mechanisms 12 to 17, `OcaTimeReferenceType` has TAI at 3, `OcaTimeProtocol` has
  StreamEndpoint at 8.
- `OcaMediaStreamEndpoint`'s channel map is an OcaMultiMap<OcaUint16, OcaPortID>, stream
  channel to port; a channel may map to more than one port, and the map keeps wire order.
- `OcaMediaStreamEndpoint` has no `operator==`; compare the fields that matter.
- `OcaIP4NetworkSettings`' addresses are OcaStrings ("A.B.C.D/N" with a prefix), its additional
  parameters an OcaJsonValue (an OcaString, "{}" when there are none).
- The adaptation data is AES70-21's, with `Aes67MediaIDType` an OcaUint8 as AES70-3 marshals
  enumerations. `encode_adaptation_data()` fails with `too_long` when the record does not fit an
  OcaBlob (65535 bytes); `decode_adaptation_data()` refuses trailing bytes.
- `oca` has no error codes of its own: what fails is the encoding's (`Ocp1Error`, `BufferError`).

## Further reading

- AES70-2, Class structure — the data types, and the connection management classes using them
  (OcaMediaTransportApplication, OcaNetworkInterface, OcaMediaClock3, OcaTimeSource).
- AES70-3 — their marshalling ([OCP1_MODULE.md](OCP1_MODULE.md)).
- AES70-21 — the AES67 adaptation.
