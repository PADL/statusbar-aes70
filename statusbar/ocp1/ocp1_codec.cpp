// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "statusbar/ocp1/ocp1_codec.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace statusbar::ocp1 {

namespace {

constexpr auto is_continuation(uint8_t const byte) noexcept -> bool
{
    return (byte & 0xC0U) == 0x80U;
}

/// The length of the UTF-8 sequence `lead` begins, or 0 if it cannot begin one.
constexpr auto sequence_length(uint8_t const lead) noexcept -> size_t
{
    if (lead < 0x80U) {
        return 1;
    }
    if (lead >= 0xC2U && lead <= 0xDFU) {
        return 2;
    }
    if (lead >= 0xE0U && lead <= 0xEFU) {
        return 3;
    }
    if (lead >= 0xF0U && lead <= 0xF4U) {
        return 4;
    }
    return 0;  // a continuation byte, an overlong 2-byte lead, or past U+10FFFF
}

/// Whether `sequence`, of the length its lead byte gives, is one well-formed
/// scalar (RFC 3629): continuation bytes, and no overlong form, surrogate or
/// value past U+10FFFF.
constexpr auto is_scalar(std::span<uint8_t const> const sequence) noexcept -> bool
{
    for (size_t i = 1; i < sequence.size(); ++i) {
        if (!is_continuation(sequence[i])) {
            return false;
        }
    }
    if (sequence.size() < 3) {
        return true;
    }
    uint8_t const lead = sequence[0];
    uint8_t const second = sequence[1];
    switch (lead) {
        case 0xE0U:
            return second >= 0xA0U;  // overlong below U+0800
        case 0xEDU:
            return second < 0xA0U;  // surrogates
        case 0xF0U:
            return second >= 0x90U;  // overlong below U+10000
        case 0xF4U:
            return second < 0x90U;  // past U+10FFFF
        default:
            return true;
    }
}

/// What a walk over UTF-8 found
struct Scan
{
    size_t scalars{0};       ///< whole scalars walked
    size_t bytes{0};         ///< the bytes they take
    bool well_formed{true};  ///< false if the walk stopped at a malformed sequence
    bool truncated{false};   ///< true if it stopped at a sequence cut short
};

/// Walk the scalars at the start of `bytes`, at most `limit` of them, stopping at
/// the end of `bytes` or at the first sequence that is not a well-formed scalar.
auto scan(std::span<uint8_t const> const bytes, size_t const limit) noexcept -> Scan
{
    Scan result;
    while (result.scalars < limit && result.bytes < bytes.size()) {
        size_t const length = sequence_length(bytes[result.bytes]);
        if (length == 0) {
            result.well_formed = false;
            break;
        }
        if (length > bytes.size() - result.bytes) {
            result.truncated = true;
            break;
        }
        if (!is_scalar(bytes.subspan(result.bytes, length))) {
            result.well_formed = false;
            break;
        }
        result.bytes += length;
        ++result.scalars;
    }
    return result;
}

}  // namespace

auto Encoder::status() const noexcept -> Status
{
    if (error_) {
        return failure(error_);
    }
    return writer_.status();
}

auto Decoder::status() const noexcept -> Status
{
    if (error_) {
        return failure(error_);
    }
    return reader_.status();
}

auto Decoder::take(size_t const length) noexcept -> std::span<uint8_t const>
{
    size_t const start = position();
    reader_.skip(length);
    if (!*this) {
        return {};
    }
    return input_.subspan(start, length);
}

void encode(Encoder& encoder, std::string_view const text)
{
    auto const bytes = std::span{reinterpret_cast<uint8_t const*>(text.data()), text.size()};
    auto const walk = scan(bytes, SIZE_MAX);
    if (walk.bytes != bytes.size()) {
        encoder.fail(Ocp1Error::invalid_utf8);
        return;
    }
    if (walk.scalars > MAX_COUNT) {
        encoder.fail(Ocp1Error::too_long);
        return;
    }
    encoder.put(static_cast<uint16_t>(walk.scalars));
    encoder.writer().append(bytes);
}

void decode(Decoder& decoder, std::string& text)
{
    // the count is of scalars, so the byte length is known only once they are walked
    uint16_t count{};
    decoder.get(count);
    text.clear();
    if (!decoder) {
        return;
    }
    auto const rest = decoder.rest();
    auto const walk = scan(rest, count);
    if (!walk.well_formed) {
        decoder.fail(Ocp1Error::invalid_utf8);
        return;
    }
    // fewer scalars than counted: asking for more than there is fails as a short buffer
    auto const bytes = decoder.take(walk.scalars == count ? walk.bytes : rest.size() + 1);
    text.assign(reinterpret_cast<char const*>(bytes.data()), bytes.size());
}

void encode(Encoder& encoder, Blob const& blob)
{
    if (blob.bytes.size() > MAX_COUNT) {
        encoder.fail(Ocp1Error::too_long);
        return;
    }
    encoder.put(static_cast<uint16_t>(blob.bytes.size()));
    encoder.writer().append(std::span<uint8_t const>{blob.bytes});
}

void decode(Decoder& decoder, Blob& blob)
{
    uint16_t length{};
    decoder.get(length);
    auto const bytes = decoder.take(length);
    blob.bytes.assign(bytes.begin(), bytes.end());
}

void encode(Encoder& encoder, LongBlob const& blob)
{
    if (blob.bytes.size() > UINT32_MAX) {
        encoder.fail(Ocp1Error::too_long);
        return;
    }
    encoder.put(static_cast<uint32_t>(blob.bytes.size()));
    encoder.writer().append(std::span<uint8_t const>{blob.bytes});
}

void decode(Decoder& decoder, LongBlob& blob)
{
    uint32_t length{};
    decoder.get(length);
    auto const bytes = decoder.take(length);
    blob.bytes.assign(bytes.begin(), bytes.end());
}

}  // namespace statusbar::ocp1
