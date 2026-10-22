// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "statusbar/ocp1/ocp1_error.hpp"

#include <string>
#include <system_error>

namespace statusbar::ocp1 {

auto Ocp1ErrorCategory::message(int ev) const -> std::string
{
    switch (static_cast<Ocp1Error>(ev)) {
        case Ocp1Error::invalid_utf8:
            return "string is not UTF-8";
        case Ocp1Error::too_long:
            return "value too long for its count";
        case Ocp1Error::trailing_data:
            return "trailing data after value";
        case Ocp1Error::invalid_sync:
            return "invalid PDU sync value";
        case Ocp1Error::unsupported_version:
            return "unsupported OCP.1 protocol version";
        case Ocp1Error::invalid_pdu_size:
            return "invalid PDU size";
        case Ocp1Error::unsupported_message_type:
            return "unsupported message type";
        case Ocp1Error::invalid_message_count:
            return "PDU does not hold exactly one message";
        case Ocp1Error::invalid_message_size:
            return "invalid message size";
        case Ocp1Error::invalid_status:
            return "invalid status code";
        case Ocp1Error::invalid_notification_type:
            return "invalid notification type";
        case Ocp1Error::invalid_parameter_count:
            return "parameter count disagrees with parameters";
        case Ocp1Error::aliased_parameters:
            return "parameters overlap the PDU buffer";
    }
    return "unknown OCP.1 error";
}

auto ocp1_error_category() noexcept -> std::error_category const&
{
    static Ocp1ErrorCategory const category;
    return category;
}

auto make_error_code(Ocp1Error e) noexcept -> std::error_code
{
    return {static_cast<int>(e), ocp1_error_category()};
}

}  // namespace statusbar::ocp1
