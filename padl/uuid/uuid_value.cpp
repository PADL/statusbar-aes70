// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "padl/uuid/uuid_value.hpp"

#include "padl/uuid/uuid_sha1.hpp"
#include "statusbar/buffer/span_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace padl::uuid {

namespace {

constexpr std::string_view HEX_DIGITS = "0123456789abcdef";

/// Whether the text form has a dash before octet `i`: 4, 6, 8 and 10.
constexpr auto dash_before(size_t const i) noexcept -> bool
{
    return i >= 4 && i <= 10 && i % 2 == 0;
}

/// The UUID of `digest`'s first octets with `version` and the RFC 9562 variant set (Section 5.5).
auto from_digest(Sha1Digest const& digest, uint8_t const version) noexcept -> Uuid
{
    Uuid uuid;
    std::ranges::copy(std::span{digest}.first<UUID_SIZE>(), uuid.octets.begin());
    auto& version_octet = uuid.octets[VERSION_OCTET];
    version_octet = static_cast<uint8_t>((version_octet & ~VERSION_MASK) | (version << VERSION_SHIFT));
    uuid.octets[VARIANT_OCTET] = static_cast<uint8_t>((uuid.octets[VARIANT_OCTET] & ~VARIANT_MASK) | VARIANT_RFC9562);
    return uuid;
}

}  // namespace

auto Uuid::text() const noexcept -> UuidText
{
    UuidText text;
    size_t at = 0;
    for (size_t i = 0; i < octets.size(); ++i) {
        if (dash_before(i)) {
            text.chars[at++] = '-';
        }
        text.chars[at++] = HEX_DIGITS[octets[i] >> 4U];
        text.chars[at++] = HEX_DIGITS[octets[i] & 0x0FU];
    }
    return text;
}

auto name_based_v5(Uuid const& space, std::string_view const name) noexcept -> Uuid
{
    Sha1 hash;
    hash.update(space.octets);
    hash.update(statusbar::make_const_span(name));
    return from_digest(std::move(hash).finish(), VERSION_NAME_SHA1);
}

}  // namespace padl::uuid
