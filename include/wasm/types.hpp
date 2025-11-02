#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace wasm
{
enum class ValueType : uint8_t
{
    I32 = 0x7F,
    I64 = 0x7E,
    F32 = 0x7D,
    F64 = 0x7C,
    FuncRef = 0x70,
    ExternRef = 0x6F,
};

inline std::string to_string(ValueType type)
{
    switch (type)
    {
    case ValueType::I32:
        return "i32";
    case ValueType::I64:
        return "i64";
    case ValueType::F32:
        return "f32";
    case ValueType::F64:
        return "f64";
    case ValueType::FuncRef:
        return "funcref";
    case ValueType::ExternRef:
        return "externref";
    default:
        return "unknown";
    }
}

struct Value
{
    ValueType type{ValueType::I32};
    union Storage
    {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        uint64_t ref;

        Storage() : i32(0) {}
    } storage;

    bool ref_is_null{false};

    Value() = default;

    explicit Value(int32_t v)
        : type(ValueType::I32)
    {
        storage.i32 = v;
        ref_is_null = false;
    }

    explicit Value(int64_t v)
        : type(ValueType::I64)
    {
        storage.i64 = v;
        ref_is_null = false;
    }

    explicit Value(float v)
        : type(ValueType::F32)
    {
        storage.f32 = v;
        ref_is_null = false;
    }

    explicit Value(double v)
        : type(ValueType::F64)
    {
        storage.f64 = v;
        ref_is_null = false;
    }

    template <typename T>
    static Value make(T v)
    {
        if constexpr (std::is_same_v<T, int32_t>)
        {
            return Value(v);
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            return Value(static_cast<int32_t>(v));
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            return Value(v);
        }
        else if constexpr (std::is_same_v<T, uint64_t>)
        {
            return Value(static_cast<int64_t>(v));
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            return Value(v);
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return Value(v);
        }
        else
        {
            static_assert(sizeof(T) == 0, "Unsupported type for Value::make");
        }
    }

    static Value make_funcref_null()
    {
        Value value;
        value.type = ValueType::FuncRef;
        value.storage.ref = 0;
        value.ref_is_null = true;
        return value;
    }

    static Value make_funcref(uint32_t index)
    {
        Value value;
        value.type = ValueType::FuncRef;
        value.storage.ref = index;
        value.ref_is_null = false;
        return value;
    }

    static Value make_externref_null()
    {
        Value value;
        value.type = ValueType::ExternRef;
        value.storage.ref = 0;
        value.ref_is_null = true;
        return value;
    }

    static Value make_externref(uint64_t handle)
    {
        Value value;
        value.type = ValueType::ExternRef;
        value.storage.ref = handle;
        value.ref_is_null = false;
        return value;
    }

    bool is_null_ref() const
    {
        return (type == ValueType::FuncRef || type == ValueType::ExternRef) && ref_is_null;
    }

    uint32_t funcref_index() const
    {
        if (type != ValueType::FuncRef || ref_is_null)
        {
            throw std::runtime_error("Value is not a non-null funcref");
        }
        return static_cast<uint32_t>(storage.ref);
    }

    template <typename T>
    T as() const
    {
        if constexpr (std::is_same_v<T, int32_t>)
        {
            if (type != ValueType::I32)
            {
                throw std::runtime_error("Value is not i32");
            }
            return storage.i32;
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            if (type != ValueType::I64)
            {
                throw std::runtime_error("Value is not i64");
            }
            return storage.i64;
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            if (type != ValueType::F32)
            {
                throw std::runtime_error("Value is not f32");
            }
            return storage.f32;
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            if (type != ValueType::F64)
            {
                throw std::runtime_error("Value is not f64");
            }
            return storage.f64;
        }
        else
        {
            static_assert(sizeof(T) == 0, "Unsupported type for Value::as");
        }
    }
};

struct ValueTypeMismatch final : std::runtime_error
{
    explicit ValueTypeMismatch(const std::string& message)
        : std::runtime_error(message)
    {
    }
};
} // namespace wasm
