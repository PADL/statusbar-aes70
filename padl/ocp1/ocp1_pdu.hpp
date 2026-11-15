#pragma once

// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// OCP.1 PDUs holding one Command, Response or Notification2 (AES70-3 Ocp1Header, Ocp1Command,
/// Ocp1Response, Ocp1Notification2): framing a byte stream, parsing and serializing. The layout and
/// what is refused are in docs/OCP1_MODULE.md.

#include "padl/ocp1/ocp1_error.hpp"
#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/status/status.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <variant>

namespace padl::ocp1 {

/// syncVal, the first octet of every PDU
inline constexpr uint8_t SYNC = 0x3B;
/// The protocolVersion written; parsing accepts any version from 1.
inline constexpr uint16_t PROTOCOL_VERSION = 1;

//
// Message types and status codes - AES70-3 OcaMessageType, AES70-2 OcaStatus
//

/// OcaMessageType: what the messages of a PDU are.
enum class MessageType : uint8_t
{
    command = 0,                ///< Ocp1CommandPdu, no response wanted
    command_response_required,  ///< Ocp1CommandPdu, response wanted
    notification1,
    response,
    keep_alive,
    notification2,
};

/// OcaStatus: what a response says of its command.
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

/// Ocp1Notification2Type: an event, or an exception the emitter reports.
enum class NotificationType : uint8_t
{
    event = 0,
    exception,
};

//
// Identifiers - AES70-2 OcaMethodID, OcaEventID, OcaEvent
//

/// OcaMethodID: a method's definition level and index; a record, for methods that need one.
struct MethodId
{
    uint16_t def_level{};
    uint16_t index{};

    auto fields(this auto& self) noexcept { return std::tie(self.def_level, self.index); }
    auto operator==(MethodId const&) const -> bool = default;
};

/// OcaEventID: an event's definition level and index.
struct EventId
{
    uint16_t def_level{};
    uint16_t index{};

    auto fields(this auto& self) noexcept { return std::tie(self.def_level, self.index); }
    auto operator==(EventId const&) const -> bool = default;
};

/// OcaEvent: the object emitting an event, and the event.
struct Event
{
    uint32_t emitter{};
    EventId id;

    auto fields(this auto& self) noexcept { return std::tie(self.emitter, self.id); }
    auto operator==(Event const&) const -> bool = default;
};

//
// Messages - AES70-3 Ocp1Command, Ocp1Response, Ocp1Notification2
//

/// A method's encoded parameters, and how many values they are. Every value is at
/// least a byte, so a count of 0 has no data and any other is at most its length.
struct Parameters
{
    uint8_t count{0};
    std::span<uint8_t const> data;
};

/// Ocp1Command: method `method` of object `target`; `response_required` picks the PDU's message type.
struct Command
{
    uint32_t handle{};
    uint32_t target{};
    MethodId method;
    Parameters parameters;
    bool response_required{true};
};

/// Ocp1Response: the answer to the command with `handle`, its parameters the method's results.
struct Response
{
    uint32_t handle{};
    OcaStatus status{OcaStatus::ok};
    Parameters parameters;
};

/// Ocp1Notification2: an event's parameters, or an exception's Ocp1Notification2ExceptionData.
/// Either way its data is not empty.
struct Notification
{
    Event event;
    NotificationType type{NotificationType::event};
    std::span<uint8_t const> data;
};

/// The one message a PDU holds
using Message = std::variant<Command, Response, Notification>;

//
// Wire headers - AES70-3 Ocp1Header and the message headers
//

/// syncVal and Ocp1Header
struct PduHeader
{
    static constexpr size_t LENGTH = 10;

    statusbar::ieee::octet_t sync;                ///< syncVal, SYNC
    statusbar::ieee::doublet_t protocol_version;  ///< protocolVersion
    statusbar::ieee::quadlet_t pdu_size;          ///< pduSize: the PDU's bytes after syncVal
    statusbar::ieee::octet_t message_type;        ///< pduType, a MessageType
    statusbar::ieee::doublet_t message_count;     ///< messageCount
};

static_assert(sizeof(PduHeader) == PduHeader::LENGTH);
static_assert(alignof(PduHeader) == 1);
static_assert(offsetof(PduHeader, sync) == 0);
static_assert(offsetof(PduHeader, protocol_version) == 1);
static_assert(offsetof(PduHeader, pdu_size) == 3);
static_assert(offsetof(PduHeader, message_type) == 7);
static_assert(offsetof(PduHeader, message_count) == 8);

/// Ocp1Command up to its parameters
struct CommandHeader
{
    static constexpr size_t LENGTH = 17;

    statusbar::ieee::quadlet_t size;           ///< commandSize: the message's bytes, this field's included
    statusbar::ieee::quadlet_t handle;         ///< handle
    statusbar::ieee::quadlet_t target;         ///< targetONo
    statusbar::ieee::doublet_t def_level;      ///< methodID.defLevel
    statusbar::ieee::doublet_t method_index;   ///< methodID.methodIndex
    statusbar::ieee::octet_t parameter_count;  ///< parameters.parameterCount
};

static_assert(sizeof(CommandHeader) == CommandHeader::LENGTH);
static_assert(offsetof(CommandHeader, size) == 0);
static_assert(offsetof(CommandHeader, handle) == 4);
static_assert(offsetof(CommandHeader, target) == 8);
static_assert(offsetof(CommandHeader, def_level) == 12);
static_assert(offsetof(CommandHeader, method_index) == 14);
static_assert(offsetof(CommandHeader, parameter_count) == 16);

/// Ocp1Response up to its parameters
struct ResponseHeader
{
    static constexpr size_t LENGTH = 10;

    statusbar::ieee::quadlet_t size;           ///< responseSize: the message's bytes, this field's included
    statusbar::ieee::quadlet_t handle;         ///< handle
    statusbar::ieee::octet_t status;           ///< statusCode, an OcaStatus
    statusbar::ieee::octet_t parameter_count;  ///< parameters.parameterCount
};

static_assert(sizeof(ResponseHeader) == ResponseHeader::LENGTH);
static_assert(offsetof(ResponseHeader, size) == 0);
static_assert(offsetof(ResponseHeader, handle) == 4);
static_assert(offsetof(ResponseHeader, status) == 8);
static_assert(offsetof(ResponseHeader, parameter_count) == 9);

/// Ocp1Notification2 up to its data
struct Notification2Header
{
    static constexpr size_t LENGTH = 13;

    statusbar::ieee::quadlet_t size;             ///< notificationSize: the message's bytes, this field's included
    statusbar::ieee::quadlet_t emitter;          ///< event.emitterONo
    statusbar::ieee::doublet_t def_level;        ///< event.eventID.defLevel
    statusbar::ieee::doublet_t event_index;      ///< event.eventID.eventIndex
    statusbar::ieee::octet_t notification_type;  ///< notificationType, a NotificationType
};

static_assert(sizeof(Notification2Header) == Notification2Header::LENGTH);
static_assert(offsetof(Notification2Header, size) == 0);
static_assert(offsetof(Notification2Header, emitter) == 4);
static_assert(offsetof(Notification2Header, def_level) == 8);
static_assert(offsetof(Notification2Header, event_index) == 10);
static_assert(offsetof(Notification2Header, notification_type) == 12);

//
// Framing, parsing and serializing
//

/// The size of the PDU whose header begins `header`, for a reader framing a
/// stream: nullopt while the header is incomplete, an error if it is not a PDU
/// header (invalid_sync) or the PDU would be smaller than its header or larger
/// than `maximum` (invalid_pdu_size), after which the stream cannot be framed.
[[nodiscard]] auto pdu_size(std::span<uint8_t const> header, size_t maximum) noexcept
    -> statusbar::StatusValue<std::optional<size_t>>;

/// The whole PDU at the front of `bytes`: empty until all of it has arrived, pdu_size()'s
/// error if the stream cannot be framed. The PDU views `bytes`.
[[nodiscard]] auto next_pdu(std::span<uint8_t const> bytes, size_t maximum) noexcept
    -> statusbar::StatusValue<std::span<uint8_t const>>;

/// The handle of the command `pdu` holds, when it wants a response and its PDU and command
/// headers read though the rest may not parse; nullopt otherwise. A device answers such a
/// command with bad_format rather than leave its controller waiting.
[[nodiscard]] auto command_handle(std::span<uint8_t const> pdu) noexcept -> std::optional<uint32_t>;

/// Parse a whole PDU holding one message. Its parameters or data view `pdu`. A
/// status code or notification type AES70 does not define, a parameter count its
/// parameters cannot hold, or a notification with no data is an error.
[[nodiscard]] auto parse_pdu(std::span<uint8_t const> pdu) noexcept -> statusbar::StatusValue<Message>;

/// The size of `message` as a PDU.
[[nodiscard]] auto serialized_pdu_size(Message const& message) noexcept -> size_t;

/// Write `message` into `buffer` as a PDU; its length. The message's parameters
/// or data must not lie in `buffer` (aliased_parameters): the headers are written
/// first, over them.
[[nodiscard]] auto serialize_pdu(Message const& message, std::span<uint8_t> buffer) noexcept -> statusbar::StatusValue<size_t>;

}  // namespace padl::ocp1

// Serialization traits - the headers are packed wire format structs
template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<padl::ocp1::PduHeader> : std::true_type
{};

template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<padl::ocp1::CommandHeader> : std::true_type
{};

template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<padl::ocp1::ResponseHeader> : std::true_type
{};

template <>
struct statusbar::traits::is_serializable_wire_fixed_struct<padl::ocp1::Notification2Header> : std::true_type
{};

// Export using declarations for ADL to find the template functions
namespace padl::ocp1 {
using statusbar::protocol::load_unchecked;
using statusbar::protocol::store_unchecked;
}  // namespace padl::ocp1
