// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

#include "statusbar/ocp1/ocp1_codec.hpp"

#include "statusbar/buffer/buffer_error.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <vector>

using namespace statusbar;
using namespace statusbar::ocp1;

namespace {

auto bytes(std::span<uint8_t const> s) -> std::vector<uint8_t>
{
    return {s.begin(), s.end()};
}

/// `values` encoded into a buffer large enough for the tests
template <typename... T>
auto encoded(T const&... values) -> std::vector<uint8_t>
{
    std::array<uint8_t, 256> buffer{};
    auto const length = encode_values(buffer, values...);
    EXPECT_TRUE(length.has_value());
    return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(length.value_or(0))};
}

enum class Availability : uint8_t
{
    unavailable = 0,
    available = 1,
};

/// OcaPortID
struct PortId
{
    uint8_t mode{};
    uint16_t index{};

    auto fields(this auto& self) noexcept { return std::tie(self.mode, self.index); }
    auto operator==(PortId const&) const -> bool = default;
};

/// OcaMediaClockRate
struct MediaClockRate
{
    float nominal_rate{};
    float pull_range{};
    float accuracy{};
    float jitter_max{};

    auto fields(this auto& self) noexcept { return std::tie(self.nominal_rate, self.pull_range, self.accuracy, self.jitter_max); }
    auto operator==(MediaClockRate const&) const -> bool = default;
};

/// A record holding an enum, a list of records and a record
struct ClockState
{
    Availability availability{};
    std::vector<MediaClockRate> supported_rates;
    MediaClockRate current_rate;

    auto fields(this auto& self) noexcept { return std::tie(self.availability, self.supported_rates, self.current_rate); }
    auto operator==(ClockState const&) const -> bool = default;
};

}  // namespace

TEST(ocp1_codec, writes_scalars_big_endian)
{
    EXPECT_EQ(encoded(uint8_t{0x12}), (std::vector<uint8_t>{0x12}));
    EXPECT_EQ(encoded(uint16_t{0x1234}), (std::vector<uint8_t>{0x12, 0x34}));
    EXPECT_EQ(encoded(uint32_t{0x12345678}), (std::vector<uint8_t>{0x12, 0x34, 0x56, 0x78}));
    EXPECT_EQ(encoded(uint64_t{0x0102030405060708}), (std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}));
    EXPECT_EQ(encoded(int16_t{-2}), (std::vector<uint8_t>{0xFF, 0xFE}));
    EXPECT_EQ(encoded(true, false), (std::vector<uint8_t>{0x01, 0x00}));
    EXPECT_EQ(encoded(48000.0F), (std::vector<uint8_t>{0x47, 0x3B, 0x80, 0x00}));
    EXPECT_EQ(encoded(1.0), (std::vector<uint8_t>{0x3F, 0xF0, 0, 0, 0, 0, 0, 0}));
    EXPECT_EQ(encoded(Availability::available), (std::vector<uint8_t>{0x01}));
}

TEST(ocp1_codec, reads_scalars_back)
{
    auto const data = encoded(int32_t{-100000}, 0.001F, -1.5, true, Availability::available);
    Decoder decoder{data};
    int32_t i{};
    float f{};
    double d{};
    bool b{};
    Availability a{};
    decoder.get(i).get(f).get(d).get(b).get(a);
    EXPECT_TRUE(static_cast<bool>(decoder));
    EXPECT_EQ(i, int32_t{-100000});
    EXPECT_EQ(f, 0.001F);
    EXPECT_EQ(d, -1.5);
    EXPECT_TRUE(b);
    EXPECT_EQ(a, Availability::available);
    EXPECT_EQ(decoder.remaining(), size_t{0});
}

TEST(ocp1_codec, counts_a_string_in_unicode_scalars)
{
    // as SwiftOCA writes it: "Bühne" is five scalars in six bytes
    EXPECT_EQ(encoded(std::string{"Bühne"}), (std::vector<uint8_t>{0x00, 0x05, 'B', 0xC3, 0xBC, 'h', 'n', 'e'}));
    // a four-byte scalar is one
    EXPECT_EQ(encoded(std::string_view{"\xF0\x9F\x8E\x9B"}), (std::vector<uint8_t>{0x00, 0x01, 0xF0, 0x9F, 0x8E, 0x9B}));
    EXPECT_EQ(encoded(std::string{}), (std::vector<uint8_t>{0x00, 0x00}));

    for (std::string const text :
         {"",
          "Bühne",
          "a\xF0\x9F\x8E\x9B"
          "b",
          "\xE2\x82\xAC 100"}) {
        auto const value = decode_value<std::string>(encoded(text));
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(value.value_or(""), text);
    }
}

TEST(ocp1_codec, refuses_strings_that_are_not_utf8)
{
    std::array<uint8_t, 32> buffer{};
    for (std::string_view const bad : {
             std::string_view{"\xC3"},              // truncated
             std::string_view{"\xC0\x80"},          // overlong NUL
             std::string_view{"\xE0\x80\xAF"},      // overlong '/'
             std::string_view{"\xED\xA0\x80"},      // surrogate
             std::string_view{"\xF4\x90\x80\x80"},  // past U+10FFFF
             std::string_view{"\xF5\x80\x80\x80"},  // invalid lead
             std::string_view{"\x80"},              // lone continuation
         }) {
        auto const result = encode_values(buffer, bad);
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), make_error_code(Ocp1Error::invalid_utf8));
    }

    // read: two scalars claimed, the second's continuation byte is not one
    std::vector<uint8_t> const bad{0x00, 0x02, 'a', 0xC3, 'b'};
    auto const value = decode_value<std::string>(bad);
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(value.error(), make_error_code(Ocp1Error::invalid_utf8));
}

TEST(ocp1_codec, a_short_string_is_a_buffer_error)
{
    for (auto const& truncated : {
             std::vector<uint8_t>{0x00, 0x03, 'a', 'b'},   // a scalar short
             std::vector<uint8_t>{0x00, 0x02, 'a', 0xC3},  // a scalar cut in two
         }) {
        auto const value = decode_value<std::string>(truncated);
        EXPECT_FALSE(value.has_value());
        EXPECT_EQ(value.error(), make_error_code(BufferError::insufficient_data));
    }
    // a string followed by more: only its scalars are taken
    std::vector<uint8_t> const followed{0x00, 0x01, 0xC3, 0xBC, 0x00, 0x07};
    Decoder decoder{followed};
    std::string text;
    uint16_t after{};
    decoder.get(text).get(after);
    EXPECT_TRUE(static_cast<bool>(decoder));
    EXPECT_EQ(text, std::string{"ü"});
    EXPECT_EQ(after, uint16_t{7});
}

TEST(ocp1_codec, writes_blobs_lists_and_maps_with_counts)
{
    EXPECT_EQ(encoded(Blob{{1, 2, 3}}), (std::vector<uint8_t>{0x00, 0x03, 1, 2, 3}));
    EXPECT_EQ(encoded(std::vector<uint16_t>{1, 2}), (std::vector<uint8_t>{0x00, 0x02, 0x00, 0x01, 0x00, 0x02}));
    // a multimap: one key twice, kept in order (OcaMultiMap<OcaUint16, OcaPortID>)
    Map<uint16_t, PortId> const channel_map{{{1, {1, 1}}, {1, {1, 2}}}};
    EXPECT_EQ(encoded(channel_map), (std::vector<uint8_t>{0x00, 0x02, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x02}));

    auto const map = decode_value<Map<uint16_t, PortId>>(encoded(channel_map));
    EXPECT_TRUE(map.has_value());
    EXPECT_TRUE(map.value_or(Map<uint16_t, PortId>{}) == channel_map);
    auto const blob = decode_value<Blob>(encoded(Blob{{9, 8, 7}}));
    EXPECT_TRUE(blob.value_or(Blob{}) == (Blob{{9, 8, 7}}));

    // OcaLongBlob: a 32-bit length
    EXPECT_EQ(encoded(LongBlob{{1, 2}}), (std::vector<uint8_t>{0x00, 0x00, 0x00, 0x02, 1, 2}));
    auto const long_blob = decode_value<LongBlob>(encoded(LongBlob{{1, 2}}));
    EXPECT_TRUE(long_blob.value_or(LongBlob{}) == (LongBlob{{1, 2}}));
}

TEST(ocp1_codec, writes_an_interval_with_its_bounds)
{
    // OcaInterval<OcaUint16> 1...8, closed
    EXPECT_EQ(encoded(Interval<uint16_t>{1, 8}), (std::vector<uint8_t>{0x00, 0x01, 0x00, 0x08, 0x00, 0x0C}));
    Interval<float> const half_open{.min = 0.0F, .max = 1.0F, .bounds = interval_bounds::MIN_INCLUSIVE};
    auto const value = decode_value<Interval<float>>(encoded(half_open));
    EXPECT_TRUE(value.value_or(Interval<float>{}) == half_open);
}

TEST(ocp1_codec, writes_a_record_as_its_fields)
{
    // as SwiftOCA writes a media clock state whose rate is unknown
    ClockState const unknown{.availability = Availability::unavailable};
    EXPECT_EQ(encoded(unknown), (std::vector<uint8_t>{0x00, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));

    ClockState const known{
        .availability = Availability::available,
        .supported_rates = {{48000.0F, 0.0F, 1e-6F, 1e-9F}, {96000.0F, 0.0F, 0.0F, 0.0F}},
        .current_rate = {48000.0F, 0.0F, 1e-6F, 1e-9F},
    };
    auto const value = decode_value<ClockState>(encoded(known));
    EXPECT_TRUE(value.has_value());
    EXPECT_TRUE(value.value_or(ClockState{}) == known);
}

TEST(ocp1_codec, leftover_bytes_are_an_error)
{
    auto const value = decode_value<uint16_t>(std::vector<uint8_t>{0x00, 0x01, 0x02});
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(value.error(), make_error_code(Ocp1Error::trailing_data));
}

TEST(ocp1_codec, a_count_past_the_data_fails_without_reading_on)
{
    // 65535 elements claimed, one there
    std::vector<uint8_t> const data{0xFF, 0xFF, 0x00, 0x01};
    auto const value = decode_value<std::vector<uint16_t>>(data);
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(value.error(), make_error_code(BufferError::insufficient_data));
}

TEST(ocp1_codec, errors_are_sticky)
{
    std::array<uint8_t, 3> buffer{};
    Encoder encoder{buffer};
    encoder.put(uint16_t{1}).put(uint16_t{2}).put(uint8_t{3});
    EXPECT_FALSE(static_cast<bool>(encoder));
    EXPECT_EQ(encoder.status().error(), make_error_code(BufferError::insufficient_space));
    EXPECT_EQ(bytes(encoder.bytes()), (std::vector<uint8_t>{0x00, 0x01}));

    std::vector<uint16_t> too_long(MAX_COUNT + 1);
    std::vector<uint8_t> large(4 * MAX_COUNT);
    auto const result = encode_values(large, too_long);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), make_error_code(Ocp1Error::too_long));
}

TEST_MAIN(statusbar_ocp1, ocp1_codec_test)
