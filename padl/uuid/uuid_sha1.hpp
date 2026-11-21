#pragma once

// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// SHA-1 (RFC 3174), for name-based version 5 UUIDs (RFC 9562 Section 5.5) and nothing that needs a secure hash:
/// SHA-1 is broken for collision resistance (RFC 6194).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace padl::uuid {

/// The octets of a SHA-1 block and of its digest (RFC 3174 Section 2).
inline constexpr size_t SHA1_BLOCK_SIZE = 64;
inline constexpr size_t SHA1_DIGEST_SIZE = 20;
/// The 32-bit words of SHA-1's state, H0 to H4 (RFC 3174 Section 6.1), which make up the digest.
inline constexpr size_t SHA1_STATE_WORDS = SHA1_DIGEST_SIZE / sizeof(uint32_t);

using Sha1Digest = std::array<uint8_t, SHA1_DIGEST_SIZE>;

/// An incremental SHA-1: update() with the message in any number of pieces, then finish(), which consumes the hash
/// (call it on an rvalue: `std::move(hash).finish()`), so a finished hash is never updated or finished again.
class Sha1
{
  public:
    /// Adds `data` to the message.
    void update(std::span<uint8_t const> data) noexcept;
    /// The digest of the message given so far.
    [[nodiscard]] auto finish() && noexcept -> Sha1Digest;

  private:
    /// Adds the buffered block to the state (RFC 3174 Section 6.1).
    void compress() noexcept;
    /// The padding before the length: a 1 bit, then 0 bits to eight octets short of a block (RFC 3174 Section 4).
    void pad() noexcept;
    /// Moves up to a block's worth of `data` into the buffer, compressing a full one; the octets used.
    [[nodiscard]] auto absorb(std::span<uint8_t const> data) noexcept -> size_t;

    std::array<uint32_t, SHA1_STATE_WORDS> state_{0x6745'2301U, 0xEFCD'AB89U, 0x98BA'DCFEU, 0x1032'5476U, 0xC3D2'E1F0U};
    std::array<uint8_t, SHA1_BLOCK_SIZE> block_{};
    size_t buffered_{0};
    uint64_t length_{0};  ///< octets of the message
};

/// The SHA-1 digest of `message`.
[[nodiscard]] auto sha1(std::span<uint8_t const> message) noexcept -> Sha1Digest;

}  // namespace padl::uuid
