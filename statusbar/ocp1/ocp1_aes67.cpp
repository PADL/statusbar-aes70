// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "statusbar/ocp1/ocp1_aes67.hpp"

#include "statusbar/buffer/buffer_error.hpp"

#include <system_error>

namespace statusbar::ocp1 {

auto encode_adaptation_data(Aes67EndpointAdaptationData const& data) -> StatusValue<Blob>
{
    Blob blob{.bytes = std::vector<uint8_t>(MAX_COUNT)};
    auto const length = encode_values(blob.bytes, data);
    if (!length && length.error() == make_error_code(BufferError::insufficient_space)) {
        return failure(Ocp1Error::too_long);
    }
    if (!length) {
        return forward_failure(length);
    }
    blob.bytes.resize(*length);
    return blob;
}

auto decode_adaptation_data(std::span<uint8_t const> const bytes) -> StatusValue<Aes67EndpointAdaptationData>
{
    return decode_value<Aes67EndpointAdaptationData>(bytes);
}

}  // namespace statusbar::ocp1
