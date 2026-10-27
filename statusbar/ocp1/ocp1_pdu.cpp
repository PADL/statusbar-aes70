// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "statusbar/ocp1/ocp1_pdu.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <variant>

namespace statusbar::ocp1 {

namespace {

template <class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};

auto valid(Parameters const& parameters) noexcept -> bool
{
    return parameters.count == 0 ? parameters.data.empty() : parameters.count <= parameters.data.size();
}

/// What framing a message needs: its PDU type, its size (header and payload), its
/// payload, and whether its count and payload agree
struct Framing
{
    MessageType type;
    size_t size;
    std::span<uint8_t const> payload;
    bool valid;
};

auto framing(Message const& message) noexcept -> Framing
{
    return std::visit(
        Overloaded{
            [](Command const& command) {
                return Framing{
                    .type = command.response_required ? MessageType::command_response_required : MessageType::command,
                    .size = CommandHeader::LENGTH + command.parameters.data.size(),
                    .payload = command.parameters.data,
                    .valid = valid(command.parameters),
                };
            },
            [](Response const& response) {
                return Framing{
                    .type = MessageType::response,
                    .size = ResponseHeader::LENGTH + response.parameters.data.size(),
                    .payload = response.parameters.data,
                    .valid = valid(response.parameters),
                };
            },
            [](Notification const& notification) {
                return Framing{
                    .type = MessageType::notification2,
                    .size = Notification2Header::LENGTH + notification.data.size(),
                    .payload = notification.data,
                    .valid = !notification.data.empty(),
                };
            },
        },
        message);
}

auto overlaps(std::span<uint8_t const> const a, std::span<uint8_t> const b) noexcept -> bool
{
    if (a.empty() || b.empty()) {
        return false;
    }
    auto const less = std::less<uint8_t const*>{};
    return less(a.data(), b.data() + b.size()) && less(b.data(), a.data() + a.size());
}

/// The size a complete header declares (pdu_size counts the bytes after sync), or nullopt
/// while the header is still short
auto declared_size(std::span<uint8_t const> const header) noexcept -> std::optional<size_t>
{
    PduHeader pdu{};
    BufferDeserializerBuilder reader{header};
    reader.parse(&pdu);
    return reader ? std::optional<size_t>{size_t{pdu.pdu_size.get()} + 1} : std::nullopt;
}

/// AES70-3's checks on a PDU header for a PDU of `size` bytes carrying one message
auto check_header(PduHeader const& header, size_t const size) noexcept -> Status
{
    if (header.sync != SYNC) {
        return failure(Ocp1Error::invalid_sync);
    }
    if (header.protocol_version < PROTOCOL_VERSION) {
        return failure(Ocp1Error::unsupported_version);
    }
    if (size_t{header.pdu_size.get()} + 1 != size) {
        return failure(Ocp1Error::invalid_pdu_size);
    }
    if (header.message_count != 1) {
        return failure(Ocp1Error::invalid_message_count);
    }
    return success();
}

/// A message's fixed header, read from `reader`, whose own size field (`size_of`) must be the body's
template <class Header, class SizeOf>
auto fixed_header(BufferDeserializerBuilder& reader, std::span<uint8_t const> const body, SizeOf const size_of) noexcept
    -> StatusValue<Header>
{
    Header fixed{};
    reader.parse(&fixed);
    if (!reader) {
        return failure(reader.error());
    }
    if (size_of(fixed) != body.size()) {
        return failure(Ocp1Error::invalid_message_size);
    }
    return fixed;
}

/// The parameters after a fixed header of `length` bytes; invalid_parameter_count if the count disagrees
auto parameters_after(uint8_t const count, std::span<uint8_t const> const body, size_t const length) noexcept
    -> StatusValue<Parameters>
{
    Parameters const parameters{.count = count, .data = body.subspan(length)};
    if (!valid(parameters)) {
        return failure(Ocp1Error::invalid_parameter_count);
    }
    return parameters;
}

auto parse_command(BufferDeserializerBuilder& reader, std::span<uint8_t const> const body, bool const response_required) noexcept
    -> StatusValue<Message>
{
    auto const command = fixed_header<CommandHeader>(reader, body, [](CommandHeader const& h) { return h.command_size; });
    if (!command) {
        return forward_failure(command);
    }
    auto const parameters = parameters_after(command->parameter_count, body, CommandHeader::LENGTH);
    if (!parameters) {
        return forward_failure(parameters);
    }
    return Command{
        .handle = command->handle,
        .target = command->target,
        .method = {.def_level = command->def_level, .index = command->method_index},
        .parameters = *parameters,
        .response_required = response_required,
    };
}

auto parse_response(BufferDeserializerBuilder& reader, std::span<uint8_t const> const body) noexcept -> StatusValue<Message>
{
    auto const response = fixed_header<ResponseHeader>(reader, body, [](ResponseHeader const& h) { return h.response_size; });
    if (!response) {
        return forward_failure(response);
    }
    if (response->status > static_cast<uint8_t>(OcaStatus::busy)) {
        return failure(Ocp1Error::invalid_status);
    }
    auto const parameters = parameters_after(response->parameter_count, body, ResponseHeader::LENGTH);
    if (!parameters) {
        return forward_failure(parameters);
    }
    return Response{
        .handle = response->handle, .status = static_cast<OcaStatus>(response->status.get()), .parameters = *parameters};
}

auto parse_notification(BufferDeserializerBuilder& reader, std::span<uint8_t const> const body) noexcept -> StatusValue<Message>
{
    auto const notification =
        fixed_header<Notification2Header>(reader, body, [](Notification2Header const& h) { return h.notification_size; });
    if (!notification) {
        return forward_failure(notification);
    }
    if (notification->notification_type > static_cast<uint8_t>(NotificationType::exception)) {
        return failure(Ocp1Error::invalid_notification_type);
    }
    if (body.size() == Notification2Header::LENGTH) {
        return failure(Ocp1Error::invalid_message_size);
    }
    return Notification{
        .event =
            {.emitter = notification->emitter, .id = {.def_level = notification->def_level, .index = notification->event_index}},
        .type = static_cast<NotificationType>(notification->notification_type.get()),
        .data = body.subspan(Notification2Header::LENGTH),
    };
}

/// The message after a valid PDU header; notification1 and keep-alive are not carried
auto parse_message(MessageType const type, BufferDeserializerBuilder& reader, std::span<uint8_t const> const body) noexcept
    -> StatusValue<Message>
{
    switch (type) {
        case MessageType::command:
            return parse_command(reader, body, false);
        case MessageType::command_response_required:
            return parse_command(reader, body, true);
        case MessageType::response:
            return parse_response(reader, body);
        case MessageType::notification2:
            return parse_notification(reader, body);
        case MessageType::notification1:
        case MessageType::keep_alive:
            break;
    }
    return failure(Ocp1Error::unsupported_message_type);
}

/// Whether `message` can be framed into `buffer`: its size, aliasing and count checks
auto check_framing(Message const& message, Framing const& framing, std::span<uint8_t> const buffer) noexcept -> Status
{
    if (PduHeader::LENGTH - 1 + framing.size > UINT32_MAX) {
        return failure(Ocp1Error::invalid_pdu_size);
    }
    if (overlaps(framing.payload, buffer)) {
        return failure(Ocp1Error::aliased_parameters);
    }
    if (!framing.valid) {
        return failure(
            std::holds_alternative<Notification>(message) ? Ocp1Error::invalid_message_size : Ocp1Error::invalid_parameter_count);
    }
    return success();
}

/// Append the PDU header, the message's fixed header (its size counts the payload) and the payload
void append_pdu(BufferSerializerBuilderWithBuffer& writer, Message const& message, Framing const& framed)
{
    auto const size = static_cast<uint32_t>(framed.size);
    writer.append(PduHeader{
        .sync = SYNC,
        .protocol_version = PROTOCOL_VERSION,
        .pdu_size = static_cast<uint32_t>(PduHeader::LENGTH - 1 + framed.size),
        .message_type = static_cast<uint8_t>(framed.type),
        .message_count = 1});
    std::visit(
        Overloaded{
            [&](Command const& c) {
                writer
                    .append(CommandHeader{
                        .command_size = size,
                        .handle = c.handle,
                        .target = c.target,
                        .def_level = c.method.def_level,
                        .method_index = c.method.index,
                        .parameter_count = c.parameters.count})
                    .append(c.parameters.data);
            },
            [&](Response const& r) {
                writer
                    .append(ResponseHeader{
                        .response_size = size,
                        .handle = r.handle,
                        .status = static_cast<uint8_t>(r.status),
                        .parameter_count = r.parameters.count})
                    .append(r.parameters.data);
            },
            [&](Notification const& n) {
                writer
                    .append(Notification2Header{
                        .notification_size = size,
                        .emitter = n.event.emitter,
                        .def_level = n.event.id.def_level,
                        .event_index = n.event.id.index,
                        .notification_type = static_cast<uint8_t>(n.type)})
                    .append(n.data);
            },
        },
        message);
}

}  // namespace

auto pdu_size(std::span<uint8_t const> const header, size_t const maximum) noexcept -> StatusValue<std::optional<size_t>>
{
    if (header.empty()) {
        return std::optional<size_t>{};
    }
    if (header[0] != SYNC) {
        return failure(Ocp1Error::invalid_sync);
    }
    auto const size = declared_size(header);
    if (size && (*size < PduHeader::LENGTH || *size > maximum)) {
        return failure(Ocp1Error::invalid_pdu_size);
    }
    return size;
}

auto parse_pdu(std::span<uint8_t const> const pdu) noexcept -> StatusValue<Message>
{
    PduHeader header{};
    BufferDeserializerBuilder reader{pdu};
    reader.parse(&header);
    if (!reader) {
        return failure(reader.error());
    }
    if (auto const checked = check_header(header, pdu.size()); !checked) {
        return forward_failure(checked);
    }
    return parse_message(static_cast<MessageType>(header.message_type.get()), reader, pdu.subspan(PduHeader::LENGTH));
}

auto serialized_pdu_size(Message const& message) noexcept -> size_t
{
    return PduHeader::LENGTH + framing(message).size;
}

auto serialize_pdu(Message const& message, std::span<uint8_t> const buffer) noexcept -> StatusValue<size_t>
{
    auto const framed = framing(message);
    if (auto const checked = check_framing(message, framed, buffer); !checked) {
        return forward_failure(checked);
    }
    BufferSerializerBuilderWithBuffer writer{buffer};
    append_pdu(writer, message, framed);
    if (!writer) {
        return failure(writer.status().error());
    }
    return writer.position();
}

}  // namespace statusbar::ocp1
