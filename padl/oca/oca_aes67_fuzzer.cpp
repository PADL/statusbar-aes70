// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// libFuzzer harness for decode_adaptation_data: it may not crash or read out of bounds, and a blob
/// that decodes encodes to the same bytes.

#include "padl/oca/oca.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(uint8_t const* data, size_t size)
{
    std::span<uint8_t const> const bytes{data, size};
    auto const decoded = padl::oca::decode_adaptation_data(bytes);
    if (!decoded) {
        return 0;
    }
    auto const blob = padl::oca::encode_adaptation_data(*decoded);
    if (!blob || !std::ranges::equal(blob->bytes, bytes)) {
        std::abort();
    }
    return 0;
}
