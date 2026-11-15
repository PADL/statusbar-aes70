// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "padl/ocp1/ocp1_pdu.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <variant>

namespace padl::ocp1 {

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

/// What framing a message needs: its PDU type, its size (header and payload) and its payload
struct Framing
{
    MessageType type;
    size_t size;
    std::span<uint8_t const> payload;
};

/// A command's or response's framing; invalid_parameter_count if its count and parameters disagree
[[nodiscard]] auto framing_for(MessageType const type, size_t const header, Parameters const& parameters) noexcept
    -> statusbar::StatusValue<Framing>
{
    if (!valid(parameters)) {
        return statusbar::failure(Ocp1Error::invalid_parameter_count);
    }
    return Framing{.type = type, .size = header + parameters.data.size(), .payload = parameters.data};
}

/// A notification's framing; invalid_message_size if it has no data
[[nodiscard]] auto framing_for(Notification const& notification) noexcept -> statusbar::StatusValue<Framing>
{
    if (notification.data.empty()) {
        return statusbar::failure(Ocp1Error::invalid_message_size);
    }
    return Framing{
        .type = MessageType::notification2,
        .size = Notification2Header::LENGTH + notification.data.size(),
        .payload = notification.data};
}

[[nodiscard]] auto framing(Message const& message) noexcept -> statusbar::StatusValue<Framing>
{
    return std::visit(
        Overloaded{
            [](Command const& c) {
                auto const type = c.response_required ? MessageType::command_response_required : MessageType::command;
                return framing_for(type, CommandHeader::LENGTH, c.parameters);
            },
            [](Response const& r) { return framing_for(MessageType::response, ResponseHeader::LENGTH, r.parameters); },
            [](Notification const& n) { return framing_for(n); },
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
    statusbar::BufferDeserializerBuilder reader{header};
    reader.parse(&pdu);
    return reader ? std::optional<size_t>{size_t{pdu.pdu_size.get()} + 1} : std::nullopt;
}

/// AES70-3's checks on a PDU header for a PDU of `size` bytes carrying one message
auto check_header(PduHeader const& header, size_t const size) noexcept -> statusbar::Status
{
    if (header.sync != SYNC) {
        return statusbar::failure(Ocp1Error::invalid_sync);
    }
    if (header.protocol_version < PROTOCOL_VERSION) {
        return statusbar::failure(Ocp1Error::unsupported_version);
    }
    if (size_t{header.pdu_size.get()} + 1 != size) {
        return statusbar::failure(Ocp1Error::invalid_pdu_size);
    }
    if (header.message_count != 1) {
        return statusbar::failure(Ocp1Error::invalid_message_count);
    }
    return statusbar::success();
}

/// A message's fixed header, read from `reader`, whose own size field must be the body's
template <class Header>
auto fixed_header(statusbar::BufferDeserializerBuilder& reader, std::span<uint8_t const> const body) noexcept
    -> statusbar::StatusValue<Header>
{
    Header fixed{};
    reader.parse(&fixed);
    if (!reader) {
        return statusbar::failure(reader.error());
    }
    if (fixed.size != body.size()) {
        return statusbar::failure(Ocp1Error::invalid_message_size);
    }
    return fixed;
}

/// The parameters after a fixed header of `length` bytes; invalid_parameter_count if the count disagrees
auto parameters_after(uint8_t const count, std::span<uint8_t const> const body, size_t const length) noexcept
    -> statusbar::StatusValue<Parameters>
{
    Parameters const parameters{.count = count, .data = body.subspan(length)};
    if (!valid(parameters)) {
        return statusbar::failure(Ocp1Error::invalid_parameter_count);
    }
    return parameters;
}

auto parse_command(
    statusbar::BufferDeserializerBuilder& reader, std::span<uint8_t const> const body, bool const response_required) noexcept
    -> statusbar::StatusValue<Message>
{
    auto const command = fixed_header<CommandHeader>(reader, body);
    if (!command) {
        return statusbar::forward_failure(command);
    }
    auto const parameters = parameters_after(command->parameter_count, body, CommandHeader::LENGTH);
    if (!parameters) {
        return statusbar::forward_failure(parameters);
    }
    return Command{
        .handle = command->handle,
        .target = command->target,
        .method = {.def_level = command->def_level, .index = command->method_index},
        .parameters = *parameters,
        .response_required = response_required,
    };
}

auto parse_response(statusbar::BufferDeserializerBuilder& reader, std::span<uint8_t const> const body) noexcept
    -> statusbar::StatusValue<Message>
{
    auto const response = fixed_header<ResponseHeader>(reader, body);
    if (!response) {
        return statusbar::forward_failure(response);
    }
    if (response->status > static_cast<uint8_t>(OcaStatus::busy)) {
        return statusbar::failure(Ocp1Error::invalid_status);
    }
    auto const parameters = parameters_after(response->parameter_count, body, ResponseHeader::LENGTH);
    if (!parameters) {
        return statusbar::forward_failure(parameters);
    }
    return Response{
        .handle = response->handle, .status = static_cast<OcaStatus>(response->status.get()), .parameters = *parameters};
}

auto parse_notification(statusbar::BufferDeserializerBuilder& reader, std::span<uint8_t const> const body) noexcept
    -> statusbar::StatusValue<Message>
{
    auto const notification = fixed_header<Notification2Header>(reader, body);
    if (!notification) {
        return statusbar::forward_failure(notification);
    }
    if (notification->notification_type > static_cast<uint8_t>(NotificationType::exception)) {
        return statusbar::failure(Ocp1Error::invalid_notification_type);
    }
    if (body.size() == Notification2Header::LENGTH) {
        return statusbar::failure(Ocp1Error::invalid_message_size);
    }
    return Notification{
        .event =
            {.emitter = notification->emitter, .id = {.def_level = notification->def_level, .index = notification->event_index}},
        .type = static_cast<NotificationType>(notification->notification_type.get()),
        .data = body.subspan(Notification2Header::LENGTH),
    };
}

/// The message after a valid PDU header; notification1 and keep-alive are not carried
auto parse_message(
    MessageType const type, statusbar::BufferDeserializerBuilder& reader, std::span<uint8_t const> const body) noexcept
    -> statusbar::StatusValue<Message>
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
    return statusbar::failure(Ocp1Error::unsupported_message_type);
}

/// The framing of `message`, if it can be written into `buffer`: its count, size and aliasing checks
[[nodiscard]] auto checked_framing(Message const& message, std::span<uint8_t> const buffer) noexcept
    -> statusbar::StatusValue<Framing>
{
    auto const framed = framing(message);
    if (framed && PduHeader::LENGTH - 1 + framed->size > UINT32_MAX) {
        return statusbar::failure(Ocp1Error::invalid_pdu_size);
    }
    if (framed && overlaps(framed->payload, buffer)) {
        return statusbar::failure(Ocp1Error::aliased_parameters);
    }
    return framed;
}

/// Append the PDU header, the message's fixed header (its size counts the payload) and the payload
void append_pdu(statusbar::BufferSerializerBuilderWithBuffer& writer, Message const& message, Framing const& framed)
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
                        .size = size,
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
                        .size = size,
                        .handle = r.handle,
                        .status = static_cast<uint8_t>(r.status),
                        .parameter_count = r.parameters.count})
                    .append(r.parameters.data);
            },
            [&](Notification const& n) {
                writer
                    .append(Notification2Header{
                        .size = size,
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

auto pdu_size(std::span<uint8_t const> const header, size_t const maximum) noexcept -> statusbar::StatusValue<std::optional<size_t>>
{
    if (header.empty()) {
        return std::optional<size_t>{};
    }
    if (header[0] != SYNC) {
        return statusbar::failure(Ocp1Error::invalid_sync);
    }
    auto const size = declared_size(header);
    if (size && (*size < PduHeader::LENGTH || *size > maximum)) {
        return statusbar::failure(Ocp1Error::invalid_pdu_size);
    }
    return size;
}

auto parse_pdu(std::span<uint8_t const> const pdu) noexcept -> statusbar::StatusValue<Message>
{
    PduHeader header{};
    statusbar::BufferDeserializerBuilder reader{pdu};
    reader.parse(&header);
    if (!reader) {
        return statusbar::failure(reader.error());
    }
    if (auto const checked = check_header(header, pdu.size()); !checked) {
        return statusbar::forward_failure(checked);
    }
    return parse_message(static_cast<MessageType>(header.message_type.get()), reader, pdu.subspan(PduHeader::LENGTH));
}

auto serialized_pdu_size(Message const& message) noexcept -> size_t
{
    auto const message_size = std::visit(
        Overloaded{
            [](Command const& c) { return CommandHeader::LENGTH + c.parameters.data.size(); },
            [](Response const& r) { return ResponseHeader::LENGTH + r.parameters.data.size(); },
            [](Notification const& n) { return Notification2Header::LENGTH + n.data.size(); },
        },
        message);
    return PduHeader::LENGTH + message_size;
}

auto serialize_pdu(Message const& message, std::span<uint8_t> const buffer) noexcept -> statusbar::StatusValue<size_t>
{
    auto const framed = checked_framing(message, buffer);
    if (!framed) {
        return statusbar::forward_failure(framed);
    }
    statusbar::BufferSerializerBuilderWithBuffer writer{buffer};
    append_pdu(writer, message, *framed);
    if (!writer) {
        return statusbar::failure(writer.status().error());
    }
    return writer.position();
}

}  // namespace padl::ocp1
