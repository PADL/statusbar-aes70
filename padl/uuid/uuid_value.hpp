#pragma once

// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// A UUID (RFC 9562) as its 16 octets, its lower-case text form, and the name-based version 5 UUID
/// (SHA-1, RFC 9562 Section 5.5) that derives the same UUID from the same namespace and name every time.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace padl::uuid {

/// The octets of a UUID, and the characters of its 8-4-4-4-12 text form (RFC 9562 Section 4).
inline constexpr size_t UUID_SIZE = 16;
inline constexpr size_t TEXT_LENGTH = 36;
/// The octet holding the version in its high nibble (RFC 9562 Section 4.2), and the one holding the variant.
inline constexpr size_t VERSION_OCTET = 6;
inline constexpr size_t VARIANT_OCTET = 8;
/// The version field, the high nibble of its octet (RFC 9562 Section 4.2), and the variant field, the two high bits of
/// its octet (Section 4.1).
inline constexpr uint8_t VERSION_MASK = 0xF0;
inline constexpr unsigned VERSION_SHIFT = 4;
inline constexpr uint8_t VARIANT_MASK = 0xC0;
/// The name-based SHA-1 version (RFC 9562 Section 5.5), and the variant's bits, 10 (Section 4.1).
inline constexpr uint8_t VERSION_NAME_SHA1 = 5;
inline constexpr uint8_t VARIANT_RFC9562 = 0x80;

/// A UUID's text form, held in place.
struct UuidText
{
    std::array<char, TEXT_LENGTH> chars{};

    /// The text; valid while this object lives.
    [[nodiscard]] auto view() const noexcept -> std::string_view { return {chars.data(), chars.size()}; }
};

/// A UUID: its octets in network order (RFC 9562 Section 4).
struct Uuid
{
    std::array<uint8_t, UUID_SIZE> octets{};

    /// The version field (RFC 9562 Section 4.2).
    [[nodiscard]] constexpr auto version() const noexcept -> uint8_t { return octets[VERSION_OCTET] >> VERSION_SHIFT; }
    /// The text form, lower case, as RFC 9562 Section 4 has it written.
    [[nodiscard]] auto text() const noexcept -> UuidText;

    auto operator==(Uuid const&) const noexcept -> bool = default;
};

/// The name-based version 5 UUID of `name` in `space` (RFC 9562 Section 5.5): the first 16 octets of the SHA-1 of
/// the namespace's octets then the name's, with the version and variant set.
[[nodiscard]] auto name_based_v5(Uuid const& space, std::string_view name) noexcept -> Uuid;

/// The namespace IDs of RFC 9562 Section 6.6: for a domain name, and for a URL.
inline constexpr Uuid NAMESPACE_DNS{
    {0x6B, 0xA7, 0xB8, 0x10, 0x9D, 0xAD, 0x11, 0xD1, 0x80, 0xB4, 0x00, 0xC0, 0x4F, 0xD4, 0x30, 0xC8}};
inline constexpr Uuid NAMESPACE_URL{
    {0x6B, 0xA7, 0xB8, 0x11, 0x9D, 0xAD, 0x11, 0xD1, 0x80, 0xB4, 0x00, 0xC0, 0x4F, 0xD4, 0x30, 0xC8}};

}  // namespace padl::uuid
