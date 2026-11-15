// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// libFuzzer harness for the OCP.1 value decoders: the first byte picks a string (the UTF-8 walk), a
/// list of records or a map, the rest is the value. Decoding may not crash or read out of bounds, and
/// a value that decodes encodes to the same bytes.

#include "padl/ocp1/ocp1.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

namespace {

/// `T` from the whole of `bytes`, which, when it decodes, encodes to `bytes` again.
template <typename T>
void check(std::span<uint8_t const> const bytes)
{
    auto const value = padl::ocp1::decode_value<T>(bytes);
    if (!value) {
        return;
    }
    std::vector<uint8_t> out(bytes.size());
    auto const length = padl::ocp1::encode_values(out, *value);
    if (!length || *length != bytes.size() || !std::equal(out.begin(), out.end(), bytes.begin())) {
        std::abort();
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(uint8_t const* data, size_t size)
{
    if (size == 0) {
        return 0;
    }
    std::span<uint8_t const> const bytes{data + 1, size - 1};
    switch (data[0] % 4) {
        case 0:
            check<std::string>(bytes);
            break;
        case 1:
            check<std::vector<padl::ocp1::Event>>(bytes);
            break;
        case 2:
            check<std::vector<padl::ocp1::Interval<float>>>(bytes);
            break;
        default:
            check<padl::ocp1::Map<uint16_t, std::string>>(bytes);
            break;
    }
    return 0;
}
