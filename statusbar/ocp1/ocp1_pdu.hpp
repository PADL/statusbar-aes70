#pragma once

// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// OCP.1 PDUs - AES70-3 section 6 framing, one message per PDU.
///
///     PDU           syncVal 0x3B, protocolVersion u16 = 1, pduSize u32 (bytes
///                   after syncVal), pduType u8, messageCount u16, messages
///     Command       commandSize u32, handle u32, targetONo u32,
///                   methodID {defLevel u16, methodIndex u16}, parameterCount u8,
///                   parameters
///     Response      responseSize u32, handle u32, statusCode u8,
///                   parameterCount u8, parameters
///     Notification2 notificationSize u32, emitterONo u32,
///                   eventID {defLevel u16, eventIndex u16}, notificationType u8,
///                   data
///
/// A message size counts the message's own bytes, its size field included. Only
/// PDUs holding exactly one Command, Response or Notification2 are accepted:
/// keep-alives and Notification1 belong to a full OCP.1 connection, which this
/// is not. Parsed parameters and data are views into the PDU.

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/ocp1/ocp1_error.hpp"
#include "statusbar/status/status.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <variant>

namespace statusbar::ocp1 {

using ieee::doublet_t;
using ieee::octet_t;
using ieee::quadlet_t;

inline constexpr uint8_t SYNC = 0x3B;
/// The version written; AES70-3 says it is always 1, but some controllers write a
/// higher one (the AES70 revision, it seems), so parsing accepts any version >= 1.
inline constexpr uint16_t PROTOCOL_VERSION = 1;

/// OcaMessageType
enum class MessageType : uint8_t
{
    command = 0,                ///< Ocp1CommandPdu, no response wanted
    command_response_required,  ///< Ocp1CommandPdu, response wanted
    notification1,
    response,
    keep_alive,
    notification2,
};

/// OcaStatus
enum class OcaStatus : uint8_t
{
    ok = 0,
    protocol_version_error,
    device_error,
    locked,
    bad_format,
    bad_ono,
    parameter_error,
    parameter_out_of_range,
    not_implemented,
    invalid_request,
    processing_failed,
    bad_method,
    partially_succeeded,
    timeout,
    buffer_overflow,
    permission_denied,
    out_of_memory,
    busy,
};

/// Ocp1Notification2Type
enum class NotificationType : uint8_t
{
    event = 0,
    exception,
};

/// OcaMethodID; a record, for methods that take one
struct MethodId
{
    uint16_t def_level{};
    uint16_t index{};

    auto fields(this auto& self) noexcept { return std::tie(self.def_level, self.index); }
    auto operator==(MethodId const&) const -> bool = default;
};

/// OcaEventID
struct EventId
{
    uint16_t def_level{};
    uint16_t index{};

    auto fields(this auto& self) noexcept { return std::tie(self.def_level, self.index); }
    auto operator==(EventId const&) const -> bool = default;
};

/// OcaEvent
struct Event
{
    uint32_t emitter{};
    EventId id;

    auto fields(this auto& self) noexcept { return std::tie(self.emitter, self.id); }
    auto operator==(Event const&) const -> bool = default;
};

/// A method's encoded parameters, and how many values they are. Every value is at
/// least a byte, so a count of 0 has no data and any other is at most its length.
struct Parameters
{
    uint8_t count{0};
    std::span<uint8_t const> data;
};

struct Command
{
    uint32_t handle{};
    uint32_t target{};
    MethodId method;
    Parameters parameters;
    bool response_required{true};
};

struct Response
{
    uint32_t handle{};
    OcaStatus status{OcaStatus::ok};
    Parameters parameters;
};

/// A Notification2. Its data is never empty: an event carries its parameters and an
/// exception its OcaNotification2ExceptionData, and SwiftOCA refuses one without.
struct Notification
{
    Event event;
    NotificationType type{NotificationType::event};
    std::span<uint8_t const> data;
};

using Message = std::variant<Command, Response, Notification>;

/// syncVal and the PDU header (AES70-3 Ocp1Header)
struct PduHeader
{
    static constexpr size_t LENGTH = 10;

    octet_t sync;
    doublet_t protocol_version;
    quadlet_t pdu_size;  ///< bytes after sync
    octet_t message_type;
    doublet_t message_count;
};

static_assert(sizeof(PduHeader) == PduHeader::LENGTH);
static_assert(alignof(PduHeader) == 1);

struct CommandHeader
{
    static constexpr size_t LENGTH = 17;

    quadlet_t command_size;
    quadlet_t handle;
    quadlet_t target;
    doublet_t def_level;
    doublet_t method_index;
    octet_t parameter_count;
};

static_assert(sizeof(CommandHeader) == CommandHeader::LENGTH);

struct ResponseHeader
{
    static constexpr size_t LENGTH = 10;

    quadlet_t response_size;
    quadlet_t handle;
    octet_t status;
    octet_t parameter_count;
};

static_assert(sizeof(ResponseHeader) == ResponseHeader::LENGTH);

struct Notification2Header
{
    static constexpr size_t LENGTH = 13;

    quadlet_t notification_size;
    quadlet_t emitter;
    doublet_t def_level;
    doublet_t event_index;
    octet_t notification_type;
};

static_assert(sizeof(Notification2Header) == Notification2Header::LENGTH);

/// The size of the PDU whose header begins `header`, for a reader framing a
/// stream: nullopt while the header is incomplete, an error if it is not a PDU
/// header (invalid_sync) or the PDU would be smaller than its header or larger
/// than `maximum` (invalid_pdu_size), after which the stream cannot be framed.
[[nodiscard]] auto pdu_size(std::span<uint8_t const> header, size_t maximum) noexcept -> StatusValue<std::optional<size_t>>;

/// Parse a whole PDU holding one message. Its parameters or data view `pdu`. A
/// status code or notification type AES70 does not define, a parameter count its
/// parameters cannot hold, or a notification with no data is an error.
[[nodiscard]] auto parse_pdu(std::span<uint8_t const> pdu) noexcept -> StatusValue<Message>;

/// The size of `message` as a PDU.
[[nodiscard]] auto serialized_pdu_size(Message const& message) noexcept -> size_t;

/// Write `message` into `buffer` as a PDU; its length. The message's parameters
/// or data must not lie in `buffer` (aliased_parameters): the headers are written
/// first, over them.
[[nodiscard]] auto serialize_pdu(Message const& message, std::span<uint8_t> buffer) noexcept -> StatusValue<size_t>;

}  // namespace statusbar::ocp1

// Serialization traits - the headers are packed wire format structs
template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<statusbar::ocp1::PduHeader> : std::true_type
{};

template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<statusbar::ocp1::CommandHeader> : std::true_type
{};

template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<statusbar::ocp1::ResponseHeader> : std::true_type
{};

template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<statusbar::ocp1::Notification2Header> : std::true_type
{};

// Export using declarations for ADL to find the template functions
namespace statusbar::ocp1 {
using protocol::load_unchecked;
using protocol::store_unchecked;
}  // namespace statusbar::ocp1
