// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// libFuzzer harness for the OCP.1 PDU parser and framer: they may not crash or read out of bounds,
/// next_pdu() frames what pdu_size() sizes, command_handle() agrees with a parsed command, and a PDU
/// that parses serializes to its own bytes (bar the protocol version, which is written as 1).

#include "padl/ocp1/ocp1.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <variant>

namespace {

constexpr size_t MAXIMUM = 4096;

/// next_pdu() is the whole PDU pdu_size() gives, once all of it is there, and fails as it fails.
void check_framing(std::span<uint8_t const> const bytes)
{
    auto const size = padl::ocp1::pdu_size(bytes, MAXIMUM);
    auto const pdu = padl::ocp1::next_pdu(bytes, MAXIMUM);
    if (size.has_value() != pdu.has_value()) {
        std::abort();
    }
    auto const whole = size.value_or(std::nullopt).value_or(SIZE_MAX);
    bool const arrived = bytes.size() >= whole;
    if (pdu && (arrived ? pdu->size() != whole || pdu->data() != bytes.data() : !pdu->empty())) {
        std::abort();
    }
}

/// A parsed command wanting a response has the handle command_handle() reads.
void check_handle(std::span<uint8_t const> const bytes, padl::ocp1::Message const& message)
{
    auto const* command = std::get_if<padl::ocp1::Command>(&message);
    auto const handle = padl::ocp1::command_handle(bytes);
    bool const wanted = command != nullptr && command->response_required;
    if (wanted != handle.has_value() || (wanted && *handle != command->handle)) {
        std::abort();
    }
}

/// `message` serializes, to `bytes` with its protocol version 1.
void check_round_trip(std::span<uint8_t const> const bytes, padl::ocp1::Message const& message)
{
    std::array<uint8_t, MAXIMUM> out{};
    auto const length = padl::ocp1::serialize_pdu(message, out);
    if (!length || *length != bytes.size() || padl::ocp1::serialized_pdu_size(message) != bytes.size()) {
        std::abort();
    }
    for (size_t i = 0; i < bytes.size(); ++i) {
        bool const version = i == 1 || i == 2;
        if (!version && out[i] != bytes[i]) {
            std::abort();
        }
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(uint8_t const* data, size_t size)
{
    std::span<uint8_t const> const bytes{data, size};
    check_framing(bytes);
    auto const message = padl::ocp1::parse_pdu(bytes);
    if (!message) {
        return 0;
    }
    check_handle(bytes, *message);
    if (bytes.size() <= MAXIMUM) {
        check_round_trip(bytes, *message);
    }
    return 0;
}
