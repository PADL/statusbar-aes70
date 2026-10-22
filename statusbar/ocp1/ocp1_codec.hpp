#pragma once

// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// OCP.1 values - the AES70-3 encoding of AES70 data types.
///
/// Values are big-endian and untagged, written one after another. An integer or
/// enum is its fixed-width bits, a boolean one octet, a float its IEEE 754 bits.
/// A string (OcaString) is a 16-bit count of Unicode scalars and then its UTF-8;
/// a blob (OcaBlob) a 16-bit length and its bytes; a list (OcaList) a 16-bit
/// count and its elements; a map or multimap a 16-bit count of key-value pairs.
/// A record is its fields in order: a struct opts in by naming them in a
/// `fields()` member,
///
///     struct OcaMediaClockRate
///     {
///         float nominal_rate, pull_range, accuracy, jitter_max;
///         auto fields(this auto& self) { return std::tie(self.nominal_rate, ...); }
///     };
///
/// and any other type by providing `encode(Encoder&, T const&)` and
/// `decode(Decoder&, T&)` in its own namespace.
///
/// An `Encoder` writes into a caller's buffer and a `Decoder` reads a span, both
/// through statusbar's buffer builders. Errors are sticky: after the first, an
/// encoder writes nothing more and a decoder reads nothing more, and `status()`
/// reports it.

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/ocp1/ocp1_error.hpp"
#include "statusbar/status/status.hpp"

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace statusbar::ocp1 {

/// The largest count a string, blob, list or map can carry
inline constexpr size_t MAX_COUNT = 0xFFFF;

/// OcaBlob: bytes, with a 16-bit length
struct Blob
{
    std::vector<uint8_t> bytes;

    auto operator==(Blob const&) const -> bool = default;
};

/// OcaLongBlob: bytes, with a 32-bit length
struct LongBlob
{
    std::vector<uint8_t> bytes;

    auto operator==(LongBlob const&) const -> bool = default;
};

/// OcaMap and OcaMultiMap: key-value pairs in wire order. A multimap may repeat a
/// key; an OcaMap may not, which decoding does not check.
template <typename K, typename V>
struct Map
{
    std::vector<std::pair<K, V>> entries;

    auto operator==(Map const&) const -> bool = default;
};

/// OcaIntervalBounds
namespace interval_bounds {
inline constexpr uint16_t MIN_OMITTED = 1U << 0U;
inline constexpr uint16_t MAX_OMITTED = 1U << 1U;
inline constexpr uint16_t MIN_INCLUSIVE = 1U << 2U;
inline constexpr uint16_t MAX_INCLUSIVE = 1U << 3U;
inline constexpr uint16_t CLOSED = MIN_INCLUSIVE | MAX_INCLUSIVE;
}  // namespace interval_bounds

/// OcaInterval: an omitted bound still has its place on the wire
template <typename T>
struct Interval
{
    T min{};
    T max{};
    uint16_t bounds{interval_bounds::CLOSED};

    auto fields(this auto& self) noexcept { return std::tie(self.min, self.max, self.bounds); }

    auto operator==(Interval const&) const -> bool = default;
};

/// A struct encoded as its fields in order
template <typename T>
concept Record = requires(T& value) { value.fields(); };

/// Writes OCP.1 values into a buffer
class Encoder
{
  public:
    explicit Encoder(std::span<uint8_t> const buffer) noexcept
        : writer_{buffer}
    {}

    /// Write `value`, unless an earlier write failed.
    template <typename T>
    auto put(T const& value) -> Encoder&
    {
        if (*this) {
            encode(*this, value);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return !error_ && static_cast<bool>(writer_); }
    [[nodiscard]] auto status() const noexcept -> Status;
    [[nodiscard]] auto position() const noexcept -> size_t { return writer_.position(); }
    /// What has been written
    [[nodiscard]] auto bytes() const noexcept -> std::span<uint8_t const> { return writer_.get_span(); }

    /// For `encode()` overloads: the builder to append wire values to.
    [[nodiscard]] auto writer() noexcept -> BufferSerializerBuilder& { return writer_; }
    /// For `encode()` overloads: fail, if nothing has failed yet.
    void fail(Ocp1Error const error) noexcept
    {
        if (!error_) {
            error_ = error;
        }
    }

  private:
    BufferSerializerBuilderWithBuffer writer_;
    std::error_code error_{};
};

/// Reads OCP.1 values from a span
class Decoder
{
  public:
    explicit Decoder(std::span<uint8_t const> const input) noexcept
        : input_{input}
        , reader_{input}
    {}

    /// Read `value`, unless an earlier read failed.
    template <typename T>
    auto get(T& value) -> Decoder&
    {
        if (*this) {
            decode(*this, value);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return !error_ && static_cast<bool>(reader_); }
    [[nodiscard]] auto status() const noexcept -> Status;
    [[nodiscard]] auto position() const noexcept -> size_t { return reader_.position(); }
    [[nodiscard]] auto remaining() const noexcept -> size_t { return input_.size() - position(); }

    /// For `decode()` overloads: the builder to parse wire values from.
    [[nodiscard]] auto reader() noexcept -> BufferDeserializerBuilder& { return reader_; }
    /// For `decode()` overloads: the bytes not yet read.
    [[nodiscard]] auto rest() const noexcept -> std::span<uint8_t const> { return input_.subspan(position()); }
    /// For `decode()` overloads: the next `length` bytes, consumed; empty, and the
    /// decoder failed, if there are fewer.
    [[nodiscard]] auto take(size_t length) noexcept -> std::span<uint8_t const>;
    /// For `decode()` overloads: fail, if nothing has failed yet.
    void fail(Ocp1Error const error) noexcept
    {
        if (!error_) {
            error_ = error;
        }
    }

  private:
    std::span<uint8_t const> input_;
    BufferDeserializerBuilder reader_;
    std::error_code error_{};
};

// ---- scalars ----

/// The unsigned integer as wide as `T`
template <typename T>
using UnsignedOfSize = std::conditional_t<
    sizeof(T) == 1,
    uint8_t,
    std::conditional_t<sizeof(T) == 2, uint16_t, std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>>>;

/// A fixed-width integer, boolean or float: one whose wire width is the same on
/// every platform (`long` and `size_t` are not, unless they are a <cstdint> type)
template <typename T>
concept FixedWidth = std::same_as<T, bool> || std::same_as<T, uint8_t> || std::same_as<T, uint16_t> || std::same_as<T, uint32_t> ||
    std::same_as<T, uint64_t> || std::same_as<T, int8_t> || std::same_as<T, int16_t> || std::same_as<T, int32_t> ||
    std::same_as<T, int64_t> || std::same_as<T, float> || std::same_as<T, double>;

/// An AES70 enumeration: an enum declared over OcaUint8 or OcaUint16 (AES70 has no
/// wider ones). An enum with no declared base is `int` and is refused, since it would
/// go out as four bytes. Decoding does not check that the value names an enumerator.
template <typename T>
concept Enumeration =
    std::is_enum_v<T> && (std::same_as<std::underlying_type_t<T>, uint8_t> || std::same_as<std::underlying_type_t<T>, uint16_t>);

/// A fixed-width integer, boolean or float, or an AES70 enumeration
template <typename T>
concept Scalar = FixedWidth<T> || Enumeration<T>;

template <Scalar T>
void encode(Encoder& encoder, T const value) noexcept
{
    if constexpr (std::is_enum_v<T>) {
        encode(encoder, std::to_underlying(value));
    } else if constexpr (std::is_same_v<T, bool>) {
        encode(encoder, static_cast<uint8_t>(value ? 1U : 0U));
    } else {
        static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);
        using U = UnsignedOfSize<T>;
        encoder.writer().append(ieee::IeeeOrderedUInt<U>{std::bit_cast<U>(value)});
    }
}

template <Scalar T>
void decode(Decoder& decoder, T& value) noexcept
{
    if constexpr (std::is_enum_v<T>) {
        std::underlying_type_t<T> raw{};
        decode(decoder, raw);
        value = static_cast<T>(raw);
    } else if constexpr (std::is_same_v<T, bool>) {
        uint8_t raw{};
        decode(decoder, raw);
        value = raw != 0;
    } else {
        using U = UnsignedOfSize<T>;
        ieee::IeeeOrderedUInt<U> wire{};
        decoder.reader().parse(&wire);
        value = std::bit_cast<T>(wire.get());
    }
}

// ---- strings and blobs ----

/// An OcaString: fails with invalid_utf8 unless `text` is well-formed UTF-8.
void encode(Encoder& encoder, std::string_view text);
void decode(Decoder& decoder, std::string& text);

void encode(Encoder& encoder, Blob const& blob);
void decode(Decoder& decoder, Blob& blob);

void encode(Encoder& encoder, LongBlob const& blob);
void decode(Decoder& decoder, LongBlob& blob);

// ---- lists and maps ----

template <typename T>
void encode(Encoder& encoder, std::vector<T> const& list)
{
    static_assert(!std::is_same_v<T, bool>, "std::vector<bool> has no element references");
    if (list.size() > MAX_COUNT) {
        encoder.fail(Ocp1Error::too_long);
        return;
    }
    encoder.put(static_cast<uint16_t>(list.size()));
    for (auto const& element : list) {
        encoder.put(element);
    }
}

template <typename T>
void decode(Decoder& decoder, std::vector<T>& list)
{
    static_assert(!std::is_same_v<T, bool>, "std::vector<bool> has no element references");
    uint16_t count{};
    decoder.get(count);
    list.clear();
    // every element is at least a byte, so a count the input cannot hold reserves nothing extra
    list.reserve(std::min<size_t>(count, decoder.remaining()));
    for (uint16_t i = 0; i < count && decoder; ++i) {
        T element{};
        decoder.get(element);
        list.push_back(std::move(element));
    }
}

template <typename K, typename V>
void encode(Encoder& encoder, Map<K, V> const& map)
{
    if (map.entries.size() > MAX_COUNT) {
        encoder.fail(Ocp1Error::too_long);
        return;
    }
    encoder.put(static_cast<uint16_t>(map.entries.size()));
    for (auto const& [key, value] : map.entries) {
        encoder.put(key).put(value);
    }
}

template <typename K, typename V>
void decode(Decoder& decoder, Map<K, V>& map)
{
    uint16_t count{};
    decoder.get(count);
    map.entries.clear();
    map.entries.reserve(std::min<size_t>(count, decoder.remaining()));
    for (uint16_t i = 0; i < count && decoder; ++i) {
        std::pair<K, V> entry{};
        decoder.get(entry.first).get(entry.second);
        map.entries.push_back(std::move(entry));
    }
}

// ---- records ----

template <Record T>
void encode(Encoder& encoder, T const& record)
{
    std::apply([&encoder](auto const&... field) { (encoder.put(field), ...); }, record.fields());
}

template <Record T>
void decode(Decoder& decoder, T& record)
{
    std::apply([&decoder](auto&... field) { (decoder.get(field), ...); }, record.fields());
}

// ---- whole buffers ----

/// Write `values` one after another into `buffer`; the number of bytes written.
template <typename... T>
[[nodiscard]] auto encode_values(std::span<uint8_t> const buffer, T const&... values) -> StatusValue<size_t>
{
    Encoder encoder{buffer};
    (encoder.put(values), ...);
    if (auto const status = encoder.status(); !status) {
        return failure(status.error());
    }
    return encoder.position();
}

/// Read a `T` that is the whole of `input`: trailing bytes are an error.
template <typename T>
[[nodiscard]] auto decode_value(std::span<uint8_t const> const input) -> StatusValue<T>
{
    Decoder decoder{input};
    T value{};
    decoder.get(value);
    if (auto const status = decoder.status(); !status) {
        return failure(status.error());
    }
    if (decoder.remaining() != 0) {
        return failure(Ocp1Error::trailing_data);
    }
    return value;
}

}  // namespace statusbar::ocp1
