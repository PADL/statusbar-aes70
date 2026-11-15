// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "padl/oca/oca_types.hpp"

#include "statusbar/test/test.hpp"
#include "statusbar/test/test_util.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

using namespace padl::oca;

namespace {

/// `value` encoded into a buffer large enough for the tests
template <typename T>
auto encoded(T const& value) -> std::vector<uint8_t>
{
    std::array<uint8_t, 256> buffer{};
    auto const length = padl::ocp1::encode_values(buffer, value);
    EXPECT_TRUE(length.has_value());
    return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(length.value_or(0))};
}

/// The bytes `hex` spells, two hex digits a byte
template <size_t L>
auto from_hex(char const (&hex)[L]) -> std::vector<uint8_t>
{
    auto const array = hex_to_bytes<(L - 1) / 2>(hex);
    return {array.begin(), array.end()};
}

/// An AAF input endpoint, and the bytes SwiftOCA's encoding tests give it
auto aaf_input() -> OcaMediaStreamEndpoint
{
    return {
        .id_internal = 1,
        .id_external = {.bytes = {0xAB, 0xCD}},
        .direction = OcaIODirection::input,
        .user_label = "in",
        .network_assignment_ids = {1},
        .channel_map_dynamic = true,
        .channel_map = {{{1, {.mode = OcaPortMode::output, .index = 1}}}},
        .alignment_level = -20.0F,
        .current_stream_mode =
            {.frame_format = OcaMediaFrameFormat::aaf,
             .encoding_type = "audio/L32",
             .sampling_rate = 48000.0F,
             .channel_count = 8,
             .packet_time = 125e-6F},
        .stream_cast_mode = OcaMediaStreamCastMode::multicast,
    };
}

constexpr char AAF_INPUT[] = "00000001"                // idInternal
                             "0002abcd"                // idExternal
                             "01"                      // direction
                             "0002696e"                // userLabel
                             "00010001"                // networkAssignmentIDs
                             "0000"                    // streamModeCapabilityIDs
                             "00000000"                // clockONo
                             "01"                      // channelMapDynamic
                             "00010001020001"          // channelMap
                             "c1a00000"                // alignmentLevel
                             "02"                      // frameFormat
                             "0009617564696f2f4c3332"  // encodingType
                             "473b8000"                // samplingRate
                             "0008"                    // channelCount
                             "3903126f"                // packetTime
                             "00"                      // securityType
                             "02"                      // streamCastMode
                             "0000"                    // adaptationData
                             "0000";                   // redundantSetID

}  // namespace

TEST(oca_types, an_endpoint_encodes_as_aes70_marshals_it)
{
    EXPECT_EQ(encoded(aaf_input()), from_hex(AAF_INPUT));
    auto const decoded = padl::ocp1::decode_value<OcaMediaStreamEndpoint>(from_hex(AAF_INPUT));
    EXPECT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded && decoded->current_stream_mode == aaf_input().current_stream_mode);
    EXPECT_TRUE(decoded && decoded->channel_map == aaf_input().channel_map);
}

TEST(oca_types, enumerations_have_their_aes70_widths)
{
    EXPECT_EQ(encoded(OcaTimeDeliveryMechanism::aes11), (std::vector<uint8_t>{11}));
    EXPECT_EQ(encoded(OcaTimeReferenceType::tai), (std::vector<uint8_t>{3}));
    EXPECT_EQ(encoded(OcaMediaStreamEndpointState::connected), (std::vector<uint8_t>{3}));
    EXPECT_EQ(encoded(OcaPortID{.mode = OcaPortMode::output, .index = 2}), (std::vector<uint8_t>{2, 0, 2}));
}

TEST(oca_types, a_counter_set_round_trips)
{
    OcaCounterSet const set{
        .id = {.bytes = {1, 2}},
        .counter = {{.id = 1, .value = 7, .initial_value = 0, .role = "rx", .notifiers = {4096}}},
    };
    auto const decoded = padl::ocp1::decode_value<OcaCounterSet>(encoded(set));
    EXPECT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded.value_or(OcaCounterSet{}) == set);
}

TEST(oca_types, ip4_settings_round_trip)
{
    OcaIP4NetworkSettings const settings{
        .address_and_prefix = "10.10.32.5/24",
        .autoconfig_mode = OcaIP4AutoconfigMode::dhcp,
        .additional_gateways = {{.destination_prefix = "239.0.0.0/8", .gateway_address = "10.10.32.1", .metric = 1}},
        .dns_server_addresses = {"10.10.32.1"},
    };
    auto const decoded = padl::ocp1::decode_value<OcaIP4NetworkSettings>(encoded(settings));
    EXPECT_TRUE(decoded.value_or(OcaIP4NetworkSettings{}) == settings);
}

TEST_MAIN(padl_oca, oca_types_test)
