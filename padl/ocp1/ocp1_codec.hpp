#pragma once

// Copyright 2026 PADL Software Pty Ltd
// SPDX-License-Identifier: MIT

/// OCP.1 values: the AES70-3 encoding of AES70 data types, big-endian and untagged, through an
/// `Encoder` and a `Decoder` over statusbar's buffer builders. A struct is a record by naming its
/// fields in `fields()`; the encoding rules are in docs/OCP1_MODULE.md.

#include "padl/ocp1/ocp1_error.hpp"
#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"
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

namespace padl::ocp1 {

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

/// Writes OCP.1 values into a caller's buffer. Errors are sticky: after the first, nothing more is written.
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

    /// Whether every write so far succeeded
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(writer_); }
    /// The first error, or success
    [[nodiscard]] auto status() const noexcept -> statusbar::Status { return writer_.status(); }
    /// The bytes written
    [[nodiscard]] auto position() const noexcept -> size_t { return writer_.position(); }

    /// For `encode()` overloads: the builder to append wire values to.
    [[nodiscard]] auto writer() noexcept -> statusbar::BufferSerializerBuilder& { return writer_; }
    /// For `encode()` overloads: fail with `error`, if nothing has failed yet.
    void fail(std::error_code const error) noexcept { writer_.fail(error); }

  private:
    statusbar::BufferSerializerBuilderWithBuffer writer_;
};

/// Reads OCP.1 values from a span. Errors are sticky: after the first, nothing more is read.
class Decoder
{
  public:
    explicit Decoder(std::span<uint8_t const> const input) noexcept
        : reader_{input}
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

    /// Whether every read so far succeeded
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(reader_); }
    /// The first error, or success
    [[nodiscard]] auto status() const noexcept -> statusbar::Status { return reader_.status(); }
    /// The bytes read
    [[nodiscard]] auto position() const noexcept -> size_t { return reader_.position(); }
    /// The number of bytes not yet read
    [[nodiscard]] auto available() const noexcept -> size_t { return reader_.available(); }

    /// For `decode()` overloads: the builder to parse wire values from.
    [[nodiscard]] auto reader() noexcept -> statusbar::BufferDeserializerBuilder& { return reader_; }
    /// For `decode()` overloads: the bytes not yet read.
    [[nodiscard]] auto remaining() const noexcept -> std::span<uint8_t const> { return reader_.remaining(); }
    /// For `decode()` overloads: the next `length` bytes, consumed; empty, and the
    /// decoder failed, if there are fewer.
    [[nodiscard]] auto consume(size_t length) noexcept -> std::span<uint8_t const>;
    /// For `decode()` overloads: fail with `error`, if nothing has failed yet.
    void fail(std::error_code const error) noexcept { reader_.fail(error); }

  private:
    statusbar::BufferDeserializerBuilder reader_;
};

//
// Scalars - AES70-3 OcaBoolean, OcaInt*, OcaUint*, OcaFloat*, enumerations
//

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

/// Write a scalar: its bits big-endian, a boolean as one octet, an enumeration as its declared base.
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
        encoder.writer().append(statusbar::ieee::IeeeOrderedUInt<U>{std::bit_cast<U>(value)});
    }
}

/// Read a scalar; any nonzero octet is true.
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
        statusbar::ieee::IeeeOrderedUInt<U> wire{};
        decoder.reader().parse(&wire);
        value = std::bit_cast<T>(wire.get());
    }
}

//
// Strings and blobs - AES70-3 OcaString, OcaBlob, OcaLongBlob
//

/// Write an OcaString: its count of Unicode scalars, then its UTF-8. invalid_utf8 unless `text`
/// is well-formed UTF-8, too_long past MAX_COUNT scalars.
void encode(Encoder& encoder, std::string_view text);
/// Read an OcaString: invalid_utf8 for a malformed scalar, insufficient_data for fewer than counted.
void decode(Decoder& decoder, std::string& text);

/// Write an OcaBlob: its 16-bit length, then its bytes; too_long past MAX_COUNT bytes.
void encode(Encoder& encoder, Blob const& blob);
/// Read an OcaBlob.
void decode(Decoder& decoder, Blob& blob);

/// Write an OcaLongBlob: its 32-bit length, then its bytes.
void encode(Encoder& encoder, LongBlob const& blob);
/// Read an OcaLongBlob.
void decode(Decoder& decoder, LongBlob& blob);

//
// Lists and maps - AES70-3 OcaList, OcaMap, OcaMultiMap
//

/// Write an OcaList: its 16-bit count, then its elements; too_long past MAX_COUNT elements.
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

/// Read an OcaList, reserving no more than the input could hold.
template <typename T>
void decode(Decoder& decoder, std::vector<T>& list)
{
    static_assert(!std::is_same_v<T, bool>, "std::vector<bool> has no element references");
    uint16_t count{};
    decoder.get(count);
    list.clear();
    // every element is at least a byte, so a count the input cannot hold reserves nothing extra
    list.reserve(std::min<size_t>(count, decoder.available()));
    for (uint16_t i = 0; i < count && decoder; ++i) {
        T element{};
        decoder.get(element);
        list.push_back(std::move(element));
    }
}

/// Write an OcaMap or OcaMultiMap: its 16-bit count, then each key and its value.
template <typename K, typename V>
void encode(Encoder& encoder, Map<K, V> const& map)
{
    encode(encoder, map.entries);
}

/// Read an OcaMap or OcaMultiMap, in wire order.
template <typename K, typename V>
void decode(Decoder& decoder, Map<K, V>& map)
{
    decode(decoder, map.entries);
}

//
// Records
//

/// Write a record: its fields in order.
template <Record T>
void encode(Encoder& encoder, T const& record)
{
    std::apply([&encoder](auto const&... field) { (encoder.put(field), ...); }, record.fields());
}

/// Read a record: its fields in order.
template <Record T>
void decode(Decoder& decoder, T& record)
{
    std::apply([&decoder](auto&... field) { (decoder.get(field), ...); }, record.fields());
}

/// Write a std::tuple as a record: its elements in order, as a method's parameters are.
template <typename... T>
void encode(Encoder& encoder, std::tuple<T...> const& values)
{
    std::apply([&encoder](auto const&... value) { (encoder.put(value), ...); }, values);
}

/// Read a std::tuple as a record.
template <typename... T>
void decode(Decoder& decoder, std::tuple<T...>& values)
{
    std::apply([&decoder](auto&... value) { (decoder.get(value), ...); }, values);
}

/// Write a std::pair as a record: first, then second.
template <typename A, typename B>
void encode(Encoder& encoder, std::pair<A, B> const& pair)
{
    encoder.put(pair.first).put(pair.second);
}

/// Read a std::pair as a record.
template <typename A, typename B>
void decode(Decoder& decoder, std::pair<A, B>& pair)
{
    decoder.get(pair.first).get(pair.second);
}

//
// Whole buffers
//

/// Write `values` one after another into `buffer`; the number of bytes written.
template <typename... T>
[[nodiscard]] auto encode_values(std::span<uint8_t> const buffer, T const&... values) -> statusbar::StatusValue<size_t>
{
    Encoder encoder{buffer};
    (encoder.put(values), ...);
    if (auto const status = encoder.status(); !status) {
        return statusbar::failure(status.error());
    }
    return encoder.position();
}

/// Read a `T` that is the whole of `input`: trailing bytes are an error.
template <typename T>
[[nodiscard]] auto decode_value(std::span<uint8_t const> const input) -> statusbar::StatusValue<T>
{
    Decoder decoder{input};
    T value{};
    decoder.get(value);
    if (auto const status = decoder.status(); !status) {
        return statusbar::failure(status.error());
    }
    if (decoder.available() != 0) {
        return statusbar::failure(Ocp1Error::trailing_data);
    }
    return value;
}

}  // namespace padl::ocp1
