// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "statusbar/ocp1/ocp1_aes67.hpp"

#include "statusbar/test/test.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace statusbar;
using namespace statusbar::ocp1;

namespace {

auto from_hex(std::string_view const hex) -> std::vector<uint8_t>
{
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>(std::stoul(std::string{hex.substr(i, 2)}, nullptr, 16)));
    }
    return out;
}

/// An output's record, as SwiftOCA's Aes67EndpointAdaptationData encodes it below.
auto output_record() -> Aes67EndpointAdaptationData
{
    return {
        .ip_parameters =
            {{.network_assignment_id = 1,
              .source_address = "10.10.32.5",
              .destination_address = "239.69.5.1",
              .destination_port = 5004}},
        .payload_type = 98,
        .rtp_timestamp_offset = 0x1234'5678,
        .remote_endpoint_name = {.bytes = {'O', 'u', 't'}},
        .remote_control_type = "NONE",
        .transmission_capabilities = AES67_UNICAST_CAPABLE | AES67_MULTICAST_CAPABLE | AES67_INSECURE_CAPABLE,
        .active_sdp = "v=0\r\n",
        .stream_id = 7,
    };
}

}  // namespace

// The bytes SwiftOCA's Aes67EndpointAdaptationData.blob writes for output_record()
TEST(ocp1_aes67, the_record_encodes_as_swiftoca_writes_it)
{
    auto const expected = from_hex("0001"  // IPParameters: one
                                   "0001"  // NetworkAssignmentID
                                   "000a"
                                   "31302e31302e33322e35"  // SourceAddress
                                   "000a"
                                   "3233392e36392e352e31"  // DestinationAddress
                                   "00"
                                   "0000"
                                   "138c"  // TimeToLive, SourcePort, DestinationPort
                                   "ff"
                                   "ff"  // DSCP, COS
                                   "62"
                                   "12345678"  // PayloadType, RTPTimestampOffset
                                   "00000000"  // TimeSkewMaxTolerated
                                   "0003"
                                   "4f7574"  // RemoteEndpointName
                                   "0004"
                                   "4e4f4e45"  // RemoteControlType
                                   "0007"      // TransmissionCapabilities
                                   "00000000"  // PresentationTimeOffset
                                   "0000"      // MediaInfo
                                   "0005"
                                   "763d300d0a"  // ActiveSDP
                                   "00000007");  // StreamID
    auto const blob = encode_adaptation_data(output_record());
    EXPECT_TRUE(blob && blob->bytes == expected);
    auto const decoded = decode_adaptation_data(expected);
    EXPECT_TRUE(decoded && *decoded == output_record());
}

TEST(ocp1_aes67, an_empty_record_round_trips)
{
    Aes67EndpointAdaptationData const empty{};
    auto const blob = encode_adaptation_data(empty);
    EXPECT_TRUE(blob.has_value());
    auto const decoded = blob ? decode_adaptation_data(blob->bytes) : decode_adaptation_data({});
    EXPECT_TRUE(decoded && *decoded == empty);
}

TEST(ocp1_aes67, trailing_bytes_are_refused)
{
    auto blob = encode_adaptation_data(output_record());
    EXPECT_TRUE(blob.has_value());
    blob->bytes.push_back(0);
    auto const decoded = decode_adaptation_data(blob->bytes);
    EXPECT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error(), make_error_code(Ocp1Error::trailing_data));
}

TEST(ocp1_aes67, a_record_past_a_blob_is_too_long)
{
    auto record = output_record();
    record.active_sdp.assign(40000, 'a');
    record.media_info.assign(40000, 'b');
    auto const blob = encode_adaptation_data(record);
    EXPECT_FALSE(blob.has_value());
    EXPECT_EQ(blob.error(), make_error_code(Ocp1Error::too_long));
}

TEST_MAIN(statusbar_ocp1, ocp1_aes67_test)
