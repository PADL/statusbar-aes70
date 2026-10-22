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

/// A message's PDU type and size (its header and parameters or data)
struct Framing
{
    MessageType type;
    size_t size;
};

auto framing(Message const& message) noexcept -> Framing
{
    return std::visit(
        Overloaded{
            [](Command const& command) {
                return Framing{
                    command.response_required ? MessageType::command_response_required : MessageType::command,
                    CommandHeader::LENGTH + command.parameters.data.size()};
            },
            [](Response const& response) {
                return Framing{MessageType::response, ResponseHeader::LENGTH + response.parameters.data.size()};
            },
            [](Notification const& notification) {
                return Framing{MessageType::notification2, Notification2Header::LENGTH + notification.data.size()};
            },
        },
        message);
}

/// The message's parameters or data
auto payload(Message const& message) noexcept -> std::span<uint8_t const>
{
    return std::visit(
        Overloaded{
            [](Command const& command) { return command.parameters.data; },
            [](Response const& response) { return response.parameters.data; },
            [](Notification const& notification) { return notification.data; },
        },
        message);
}

auto valid(Parameters const& parameters) noexcept -> bool
{
    return parameters.count == 0 ? parameters.data.empty() : parameters.count <= parameters.data.size();
}

auto overlaps(std::span<uint8_t const> const a, std::span<uint8_t> const b) noexcept -> bool
{
    if (a.empty() || b.empty()) {
        return false;
    }
    auto const less = std::less<uint8_t const*>{};
    return less(a.data(), b.data() + b.size()) && less(b.data(), a.data() + a.size());
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
    PduHeader pdu{};
    BufferDeserializerBuilder reader{header};
    reader.parse(&pdu);
    if (!reader) {
        return std::optional<size_t>{};
    }
    size_t const size = size_t{pdu.pdu_size.get()} + 1;
    if (size < PduHeader::LENGTH || size > maximum) {
        return failure(Ocp1Error::invalid_pdu_size);
    }
    return std::optional<size_t>{size};
}

auto parse_pdu(std::span<uint8_t const> const pdu) noexcept -> StatusValue<Message>
{
    PduHeader header{};
    BufferDeserializerBuilder reader{pdu};
    reader.parse(&header);
    if (!reader) {
        return failure(reader.error());
    }
    if (header.sync != SYNC) {
        return failure(Ocp1Error::invalid_sync);
    }
    if (header.protocol_version != PROTOCOL_VERSION) {
        return failure(Ocp1Error::unsupported_version);
    }
    if (size_t{header.pdu_size.get()} + 1 != pdu.size()) {
        return failure(Ocp1Error::invalid_pdu_size);
    }
    if (header.message_count != 1) {
        return failure(Ocp1Error::invalid_message_count);
    }
    auto const body = pdu.subspan(PduHeader::LENGTH);

    switch (static_cast<MessageType>(header.message_type.get())) {
        case MessageType::command:
        case MessageType::command_response_required: {
            CommandHeader command{};
            reader.parse(&command);
            if (!reader) {
                return failure(reader.error());
            }
            if (command.command_size != body.size()) {
                return failure(Ocp1Error::invalid_message_size);
            }
            Parameters const parameters{.count = command.parameter_count, .data = body.subspan(CommandHeader::LENGTH)};
            if (!valid(parameters)) {
                return failure(Ocp1Error::invalid_parameter_count);
            }
            return Command{
                .handle = command.handle,
                .target = command.target,
                .method = {.def_level = command.def_level, .index = command.method_index},
                .parameters = parameters,
                .response_required = header.message_type == static_cast<uint8_t>(MessageType::command_response_required),
            };
        }
        case MessageType::response: {
            ResponseHeader response{};
            reader.parse(&response);
            if (!reader) {
                return failure(reader.error());
            }
            if (response.response_size != body.size()) {
                return failure(Ocp1Error::invalid_message_size);
            }
            if (response.status > static_cast<uint8_t>(OcaStatus::busy)) {
                return failure(Ocp1Error::invalid_status);
            }
            Parameters const parameters{.count = response.parameter_count, .data = body.subspan(ResponseHeader::LENGTH)};
            if (!valid(parameters)) {
                return failure(Ocp1Error::invalid_parameter_count);
            }
            return Response{
                .handle = response.handle,
                .status = static_cast<OcaStatus>(response.status.get()),
                .parameters = parameters,
            };
        }
        case MessageType::notification2: {
            Notification2Header notification{};
            reader.parse(&notification);
            if (!reader) {
                return failure(reader.error());
            }
            if (notification.notification_size != body.size()) {
                return failure(Ocp1Error::invalid_message_size);
            }
            if (notification.notification_type > static_cast<uint8_t>(NotificationType::exception)) {
                return failure(Ocp1Error::invalid_notification_type);
            }
            return Notification{
                .event =
                    {.emitter = notification.emitter,
                     .id = {.def_level = notification.def_level, .index = notification.event_index}},
                .type = static_cast<NotificationType>(notification.notification_type.get()),
                .data = body.subspan(Notification2Header::LENGTH),
            };
        }
        case MessageType::notification1:
        case MessageType::keep_alive:
            break;
    }
    return failure(Ocp1Error::unsupported_message_type);
}

auto serialized_pdu_size(Message const& message) noexcept -> size_t
{
    return PduHeader::LENGTH + framing(message).size;
}

auto serialize_pdu(Message const& message, std::span<uint8_t> const buffer) noexcept -> StatusValue<size_t>
{
    auto const [type, length] = framing(message);
    if (PduHeader::LENGTH - 1 + length > UINT32_MAX) {
        return failure(Ocp1Error::invalid_pdu_size);
    }
    if (overlaps(payload(message), buffer)) {
        return failure(Ocp1Error::aliased_parameters);
    }
    auto const* const command = std::get_if<Command>(&message);
    auto const* const response = std::get_if<Response>(&message);
    if ((command != nullptr && !valid(command->parameters)) || (response != nullptr && !valid(response->parameters))) {
        return failure(Ocp1Error::invalid_parameter_count);
    }

    PduHeader header{};
    header.sync = SYNC;
    header.protocol_version = PROTOCOL_VERSION;
    header.pdu_size = static_cast<uint32_t>(PduHeader::LENGTH - 1 + length);
    header.message_type = static_cast<uint8_t>(type);
    header.message_count = 1;

    BufferSerializerBuilderWithBuffer writer{buffer};
    writer.append(header);
    std::visit(
        Overloaded{
            [&](Command const& command) {
                CommandHeader fixed{};
                fixed.command_size = static_cast<uint32_t>(length);
                fixed.handle = command.handle;
                fixed.target = command.target;
                fixed.def_level = command.method.def_level;
                fixed.method_index = command.method.index;
                fixed.parameter_count = command.parameters.count;
                writer.append(fixed).append(command.parameters.data);
            },
            [&](Response const& response) {
                ResponseHeader fixed{};
                fixed.response_size = static_cast<uint32_t>(length);
                fixed.handle = response.handle;
                fixed.status = static_cast<uint8_t>(response.status);
                fixed.parameter_count = response.parameters.count;
                writer.append(fixed).append(response.parameters.data);
            },
            [&](Notification const& notification) {
                Notification2Header fixed{};
                fixed.notification_size = static_cast<uint32_t>(length);
                fixed.emitter = notification.event.emitter;
                fixed.def_level = notification.event.id.def_level;
                fixed.event_index = notification.event.id.index;
                fixed.notification_type = static_cast<uint8_t>(notification.type);
                writer.append(fixed).append(notification.data);
            },
        },
        message);
    if (!writer) {
        return failure(writer.status().error());
    }
    return writer.position();
}

}  // namespace statusbar::ocp1
