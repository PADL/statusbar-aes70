#pragma once

// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// AES70-2 data types of media stream endpoints, network interfaces, counters, media clocks and
/// time sources (the AES70-2023 connection management classes), as OCP.1 values: each record names
/// its fields in AES70's order, each enumeration its AES70 width. Members are snake_case.

#include "padl/ocp1/ocp1.hpp"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace padl::oca {

//
// Base types - AES70-2 OcaBaseDataTypes
//

/// OcaONo: an object number
using OcaONo = uint32_t;
/// OcaID16: a 16-bit identifier
using OcaID16 = uint16_t;
/// OcaFrequency, in hertz
using OcaFrequency = float;
/// OcaTimeInterval, in seconds
using OcaTimeInterval = float;
/// OcaDBFS: a level in dB relative to full scale
using OcaDBFS = float;
/// OcaMimeType: a MIME type, such as "audio/L24"
using OcaMimeType = std::string;
/// OcaBlob
using OcaBlob = ocp1::Blob;

/// The ONo no object has
inline constexpr OcaONo OCA_INVALID_ONO = 0;

/// OcaIODirection
enum class OcaIODirection : uint8_t
{
    input = 1,
    output = 2,
};

/// OcaPortMode: a port's direction
using OcaPortMode = OcaIODirection;

/// OcaPortID: a port of an object, by direction and index
struct OcaPortID
{
    OcaPortMode mode{OcaPortMode::input};
    uint16_t index{0};

    auto fields(this auto& self) noexcept { return std::tie(self.mode, self.index); }
    auto operator==(OcaPortID const&) const -> bool = default;
};

/// OcaSecurityType: whether a stream is secured
enum class OcaSecurityType : uint8_t
{
    none = 0,
    default_ = 1,
};

//
// Media stream endpoints - AES70-2 OcaMediaTransportApplication data types
//

/// OcaMediaStreamEndpointID: an endpoint's ID within its media transport application
using OcaMediaStreamEndpointID = uint32_t;
/// OcaAdaptationData: what an adaptation of AES70 defines, encoded in a blob
using OcaAdaptationData = ocp1::Blob;

/// OcaMediaStreamEndpointState
enum class OcaMediaStreamEndpointState : uint8_t
{
    unknown = 0,
    not_ready = 1,
    ready = 2,
    connected = 3,
    running = 4,
    error_halt = 5,
};

/// OcaMediaStreamEndpointCommand: what ApplyCommand asks of an endpoint
enum class OcaMediaStreamEndpointCommand : uint8_t
{
    none = 0,
    set_ready = 1,
    connect = 2,
    connect_and_start = 3,
    disconnect = 4,
    stop_and_disconnect = 5,
    start = 6,
    stop = 7,
};

/// OcaMediaFrameFormat: a stream's packet format
enum class OcaMediaFrameFormat : uint8_t
{
    undefined = 0,
    rtp = 1,
    aaf = 2,
    crf_milan = 3,
    iec_61883_6 = 4,
    usb_audio_2_0 = 5,
    extension_point = 65,
};

/// OcaMediaStreamCastMode
enum class OcaMediaStreamCastMode : uint8_t
{
    none = 0,
    unicast = 1,
    multicast = 2,
};

/// OcaMediaStreamMode: a stream's format, rate, channel count and packet time
struct OcaMediaStreamMode
{
    OcaMediaFrameFormat frame_format{OcaMediaFrameFormat::undefined};
    OcaMimeType encoding_type;
    OcaFrequency sampling_rate{0};
    uint16_t channel_count{0};
    OcaTimeInterval packet_time{0};

    auto fields(this auto& self) noexcept
    {
        return std::tie(self.frame_format, self.encoding_type, self.sampling_rate, self.channel_count, self.packet_time);
    }
    auto operator==(OcaMediaStreamMode const&) const -> bool = default;
};

/// OcaMediaStreamEndpoint: one end of a media stream; its channel map is an
/// OcaMultiMap<OcaUint16, OcaPortID>, stream channel to port.
struct OcaMediaStreamEndpoint
{
    OcaMediaStreamEndpointID id_internal{0};
    OcaBlob id_external;
    OcaIODirection direction{OcaIODirection::input};
    std::string user_label;
    std::vector<OcaID16> network_assignment_ids;
    std::vector<OcaID16> stream_mode_capability_ids;
    OcaONo clock_ono{OCA_INVALID_ONO};
    bool channel_map_dynamic{false};
    ocp1::Map<uint16_t, OcaPortID> channel_map;
    OcaDBFS alignment_level{0};
    OcaMediaStreamMode current_stream_mode;
    OcaSecurityType security_type{OcaSecurityType::none};
    OcaMediaStreamCastMode stream_cast_mode{OcaMediaStreamCastMode::none};
    OcaAdaptationData adaptation_data;
    OcaID16 redundant_set_id{0};

    auto fields(this auto& self) noexcept
    {
        return std::tie(
            self.id_internal,
            self.id_external,
            self.direction,
            self.user_label,
            self.network_assignment_ids,
            self.stream_mode_capability_ids,
            self.clock_ono,
            self.channel_map_dynamic,
            self.channel_map,
            self.alignment_level,
            self.current_stream_mode,
            self.security_type,
            self.stream_cast_mode,
            self.adaptation_data,
            self.redundant_set_id);
    }
};

/// OcaMediaStreamEndpointStatus: an endpoint's state, and an error code when it has halted
struct OcaMediaStreamEndpointStatus
{
    OcaMediaStreamEndpointState state{OcaMediaStreamEndpointState::unknown};
    uint16_t error_code{0};

    auto fields(this auto& self) noexcept { return std::tie(self.state, self.error_code); }
    auto operator==(OcaMediaStreamEndpointStatus const&) const -> bool = default;
};

//
// Counters - AES70-2 OcaCounter, OcaCounterSet
//

/// OcaCounterSetID: what an adaptation names a counter set by
using OcaCounterSetID = ocp1::Blob;

/// OcaCounter: one counter, and the OcaCounterNotifiers watching it
struct OcaCounter
{
    OcaID16 id{0};
    uint64_t value{0};
    uint64_t initial_value{0};
    std::string role;
    std::vector<OcaONo> notifiers;

    auto fields(this auto& self) noexcept { return std::tie(self.id, self.value, self.initial_value, self.role, self.notifiers); }
    auto operator==(OcaCounter const&) const -> bool = default;
};

/// OcaCounterSet
struct OcaCounterSet
{
    OcaCounterSetID id;
    std::vector<OcaCounter> counter;

    auto fields(this auto& self) noexcept { return std::tie(self.id, self.counter); }
    auto operator==(OcaCounterSet const&) const -> bool = default;
};

//
// Network interfaces - AES70-2 OcaNetworkInterface data types
//

/// OcaNetworkInterfaceState
enum class OcaNetworkInterfaceState : uint8_t
{
    not_ready = 0,
    ready = 1,
    fault = 2,
};

/// OcaNetworkInterfaceStatus: an interface's state and its adaptation's status data
struct OcaNetworkInterfaceStatus
{
    OcaNetworkInterfaceState state{OcaNetworkInterfaceState::not_ready};
    OcaAdaptationData adaptation_data;

    auto fields(this auto& self) noexcept { return std::tie(self.state, self.adaptation_data); }
    auto operator==(OcaNetworkInterfaceStatus const&) const -> bool = default;
};

/// OcaIP4AutoconfigMode
enum class OcaIP4AutoconfigMode : uint8_t
{
    none = 0,
    dhcp = 1,
    dhcp_link_local = 2,
    link_local = 3,
};

/// OcaIP4Gateway: a route through a gateway
struct OcaIP4Gateway
{
    std::string destination_prefix;  ///< OcaIP4AddressAndPrefix, "A.B.C.D/N"
    std::string gateway_address;     ///< OcaIP4Address
    uint16_t metric{0};

    auto fields(this auto& self) noexcept { return std::tie(self.destination_prefix, self.gateway_address, self.metric); }
    auto operator==(OcaIP4Gateway const&) const -> bool = default;
};

/// OcaIP4NetworkSettings: an IPv4 interface's settings; addresses are OcaStrings, the
/// parameter record an OcaJsonValue.
struct OcaIP4NetworkSettings
{
    std::string address_and_prefix;  ///< "A.B.C.D/N"
    OcaIP4AutoconfigMode autoconfig_mode{OcaIP4AutoconfigMode::none};
    std::string dhcp_server_address;
    std::string default_gateway_address;
    std::vector<OcaIP4Gateway> additional_gateways;
    std::vector<std::string> dns_server_addresses;
    std::string additional_parameters{"{}"};

    auto fields(this auto& self) noexcept
    {
        return std::tie(
            self.address_and_prefix,
            self.autoconfig_mode,
            self.dhcp_server_address,
            self.default_gateway_address,
            self.additional_gateways,
            self.dns_server_addresses,
            self.additional_parameters);
    }
    auto operator==(OcaIP4NetworkSettings const&) const -> bool = default;
};

//
// Media clocks - AES70-2 OcaMediaClock3 data types
//

/// OcaMediaClockAvailability
enum class OcaMediaClockAvailability : uint8_t
{
    unavailable = 0,
    available = 1,
};

/// OcaMediaClockRate: a nominal rate in hertz, its pull range, accuracy and maximum jitter
struct OcaMediaClockRate
{
    OcaFrequency nominal_rate{48000.0F};
    OcaFrequency pull_range{0};
    float accuracy{0};
    float jitter_max{0};

    auto fields(this auto& self) noexcept { return std::tie(self.nominal_rate, self.pull_range, self.accuracy, self.jitter_max); }
    auto operator==(OcaMediaClockRate const&) const -> bool = default;
};

//
// Time sources - AES70-2 OcaTimeSource data types
//

/// OcaTimeSourceAvailability
enum class OcaTimeSourceAvailability : uint8_t
{
    unavailable = 0,
    available = 1,
};

/// OcaTimeSourceSyncStatus
enum class OcaTimeSourceSyncStatus : uint8_t
{
    undefined = 0,
    unsynchronized = 1,
    synchronizing = 2,
    synchronized = 3,
};

/// OcaTimeProtocol
enum class OcaTimeProtocol : uint8_t
{
    undefined = 0,
    none = 1,
    private_ = 2,
    ntp = 3,
    sntp = 4,
    ieee1588_2002 = 5,
    ieee1588_2008 = 6,
    ieee_avb = 7,
    stream_endpoint = 8,
    aes11 = 9,
    genlock = 10,
    expansion_base = 128,
};

/// OcaTimeDeliveryMechanism
enum class OcaTimeDeliveryMechanism : uint8_t
{
    undefined = 0,
    local = 1,
    private_ = 2,
    ntp = 3,
    sntp = 4,
    ieee1588v1 = 5,
    ieee1588v2 = 6,
    ieee1588v2_1 = 7,
    ieee8021as = 8,
    stream_endpoint = 9,
    aes11 = 11,
    terrestrial_radio = 12,
    gps = 13,
    galileo = 14,
    glonass = 15,
    beidou = 16,
    inrss = 17,
    expansion_base = 128,
};

/// OcaTimeReferenceType
enum class OcaTimeReferenceType : uint8_t
{
    undefined = 0,
    local = 1,
    private_ = 2,
    tai = 3,
    expansion_base = 128,
};

}  // namespace padl::oca
