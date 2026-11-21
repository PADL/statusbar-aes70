// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "padl/uuid/uuid_sha1.hpp"

#include "statusbar/ieee/ieee_base.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <utility>

namespace padl::uuid {

namespace {

using statusbar::ieee::octlet_t;
using statusbar::ieee::quadlet_t;

constexpr size_t WORDS_PER_BLOCK = SHA1_BLOCK_SIZE / sizeof(uint32_t);
constexpr size_t ROUNDS = 80;
/// The padding's first octet, 1 then 0 bits (RFC 3174 Section 4).
constexpr uint8_t PAD_FIRST = 0x80;
/// Where a block's bit length goes: its last eight octets.
constexpr size_t LENGTH_OFFSET = SHA1_BLOCK_SIZE - sizeof(uint64_t);

static_assert(sizeof(quadlet_t) == sizeof(uint32_t) && sizeof(octlet_t) == sizeof(uint64_t));

/// f(t;B,C,D) + K(t) of round `t` (RFC 3174 Section 5).
constexpr auto round_function(size_t const t, uint32_t const b, uint32_t const c, uint32_t const d) noexcept -> uint32_t
{  // NOLINT(statusbar-block-statements): RFC 3174 Section 5's four rounds, as a table
    if (t < 20) {
        return ((b & c) | (~b & d)) + 0x5A82'7999U;
    }
    if (t < 40) {
        return (b ^ c ^ d) + 0x6ED9'EBA1U;
    }
    if (t < 60) {
        return ((b & c) | (b & d) | (c & d)) + 0x8F1B'BCDCU;
    }
    return (b ^ c ^ d) + 0xCA62'C1D6U;
}

/// W(0) to W(79) of `block` (RFC 3174 Section 6.1 steps a and b).
auto schedule(std::array<uint8_t, SHA1_BLOCK_SIZE> const& block) noexcept -> std::array<uint32_t, ROUNDS>
{
    auto const words = std::bit_cast<std::array<quadlet_t, WORDS_PER_BLOCK>>(block);
    std::array<uint32_t, ROUNDS> w{};
    std::ranges::transform(words, w.begin(), [](quadlet_t const word) { return word.get(); });
    for (size_t t = WORDS_PER_BLOCK; t < ROUNDS; ++t) {
        w[t] = std::rotl(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
    }
    return w;
}

/// The state as the digest's octets, each word big-endian (RFC 3174 Section 6.1).
auto digest_for(std::array<uint32_t, SHA1_STATE_WORDS> const& state) noexcept -> Sha1Digest
{
    std::array<quadlet_t, SHA1_STATE_WORDS> words{};
    std::ranges::copy(state, words.begin());
    return std::bit_cast<Sha1Digest>(words);
}

}  // namespace

void Sha1::compress() noexcept
{
    auto const w = schedule(block_);
    auto v = state_;  // A to E
    for (size_t t = 0; t < ROUNDS; ++t) {
        uint32_t const temp = std::rotl(v[0], 5) + round_function(t, v[1], v[2], v[3]) + v[4] + w[t];
        v = {temp, v[0], std::rotl(v[1], 30), v[2], v[3]};
    }
    std::ranges::transform(state_, v, state_.begin(), std::plus{});
    buffered_ = 0;
}

auto Sha1::absorb(std::span<uint8_t const> const data) noexcept -> size_t
{
    size_t const count = std::min(data.size(), SHA1_BLOCK_SIZE - buffered_);
    std::ranges::copy(data.first(count), std::span{block_}.subspan(buffered_).begin());
    buffered_ += count;
    if (buffered_ == SHA1_BLOCK_SIZE) {
        compress();
    }
    return count;
}

void Sha1::update(std::span<uint8_t const> data) noexcept
{
    length_ += data.size();
    while (!data.empty()) {
        data = data.subspan(absorb(data));
    }
}

void Sha1::pad() noexcept
{
    std::array<uint8_t, 1> const one{PAD_FIRST};
    (void)absorb(one);
    std::array<uint8_t, 1> const zero{};
    while (buffered_ != LENGTH_OFFSET) {
        (void)absorb(zero);
    }
}

auto Sha1::finish() && noexcept -> Sha1Digest
{
    octlet_t const bits{length_ * 8};
    pad();
    (void)absorb(std::bit_cast<std::array<uint8_t, sizeof(octlet_t)>>(bits));
    return digest_for(state_);
}

auto sha1(std::span<uint8_t const> const message) noexcept -> Sha1Digest
{
    Sha1 hash;
    hash.update(message);
    return std::move(hash).finish();
}

}  // namespace padl::uuid
