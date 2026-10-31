#pragma once

// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// The AES70-21 AES67 adaptation records an OcaMediaStreamEndpoint's AdaptationData holds,
/// as OCP.1 values: Aes67EndpointIPParameters and Aes67EndpointAdaptationData.

#include "statusbar/ocp1/ocp1_codec.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace statusbar::ocp1 {

/// Aes67StreamTransmissionCapabilities (AES70-21), bits of Aes67EndpointAdaptationData.TransmissionCapabilities.
inline constexpr uint16_t AES67_UNICAST_CAPABLE = 1U << 0U;
inline constexpr uint16_t AES67_MULTICAST_CAPABLE = 1U << 1U;
inline constexpr uint16_t AES67_INSECURE_CAPABLE = 1U << 2U;
inline constexpr uint16_t AES67_DEFAULT_SECURITY_CAPABLE = 1U << 3U;

/// Aes67EndpointIPParameters (AES70-21): an endpoint's addresses on one
/// network assignment; a DSCP or COS of -1 leaves it to the device.
struct Aes67EndpointIPParameters
{
    uint16_t network_assignment_id{0};
    std::string source_address;
    std::string destination_address;
    uint8_t time_to_live{0};
    uint16_t source_port{0};
    uint16_t destination_port{0};
    int8_t dscp{-1};
    int8_t cos{-1};

    auto fields(this auto& self) noexcept
    {
        return std::tie(
            self.network_assignment_id,
            self.source_address,
            self.destination_address,
            self.time_to_live,
            self.source_port,
            self.destination_port,
            self.dscp,
            self.cos);
    }
    auto operator==(Aes67EndpointIPParameters const&) const -> bool = default;
};

/// Aes67EndpointAdaptationData (AES70-21): what an AES67 endpoint's AdaptationData blob holds,
/// fields in the standard's order; the time intervals are seconds.
struct Aes67EndpointAdaptationData
{
    std::vector<Aes67EndpointIPParameters> ip_parameters;
    uint8_t payload_type{0};
    uint32_t rtp_timestamp_offset{0};
    float time_skew_max_tolerated{0};
    Blob remote_endpoint_name;
    std::string remote_control_type;
    uint16_t transmission_capabilities{0};  ///< AES67_*_CAPABLE bits
    float presentation_time_offset{0};
    std::string media_info;
    std::string active_sdp;
    uint32_t stream_id{0};

    auto fields(this auto& self) noexcept
    {
        return std::tie(
            self.ip_parameters,
            self.payload_type,
            self.rtp_timestamp_offset,
            self.time_skew_max_tolerated,
            self.remote_endpoint_name,
            self.remote_control_type,
            self.transmission_capabilities,
            self.presentation_time_offset,
            self.media_info,
            self.active_sdp,
            self.stream_id);
    }
    auto operator==(Aes67EndpointAdaptationData const&) const -> bool = default;
};

/// `data` as an AdaptationData blob; too_long when it does not fit one (65535 bytes).
[[nodiscard]] auto encode_adaptation_data(Aes67EndpointAdaptationData const& data) -> StatusValue<Blob>;

/// The record a whole AdaptationData blob's `bytes` hold; trailing bytes are an error.
[[nodiscard]] auto decode_adaptation_data(std::span<uint8_t const> bytes) -> StatusValue<Aes67EndpointAdaptationData>;

}  // namespace statusbar::ocp1
