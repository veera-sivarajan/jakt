#pragma once

#include <AK/AllOf.h>
#include <AK/Assertions.h>
#include <AK/Atomic.h>
#include <AK/BitCast.h>
#include <AK/CharacterTypes.h>
#include <AK/Checked.h>
#include <AK/Concepts.h>
#include <AK/Debug.h>
#include <AK/Error.h>
#include <AK/Find.h>
#include <AK/Format.h>
#include <AK/Forward.h>
#include <AK/Function.h>
#include <AK/GenericLexer.h>
#include <AK/HashFunctions.h>
#include <AK/HashTable.h>
#include <AK/Iterator.h>
#include <AK/LinearArray.h>
#include <AK/Memory.h>
#include <AK/Noncopyable.h>
#include <AK/NonnullRefPtr.h>
#include <AK/NumericLimits.h>
#include <AK/Optional.h>
#include <AK/Platform.h>
#include <AK/RefCounted.h>
#include <AK/RefPtr.h>
#include <AK/ScopeGuard.h>
#include <AK/Span.h>
#include <AK/StdLibExtraDetails.h>
#include <AK/StdLibExtras.h>
#include <AK/String.h>
#include <AK/StringBuilder.h>
#include <AK/StringHash.h>
#include <AK/StringUtils.h>
#include <AK/StringView.h>
#include <AK/Traits.h>
#include <AK/Try.h>
#include <AK/Tuple.h>
#include <AK/TypeCasts.h>
#include <AK/TypeList.h>
#include <AK/TypedTransfer.h>
#include <AK/Types.h>
#include <AK/UnicodeUtils.h>
#include <AK/Variant.h>
#include <AK/WeakPtr.h>
#include <AK/Weakable.h>
#include <AK/kmalloc.h>
#include <AK/kstdio.h>

#include <AK/Format.cpp>
#include <AK/GenericLexer.cpp>
#include <AK/String.cpp>
#include <AK/StringBuilder.cpp>
#include <AK/StringUtils.cpp>
#include <AK/StringView.cpp>
#include <AK/kmalloc.cpp>

namespace JaktInternal {
template<typename T>
class Set;
}

#include <Builtins/Array.h>
#include <Builtins/Dictionary.h>
#include <Builtins/Set.h>

#include <IO/File.h>

#include <IO/File.cpp>

using f32 = float;
using f64 = double;

template<typename T>
struct Range {
    using IndexType = T;
    T start {};
    T end {};
    T current {};

    Range(T start, T end)
        : start(start)
        , end(end)
        , current(start)
    {
    }

    Optional<T> next()
    {
        if (current == end)
            return {};
        return current++;
    }
};

namespace JaktInternal {

#ifdef JAKT_CONTINUE_ON_PANIC
constexpr auto continue_on_panic = true;
#else
constexpr auto continue_on_panic = false;
#endif

using OptionalNone = AK::NullOptional;

inline void panic(StringView message)
{
    warnln("Panic: {}", message);
    if (continue_on_panic)
        return;
    VERIFY_NOT_REACHED();
}

template<typename T>
inline constexpr T checked_add(T value, T other)
{
    Checked<T> checked = value;
    checked += other;
    if (checked.has_overflow())
        panic(MUST(String::formatted("Overflow in checked addition '{} + {}'", value, other)));
    return checked.value_unchecked();
}

template<typename T>
inline constexpr T checked_sub(T value, T other)
{
    Checked<T> checked = value;
    checked -= other;
    if (checked.has_overflow())
        panic(MUST(String::formatted("Overflow in checked subtraction '{} - {}'", value, other)));
    return checked.value_unchecked();
}

template<typename T>
inline constexpr T checked_mul(T value, T other)
{
    Checked<T> checked = value;
    checked *= other;
    if (checked.has_overflow())
        panic(MUST(String::formatted("Overflow in checked multiplication '{} * {}'", value, other)));
    return checked.value_unchecked();
}

template<typename T>
inline constexpr T checked_div(T value, T other)
{
    Checked<T> checked = value;
    checked /= other;
    if (checked.has_overflow()) {
        if (other == 0)
            panic(MUST(String::formatted("Division by zero in checked division '{} / {}'", value, other)));
        else
            panic(MUST(String::formatted("Overflow in checked division '{} / {}'", value, other)));
    }
    return checked.value_unchecked();
}

template<typename T>
inline constexpr T checked_mod(T value, T other)
{
    Checked<T> checked = value;
    checked %= other;
    if (checked.has_overflow()) {
        if (other == 0)
            panic(MUST(String::formatted("Division by zero in checked modulo '{} % {}'", value, other)));
        else
            panic(MUST(String::formatted("Overflow in checked modulo '{} % {}'", value, other)));
    }
    return checked.value_unchecked();
}

template<typename T>
inline constexpr T arithmetic_shift_right(T value, size_t steps)
{
    if constexpr (IsSigned<T>) {
        if constexpr (sizeof(T) == 1) {
            auto sign = (value & 0x80);
            // 8-bit variant
            return ((value >> steps) | sign);
        } else if constexpr (sizeof(T) == 2) {
            auto sign = (value & 0x8000);
            // 16-bit variant
            return ((value >> steps) | sign);
        } else if constexpr (sizeof(T) == 4) {
            auto sign = (value & 0x80000000);
            // 32-bit variant
            return ((value >> steps) | sign);
        } else if constexpr (sizeof(T) == 8) {
            auto sign = (value & 0x8000000000000000);
            // 64-bit variant
            return ((value >> steps) | sign);
        }
    } else {
        return (value >> steps);
    }
}

template<typename Value>
struct ExplicitValue {
    ExplicitValue(Value&& v)
        : value(move(v))
    {
    }
    ExplicitValue(Value const& v)
        : value(v)
    {
    }

    Value value;
};

template<>
struct ExplicitValue<void> {
    ExplicitValue()
    {
    }
};

struct LoopBreak { };
struct LoopContinue { };

template<typename Value, typename Return>
struct ExplicitValueOrControlFlow {
    template<typename U>
    ExplicitValueOrControlFlow(ExplicitValue<U>&& v)
        : value(ExplicitValue<Value> { move(v.value) })
    {
    }

    ExplicitValueOrControlFlow(ExplicitValue<void>&&)
        : value(ExplicitValue<void> {})
    {
    }

    template<typename U>
    ExplicitValueOrControlFlow(U&& v) requires(!IsVoid<Return>)
        : value(Return { forward<U>(v) })
    {
    }

    ExplicitValueOrControlFlow(void) requires(IsVoid<Return>)
        : value(Empty {})
    {
    }

    ExplicitValueOrControlFlow(LoopContinue)
        : value(LoopContinue {})
    {
    }

    ExplicitValueOrControlFlow(LoopBreak)
        : value(LoopBreak {})
    {
    }

    bool is_return() const
    {
        return value.template has<Conditional<IsVoid<Return>, Empty, Return>>();
    }

    bool is_loop_break() const
    {
        return value.template has<LoopBreak>();
    }

    bool is_loop_continue() const
    {
        return value.template has<LoopContinue>();
    }

    Return release_return()
    {
        if constexpr (IsVoid<Return>)
            return;
        else
            return move(value).template get<Return>();
    }
    Value release_value()

    {
        if constexpr (IsVoid<Value>)
            return;
        else
            return move(value).template get<ExplicitValue<Value>>().value;
    }

    Variant<Conditional<IsVoid<Return>, Empty, Return>, ExplicitValue<Value>, LoopContinue, LoopBreak> value;
};

#define JAKT_RESOLVE_EXPLICIT_VALUE_OR_CONTROL_FLOW_RETURN_ONLY(x) ({ \
    auto&& _jakt_value = x;                                           \
    if (_jakt_value.is_return())                                      \
        return _jakt_value.release_return();                          \
    _jakt_value.release_value();                                      \
})

// XXX: `break` might not work if codegen is inside a switch block.
#define JAKT_RESOLVE_EXPLICIT_VALUE_OR_CONTROL_FLOW_AT_LOOP(x) ({ \
    auto&& _jakt_value = x;                                       \
    if (_jakt_value.is_return())                                  \
        return _jakt_value.release_return();                      \
    else if (_jakt_value.is_loop_break())                         \
        break;                                                    \
    else if (_jakt_value.is_loop_continue())                      \
        continue;                                                 \
    _jakt_value.release_value();                                  \
})

#define JAKT_RESOLVE_EXPLICIT_VALUE_OR_CONTROL_FLOW_AT_LOOP_NESTED_MATCH(x) ({ \
    auto&& _jakt_value = x;                                                    \
    if (_jakt_value.is_return())                                               \
        return _jakt_value.release_return();                                   \
    else if (_jakt_value.is_loop_break())                                      \
        return JaktInternal::LoopBreak {};                                     \
    else if (_jakt_value.is_loop_continue())                                   \
        return JaktInternal::LoopContinue {};                                  \
    _jakt_value.release_value();                                               \
})

template<typename OutputType, typename InputType>
ALWAYS_INLINE Optional<OutputType> fallible_integer_cast(InputType input)
{
    if constexpr (IsEnum<InputType>) {
        return fallible_integer_cast<OutputType>(to_underlying(input));
    } else {
        static_assert(IsIntegral<InputType>);
        if (!AK::is_within_range<OutputType>(input))
            return {};
        return static_cast<OutputType>(input);
    }
}

template<typename... Ts>
void compiletime_fail(Ts...) { }

template<typename OutputType, typename InputType>
ALWAYS_INLINE constexpr OutputType infallible_integer_cast(InputType input)
{
    if constexpr (IsEnum<InputType>) {
        return infallible_integer_cast<OutputType>(to_underlying(input));
    } else {
        static_assert(IsIntegral<InputType>);
        if (is_constant_evaluated()) {
            if (!AK::is_within_range<OutputType>(input))
                compiletime_fail("Integer cast out of range");
        } else {
            VERIFY(AK::is_within_range<OutputType>(input));
        }
        return static_cast<OutputType>(input);
    }
}

template<typename OutputType, typename InputType>
ALWAYS_INLINE constexpr OutputType as_saturated(InputType input)
{
    if constexpr (IsEnum<InputType>) {
        return as_saturated<OutputType>(to_underlying(input));
    } else {
        static_assert(IsIntegral<InputType>);
        if (!AK::is_within_range<OutputType>(input)) {
            if constexpr (IsSigned<InputType>) {
                if (input < 0)
                    return NumericLimits<OutputType>::min();
            }
            return NumericLimits<OutputType>::max();
        }
        return static_cast<OutputType>(input);
    }
}

template<typename OutputType, typename InputType>
ALWAYS_INLINE constexpr OutputType as_truncated(InputType input)
{
    if constexpr (IsEnum<InputType>) {
        return as_truncated<OutputType>(to_underlying(input));
    } else {
        static_assert(IsIntegral<InputType>);
        return static_cast<OutputType>(input);
    }
}

template<typename T>
struct _RemoveRefPtr {
    using Type = T;
};

template<typename T>
struct _RemoveRefPtr<NonnullRefPtr<T>> {
    using Type = T;
};

template<typename T>
using RemoveRefPtr = typename _RemoveRefPtr<RemoveCVReference<T>>::Type;

template<typename T>
ALWAYS_INLINE decltype(auto) deref_if_ref_pointer(T&& value)
{
    if constexpr (IsSpecializationOf<RemoveCVReference<T>, NonnullRefPtr>)
        return static_cast<CopyConst<RemoveReference<T>, RemoveRefPtr<T>>&>(*value);
    else
        return static_cast<Conditional<IsRvalueReference<T>, RemoveReference<T>, T>>(value);
}

}

using JaktInternal::as_saturated;
using JaktInternal::as_truncated;
using JaktInternal::fallible_integer_cast;
using JaktInternal::infallible_integer_cast;

// We place main in a separate namespace to ensure it has access to the same identifiers as other functions
namespace JaktMain {
ErrorOr<int> main(Array<String>);
}

int main(int argc, char** argv)
{
    Array<String> args;
    for (int i = 0; i < argc; ++i) {
        MUST(args.push(MUST(String::copy(StringView(argv[i])))));
    }
    auto result = JaktMain::main(move(args));
    if (result.is_error()) {
        warnln("Runtime error: {}", result.error());
        return 1;
    }
    return result.value();
}
