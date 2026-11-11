// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "padl/ocp1/ocp1_aes67.hpp"

#include "statusbar/buffer/buffer_error.hpp"

#include <system_error>

namespace padl::ocp1 {

auto encode_adaptation_data(Aes67EndpointAdaptationData const& data) -> statusbar::StatusValue<Blob>
{
    Blob blob{.bytes = std::vector<uint8_t>(MAX_COUNT)};
    auto const length = encode_values(blob.bytes, data);
    if (!length && length.error() == make_error_code(statusbar::BufferError::insufficient_space)) {
        return statusbar::failure(Ocp1Error::too_long);
    }
    if (!length) {
        return statusbar::forward_failure(length);
    }
    blob.bytes.resize(*length);
    return blob;
}

auto decode_adaptation_data(std::span<uint8_t const> const bytes) -> statusbar::StatusValue<Aes67EndpointAdaptationData>
{
    return decode_value<Aes67EndpointAdaptationData>(bytes);
}

}  // namespace padl::ocp1
