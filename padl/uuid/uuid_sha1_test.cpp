// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// SHA-1 against RFC 3174 Section 7.3's test vectors and Python's hashlib.sha1 at the padding's edges, whole and
/// in pieces.

#include "padl/uuid/uuid_sha1.hpp"

#include "statusbar/buffer/span_utils.hpp"
#include "statusbar/test/test.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>

using padl::uuid::Sha1;
using padl::uuid::Sha1Digest;

namespace {

auto hex_for(Sha1Digest const& digest) -> std::string
{
    std::string text;
    for (auto const octet : digest) {
        text += std::format("{:02x}", octet);
    }
    return text;
}

auto sha1_for(std::string_view const message) -> std::string
{
    return hex_for(padl::uuid::sha1(statusbar::make_const_span(message)));
}

/// Whether finish() can be called on a `T`: only on an rvalue, so a finished hash is never reused.
template <typename T>
concept Finishable = requires(T hash) { std::forward<T>(hash).finish(); };

static_assert(Finishable<Sha1&&>);
static_assert(!Finishable<Sha1&>);

}  // namespace

TEST(uuid_sha1, the_rfc_3174_test_vectors_hash_as_published)
{
    EXPECT_EQ(sha1_for("abc"), std::string{"a9993e364706816aba3e25717850c26c9cd0d89d"});
    EXPECT_EQ(
        sha1_for("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
        std::string{"84983e441c3bd26ebaae4aa1f95129e5e54670f1"});
    EXPECT_EQ(sha1_for(std::string(1'000'000, 'a')), std::string{"34aa973cd4c4daa4f61eeb2bdbad27316534016f"});
    std::string repeated;
    for (int i = 0; i < 80; ++i) {
        repeated += "01234567";
    }
    EXPECT_EQ(sha1_for(repeated), std::string{"dea356a2cddd90c7a7ecedc5ebb563934f460452"});
}

// The empty message, and the lengths at which the length field moves to a block of its own.
TEST(uuid_sha1, messages_at_the_padding_s_edges_hash_as_python_s)
{
    EXPECT_EQ(sha1_for(""), std::string{"da39a3ee5e6b4b0d3255bfef95601890afd80709"});
    EXPECT_EQ(sha1_for(std::string(55, 'a')), std::string{"c1c8bbdc22796e28c0e15163d20899b65621d65a"});
    EXPECT_EQ(sha1_for(std::string(56, 'a')), std::string{"c2db330f6083854c99d4b5bfb6e8f29f201be699"});
    EXPECT_EQ(sha1_for(std::string(64, 'a')), std::string{"0098ba824b5c16427bd7a1122a5a442a25ec644d"});
}

TEST(uuid_sha1, a_message_given_in_pieces_hashes_as_whole)
{
    std::string const message(1000, 'a');
    for (size_t const piece : {1UZ, 7UZ, 63UZ, 64UZ, 65UZ}) {
        Sha1 hash;
        for (size_t at = 0; at < message.size(); at += piece) {
            hash.update(statusbar::make_const_span(std::string_view{message}.substr(at, piece)));
        }
        auto const digest = std::move(hash).finish();
        EXPECT_EQ(hex_for(digest), sha1_for(message));
    }
}

TEST_MAIN(padl_uuid, uuid_sha1_test)
