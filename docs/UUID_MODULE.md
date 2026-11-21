[← back to module index](README.md)

# uuid

UUIDs (RFC 9562) as values: the 16 octets, the lower-case text form, and the name-based version 5
UUID that derives the same UUID from the same namespace and name on every run, with the SHA-1
(RFC 3174) it needs.

## Overview

`Uuid` holds a UUID's octets in network order; `text()` writes the 8-4-4-4-12 form in lower
case into a `UuidText` held in place, without allocating. `name_based_v5()` hashes the
namespace's octets then the name's (UTF-8, as given) with SHA-1 and keeps the first 16 octets,
version 5 and variant 10 set (RFC 9562 Section 5.5). A program that names its resources this way,
as AMWA NMOS IS-04 resources are, gives the same IDs after every restart, and any other
implementation of Section 5.5 (Python's `uuid.uuid5`, SwiftNMOS's `UUID(version5:namespace:)`) derives the
same.
`Sha1` is incremental (`update()` in pieces, `finish()` once); `sha1()` hashes one message.

## Key types

- `Uuid` — the octets; `version()`, `text()`, `operator==`.
- `UuidText` — the text form's 36 characters; `view()`.
- `name_based_v5()` — the version 5 UUID of a name in a namespace.
- `NAMESPACE_DNS`, `NAMESPACE_URL` — RFC 9562 Section 6.6's namespace IDs.
- `Sha1`, `sha1()`, `Sha1Digest` — SHA-1.

## Quick example

```cpp
#include "padl/uuid/uuid.hpp"

// The same ID for "www.example.com" every run: 2ed6657d-e927-568b-95e1-2665a8aea6a2.
auto example_id() -> std::string
{
    auto const id = padl::uuid::name_based_v5(padl::uuid::NAMESPACE_DNS, "www.example.com");
    return std::string{id.text().view()};
}
```

## Headers

- `padl/uuid/uuid.hpp` — the umbrella: everything below.
- `padl/uuid/uuid_value.hpp` — `Uuid`, `UuidText`, `name_based_v5()`, the namespace IDs.
- `padl/uuid/uuid_sha1.hpp` — `Sha1`, `sha1()`.

## Dependencies

- **Statusbar modules:** `buffer` (`make_const_span()`), `ieee` (`quadlet_t`, `octlet_t` for the big-endian words).
- **System / external:** none.

## Notes & caveats

- SHA-1 is here for name-based UUIDs only. It is broken for collision resistance (RFC 6194): never use
  it for integrity or authentication. RFC 9562 shows version 8 with SHA-256 for name-based IDs
  (Appendix B.2), but schemas that pattern IDs as versions 1 to 5 (AMWA IS-04) admit only 3 and 5.
- There is no parser for the text form: nothing here reads UUIDs from outside, and a parser would
  need its own fuzzer.
- The tests check RFC 3174 Section 7.3's vectors, the padding's edges (55, 56 and 64 octets),
  RFC 9562 Appendix A.4's version 5 example and Python's `uuid.uuid5`.

## Further reading

- RFC 9562 — Universally Unique IDentifiers (UUIDs); Section 5.5 (version 5), Section 6.6 (namespace IDs).
- RFC 3174 — US Secure Hash Algorithm 1 (SHA1); RFC 6194 — security considerations for SHA-1.
