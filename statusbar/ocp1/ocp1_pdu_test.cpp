// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "statusbar/ocp1/ocp1_pdu.hpp"

#include "statusbar/buffer/buffer_error.hpp"
#include "statusbar/ocp1/ocp1_codec.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

using namespace statusbar;
using namespace statusbar::ocp1;

namespace {

auto bytes(std::span<uint8_t const> s) -> std::vector<uint8_t>
{
    return {s.begin(), s.end()};
}

auto from_hex(std::string_view const hex) -> std::vector<uint8_t>
{
    auto const nibble = [](char const c) -> uint8_t { return static_cast<uint8_t>(c <= '9' ? c - '0' : c - 'a' + 10); };
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>((nibble(hex[i]) << 4U) | nibble(hex[i + 1])));
    }
    return out;
}

auto serialized(Message const& message) -> std::vector<uint8_t>
{
    std::array<uint8_t, 256> buffer{};
    auto const length = serialize_pdu(message, buffer);
    EXPECT_TRUE(length.has_value());
    return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(length.value_or(0))};
}

// PDUs as SwiftOCA writes them (InfernoAES70's MediaBridgeWire.txt): a command with
// no parameters to ONo 4096 method {1, 1}, handle 1; a refusal (parameterOutOfRange)
// of handle 7; an event {1, 4} from ONo 4096 carrying the OcaUint32 1001
constexpr std::string_view COMMAND = "3b00010000001a0100010000001100000001000010000001000100";
constexpr std::string_view REFUSAL = "3b0001000000130300010000000a000000070700";
constexpr std::string_view EVENT = "3b00010000001a05000100000011000010000001000400000003e9";

}  // namespace

TEST(ocp1_pdu, writes_a_command_as_swiftoca_does)
{
    Command const command{.handle = 1, .target = 4096, .method = {1, 1}};
    EXPECT_EQ(serialized(command), from_hex(COMMAND));
    EXPECT_EQ(serialized_pdu_size(command), from_hex(COMMAND).size());
}

TEST(ocp1_pdu, writes_a_refusal_and_an_event_as_swiftoca_does)
{
    EXPECT_EQ(serialized(Response{.handle = 7, .status = OcaStatus::parameter_out_of_range}), from_hex(REFUSAL));

    std::array<uint8_t, 4> data{};
    EXPECT_TRUE(encode_values(data, uint32_t{1001}).has_value());
    EXPECT_EQ(serialized(Notification{.event = {4096, {1, 4}}, .data = data}), from_hex(EVENT));
}

TEST(ocp1_pdu, parses_what_swiftoca_writes)
{
    auto const command_pdu = from_hex(COMMAND);
    auto const command = parse_pdu(command_pdu);
    EXPECT_TRUE(command.has_value() && std::holds_alternative<Command>(*command));
    auto const& c = std::get<Command>(command.value());
    EXPECT_EQ(c.handle, uint32_t{1});
    EXPECT_EQ(c.target, uint32_t{4096});
    EXPECT_TRUE(c.method == (MethodId{1, 1}));
    EXPECT_TRUE(c.response_required);
    EXPECT_EQ(c.parameters.count, uint8_t{0});
    EXPECT_TRUE(c.parameters.data.empty());

    auto const refusal_pdu = from_hex(REFUSAL);
    auto const refusal = parse_pdu(refusal_pdu);
    EXPECT_TRUE(refusal.has_value() && std::holds_alternative<Response>(*refusal));
    EXPECT_EQ(std::get<Response>(refusal.value()).handle, uint32_t{7});
    EXPECT_TRUE(std::get<Response>(refusal.value()).status == OcaStatus::parameter_out_of_range);

    auto const event_pdu = from_hex(EVENT);
    auto const event = parse_pdu(event_pdu);
    EXPECT_TRUE(event.has_value() && std::holds_alternative<Notification>(*event));
    auto const& n = std::get<Notification>(event.value());
    EXPECT_TRUE(n.event == (Event{4096, {1, 4}}));
    EXPECT_TRUE(n.type == NotificationType::event);
    EXPECT_EQ(decode_value<uint32_t>(n.data).value_or(0), uint32_t{1001});
}

TEST(ocp1_pdu, round_trips_parameters_and_a_command_wanting_no_response)
{
    std::array<uint8_t, 32> parameters{};
    auto const length = encode_values(parameters, uint32_t{7}, std::string_view{"v=0"});
    EXPECT_TRUE(length.has_value());
    Command const command{
        .handle = 9,
        .target = 4096,
        .method = {1, 14},
        .parameters = {.count = 2, .data = std::span{parameters}.first(length.value_or(0))},
        .response_required = false,
    };
    auto const pdu = serialized(command);
    EXPECT_EQ(pdu[7], uint8_t{0});  // Ocp1CommandPdu with no response
    auto const parsed = parse_pdu(pdu);
    EXPECT_TRUE(parsed.has_value());
    auto const& c = std::get<Command>(parsed.value());
    EXPECT_FALSE(c.response_required);
    EXPECT_EQ(c.parameters.count, uint8_t{2});
    EXPECT_EQ(bytes(c.parameters.data), bytes(command.parameters.data));
}

TEST(ocp1_pdu, frames_a_stream_by_its_headers)
{
    auto const pdu = from_hex(EVENT);
    auto const whole = pdu_size(pdu, 1 << 20);
    EXPECT_TRUE(whole.has_value() && whole->has_value());
    EXPECT_EQ(whole.value_or(std::nullopt).value_or(0), pdu.size());
    auto const header_only = pdu_size(std::span{pdu}.first(PduHeader::LENGTH), 1 << 20);
    EXPECT_EQ(header_only.value_or(std::nullopt).value_or(0), pdu.size());

    // not yet a whole header: wait, not an error
    auto const partial = pdu_size(std::span{pdu}.first(PduHeader::LENGTH - 1), 1 << 20);
    EXPECT_TRUE(partial.has_value());
    EXPECT_FALSE(partial->has_value());
    EXPECT_TRUE(pdu_size({}, 1 << 20).has_value());

    // a stream that can never be framed: an error, so the reader gives up
    auto const too_large = pdu_size(pdu, pdu.size() - 1);
    EXPECT_FALSE(too_large.has_value());
    EXPECT_EQ(too_large.error(), make_error_code(Ocp1Error::invalid_pdu_size));
    auto bad_sync = pdu;
    bad_sync[0] = 0x3C;
    auto const not_a_pdu = pdu_size(std::span{bad_sync}.first(1), 1 << 20);
    EXPECT_FALSE(not_a_pdu.has_value());
    EXPECT_EQ(not_a_pdu.error(), make_error_code(Ocp1Error::invalid_sync));
}

TEST(ocp1_pdu, refuses_malformed_pdus)
{
    auto const good = from_hex(REFUSAL);
    auto const refused = [&](size_t const index, uint8_t const value, Ocp1Error const error) {
        auto pdu = good;
        pdu[index] = value;
        auto const result = parse_pdu(pdu);
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), make_error_code(error));
    };
    refused(0, 0x3C, Ocp1Error::invalid_sync);
    refused(2, 0x00, Ocp1Error::unsupported_version);
    refused(6, 0x14, Ocp1Error::invalid_pdu_size);
    refused(9, 0x02, Ocp1Error::invalid_message_count);
    refused(13, 0x0B, Ocp1Error::invalid_message_size);
    refused(7, static_cast<uint8_t>(MessageType::keep_alive), Ocp1Error::unsupported_message_type);
    refused(7, static_cast<uint8_t>(MessageType::notification1), Ocp1Error::unsupported_message_type);
    refused(7, 0x09, Ocp1Error::unsupported_message_type);
    refused(18, 0x40, Ocp1Error::invalid_status);
    refused(19, 0x01, Ocp1Error::invalid_parameter_count);  // one parameter claimed, no bytes

    auto event = from_hex(EVENT);
    event[22] = 0x07;
    auto const bad_type = parse_pdu(event);
    EXPECT_FALSE(bad_type.has_value());
    EXPECT_EQ(bad_type.error(), make_error_code(Ocp1Error::invalid_notification_type));

    // a higher protocol version is accepted, as SwiftOCA accepts it: some controllers write one
    auto newer = good;
    newer[2] = 0x07;
    EXPECT_TRUE(parse_pdu(newer).has_value());

    // a notification with no data: SwiftOCA refuses one, so it is never written or read
    auto const empty_event = from_hex("3b0001000000160500010000000d000010000001000400");
    auto const no_data = parse_pdu(empty_event);
    EXPECT_FALSE(no_data.has_value());
    EXPECT_EQ(no_data.error(), make_error_code(Ocp1Error::invalid_message_size));
    std::array<uint8_t, 64> out{};
    auto const unwritten = serialize_pdu(Notification{.event = {4096, {1, 4}}}, out);
    EXPECT_FALSE(unwritten.has_value());
    EXPECT_EQ(unwritten.error(), make_error_code(Ocp1Error::invalid_message_size));

    // a PDU whose header claims the bytes it has, but too few for its message header
    auto const short_pdu = from_hex("3b0001000000100300010000000a000000");
    auto const result = parse_pdu(short_pdu);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(BufferError::insufficient_data));
}

TEST(ocp1_pdu, a_buffer_too_small_is_an_error)
{
    std::array<uint8_t, 12> buffer{};
    auto const result = serialize_pdu(Response{.handle = 1}, buffer);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(BufferError::insufficient_space));
}

TEST(ocp1_pdu, refuses_to_write_over_its_own_parameters)
{
    std::array<uint8_t, 64> buffer{};
    auto const length = encode_values(buffer, uint32_t{7});
    EXPECT_TRUE(length.has_value());
    Command const command{
        .handle = 1, .target = 4096, .method = {1, 7}, .parameters = {.count = 1, .data = std::span{buffer}.first(4)}};
    auto const result = serialize_pdu(command, buffer);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(Ocp1Error::aliased_parameters));

    // the same parameters in another buffer are fine
    std::array<uint8_t, 64> other{};
    EXPECT_TRUE(serialize_pdu(command, other).has_value());

    // a count the parameters cannot hold is refused before anything is written
    Command const miscounted{.handle = 1, .target = 4096, .method = {1, 7}, .parameters = {.count = 2, .data = {}}};
    auto const bad = serialize_pdu(miscounted, other);
    EXPECT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error(), make_error_code(Ocp1Error::invalid_parameter_count));
}

TEST(ocp1_pdu, ids_are_records)
{
    // OcaEvent as a parameter (AddSubscription2 takes one): emitter, then the id
    std::array<uint8_t, 8> buffer{};
    EXPECT_TRUE(encode_values(buffer, Event{4096, {1, 4}}).has_value());
    EXPECT_EQ(bytes(buffer), (std::vector<uint8_t>{0x00, 0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x04}));
    EXPECT_TRUE(decode_value<Event>(buffer).value_or(Event{}) == (Event{4096, {1, 4}}));
    EXPECT_TRUE(decode_value<MethodId>(std::span{buffer}.first(4)).value_or(MethodId{}) == (MethodId{0, 4096}));
}

TEST_MAIN(statusbar_ocp1, ocp1_pdu_test)
