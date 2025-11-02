#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace wasm
{
class BinaryReader
{
public:
    explicit BinaryReader(std::span<const uint8_t> data) noexcept
        : data_(data)
    {
    }

    [[nodiscard]] bool eof() const noexcept { return offset_ >= data_.size(); }

    [[nodiscard]] size_t offset() const noexcept { return offset_; }

    [[nodiscard]] std::span<const uint8_t> data() const noexcept { return data_; }

    void set_offset(size_t offset)
    {
        if (offset > data_.size())
        {
            throw std::out_of_range("BinaryReader::set_offset");
        }
        offset_ = offset;
    }

    uint8_t read_u8()
    {
        ensure_available(1);
        return data_[offset_++];
    }

    uint32_t read_u32()
    {
        ensure_available(4);
        uint32_t value = data_[offset_] | (data_[offset_ + 1] << 8U) | (data_[offset_ + 2] << 16U) |
                         (data_[offset_ + 3] << 24U);
        offset_ += 4;
        return value;
    }

    uint32_t read_varuint1() { return read_leb_unsigned<uint32_t>(1); }
    uint32_t read_varuint7() { return read_leb_unsigned<uint32_t>(7); }
    uint32_t read_varuint32() { return read_leb_unsigned<uint32_t>(32); }

    int32_t read_varint7() { return read_leb_signed<int32_t>(7); }
    int32_t read_varint32() { return read_leb_signed<int32_t>(32); }
    int64_t read_varint64() { return read_leb_signed<int64_t>(64); }

    float read_f32()
    {
        const auto bits = read_u32();
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    double read_f64()
    {
        const auto lo = static_cast<uint64_t>(read_u32());
        const auto hi = static_cast<uint64_t>(read_u32());
        const auto bits = lo | (hi << 32U);
        double value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    void skip_bytes(size_t count)
    {
        ensure_available(count);
        offset_ += count;
    }

private:
    template <typename T>
    T read_leb_unsigned(int max_bits)
    {
        T result = 0;
        int shift = 0;
        while (true)
        {
            uint8_t byte = read_u8();
            result |= static_cast<T>(byte & 0x7F) << shift;
            if ((byte & 0x80U) == 0)
            {
                break;
            }
            shift += 7;
            if (shift >= max_bits)
            {
                throw std::runtime_error("LEB128 overflow");
            }
        }
        return result;
    }

    template <typename T>
    T read_leb_signed(int max_bits)
    {
        T result = 0;
        int shift = 0;
        uint8_t byte;
        while (true)
        {
            byte = read_u8();
            result |= static_cast<T>(byte & 0x7F) << shift;
            shift += 7;
            if ((byte & 0x80U) == 0)
            {
                break;
            }
            if (shift >= max_bits)
            {
                throw std::runtime_error("LEB128 overflow");
            }
        }

        if (shift < max_bits && (byte & 0x40U) != 0)
        {
            result |= static_cast<T>(-1) << shift;
        }
        return result;
    }

    void ensure_available(size_t count)
    {
        if (offset_ + count > data_.size())
        {
            throw std::out_of_range("BinaryReader::ensure_available");
        }
    }

    std::span<const uint8_t> data_;
    size_t offset_{0};
};
} // namespace wasm
