#pragma once

// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// OCP.1 error codes and category

#include <string>
#include <system_error>

namespace statusbar::ocp1 {

/// OCP.1 encoding and framing error codes. A value that runs past the end of its
/// buffer, or does not fit in the one being written, is a statusbar BufferError.
enum class Ocp1Error
{
    invalid_utf8 = 1,           ///< a string that is not well-formed UTF-8
    too_long,                   ///< a string, blob, list or map past its 16-bit count
    trailing_data,              ///< bytes left over after the value
    invalid_sync,               ///< a PDU not starting with 0x3B
    unsupported_version,        ///< a protocol version other than 1
    invalid_pdu_size,           ///< a PDU size that disagrees with the PDU, or is too large
    unsupported_message_type,   ///< a keep-alive or Notification1, or an unknown type
    invalid_message_count,      ///< a PDU holding other than one message
    invalid_message_size,       ///< a message size that disagrees with the PDU
    invalid_status,             ///< a response status code AES70 does not define
    invalid_notification_type,  ///< a Notification2 type other than event or exception
    invalid_parameter_count,    ///< a parameter count that disagrees with the parameters
    aliased_parameters,         ///< parameters inside the buffer the PDU is written to
};

/// Error category for OCP.1 errors
class Ocp1ErrorCategory : public std::error_category
{
  public:
    [[nodiscard]] auto name() const noexcept -> char const* override { return "statusbar.ocp1"; }

    [[nodiscard]] auto message(int ev) const -> std::string override;
};

/// Get the OCP.1 error category singleton
[[nodiscard]] auto ocp1_error_category() noexcept -> std::error_category const&;

/// Create an error_code from an Ocp1Error
/// @param e The OCP.1 error code to convert
[[nodiscard]] auto make_error_code(Ocp1Error e) noexcept -> std::error_code;

}  // namespace statusbar::ocp1

template <>
struct std::is_error_code_enum<statusbar::ocp1::Ocp1Error> : std::true_type
{};
