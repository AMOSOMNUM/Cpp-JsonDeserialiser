#ifndef JSON_DESERIALISE_UTILITIES_H
#define JSON_DESERIALISE_UTILITIES_H

#include <cstdint>
#include <type_traits>

namespace JsonDeserialise {
enum class Trait : uint8_t {
    NUL = 0,
    OBJECT = 1,
    ARRAY = 2,
    FIELD = 4,
    OPTIONAL = 8,
};

enum class ArrayInsertWay : uint8_t {
    Unknown = 0,
    Emplace_Back = 1,
    Push_Back = 2,
    Append = 4,
    Insert = 8
};

template <typename T, typename Element>
struct GetArrayInsertWay {
    static constexpr ArrayInsertWay calculate() {
        ArrayInsertWay result = ArrayInsertWay::Unknown;

        if (is_emplaceback<T, Element>(nullptr))
            result = ArrayInsertWay(uint8_t(result) | uint8_t(ArrayInsertWay::Emplace_Back));
        if (is_pushback<T, Element>(nullptr))
            result = ArrayInsertWay(uint8_t(result) | uint8_t(ArrayInsertWay::Push_Back));
        if (is_append<T, Element>(nullptr))
            result = ArrayInsertWay(uint8_t(result) | uint8_t(ArrayInsertWay::Append));
        if (is_insert<T, Element>(nullptr))
            result = ArrayInsertWay(uint8_t(result) | uint8_t(ArrayInsertWay::Insert));

        return result;
    }
    template <typename U, typename V, typename = decltype(std::declval<U>().emplace_back())>
    static constexpr bool is_emplaceback(int* p) {
        return true;
    }
    template <typename...>
    static constexpr bool is_emplaceback(...) {
        return false;
    }
    template <typename U, typename V,
              typename = decltype(std::declval<U>().push_back(std::declval<V>()))>
    static constexpr bool is_pushback(int* p) {
        return true;
    }
    template <typename...>
    static constexpr bool is_pushback(...) {
        return false;
    }
    template <typename U, typename V,
              typename = decltype(std::declval<U>().append(std::declval<V>()))>
    static constexpr bool is_append(int* p) {
        return true;
    }
    template <typename...>
    static constexpr bool is_append(...) {
        return false;
    }
    template <typename U, typename V,
              typename = decltype(std::declval<U>().insert(std::declval<V>()))>
    static constexpr bool is_insert(int* p) {
        return true;
    }
    template <typename...>
    static constexpr bool is_insert(...) {
        return false;
    }
    static constexpr ArrayInsertWay value = calculate();
    static constexpr bool insert_only = value == ArrayInsertWay::Insert;

    inline static Element& push_back(T& container) {
        if constexpr (uint8_t(value) & uint8_t(ArrayInsertWay::Emplace_Back))
            return container.emplace_back();
        else {
            if constexpr (uint8_t(value) & uint8_t(ArrayInsertWay::Push_Back))
                container.push_back(Element());
            else if constexpr (uint8_t(value) & uint8_t(ArrayInsertWay::Append))
                container.append(Element());
            return container.back();
        }
    }
};
} // namespace JsonDeserialise

// unwrap a pack that has only one type
template <typename Front, typename...>
struct PackToType;

template <typename Front>
struct PackToType<Front> {
    using Type = Front;
};

template <int... pack>
struct ConstexprArrayPack {
    constexpr static int length = sizeof...(pack);
    constexpr static int value[sizeof...(pack)] = {pack...};
};

template <>
struct ConstexprArrayPack<> {
    constexpr static int length = 0;
};

template <int N, int current = 0, int... pack>
struct ConstexprIncArray : public ConstexprIncArray<N, current + 1, pack..., current> {};

template <int N, int... pack>
struct ConstexprIncArray<N, N, pack...> : public ConstexprArrayPack<pack...> {
    using Type = ConstexprArrayPack<pack...>;
};

template <int N, int Constant, typename Result = ConstexprArrayPack<Constant>, int num = 1,
          bool = N == num, typename = std::enable_if_t<(N >= 0)>>
struct ConstexprConstantArray;

template <int N, int Constant, int... Result, int num>
struct ConstexprConstantArray<N, Constant, ConstexprArrayPack<Result...>, num, false> {
    using Next =
        ConstexprConstantArray<N, Constant, ConstexprArrayPack<Constant, Result...>, num + 1>;
    using Type = typename Next::Type;
};

template <int N, int Constant, int... Result>
struct ConstexprConstantArray<N, Constant, ConstexprArrayPack<Result...>, N, true> {
    using Type = ConstexprArrayPack<Result...>;
};

template <int Constant>
struct ConstexprConstantArray<0, Constant, ConstexprArrayPack<Constant>, 1, false> {
    using Type = ConstexprArrayPack<>;
};

template <typename pack, int N, int current = 0, typename Result = ConstexprArrayPack<>,
          bool = N == current, typename = std::enable_if_t<(N <= pack::length)>>
struct ConstexprArrayPackFront;

template <typename pack, int N, int current, int... Last>
struct ConstexprArrayPackFront<pack, N, current, ConstexprArrayPack<Last...>, false> {
    using Current = ConstexprArrayPack<Last..., pack::value[current]>;
    using Type = typename ConstexprArrayPackFront<pack, N, current + 1, Current>::Type;
};

template <typename pack>
struct ConstexprArrayPackFront<pack, 0> {
    using Type = ConstexprArrayPack<>;
};

template <typename pack, int N, int... Last>
struct ConstexprArrayPackFront<pack, N, N, ConstexprArrayPack<Last...>, true> {
    using Type = ConstexprArrayPack<Last...>;
};

template <typename pack, int N, int current = 0, typename Result = ConstexprArrayPack<>,
          bool = N == current, typename = std::enable_if_t<(N <= pack::length)>>
struct ConstexprArrayPackBack;

template <typename pack>
struct ConstexprArrayPackBack<pack, 0> {
    using Type = ConstexprArrayPack<>;
};

template <typename pack, int N, int current, int... Last>
struct ConstexprArrayPackBack<pack, N, current, ConstexprArrayPack<Last...>, false> {
    using Current = ConstexprArrayPack<pack::value[pack::length - 1 - current], Last...>;
    using Type = typename ConstexprArrayPackFront<pack, N, current + 1, Current>::Type;
};

template <typename pack, int N, int... Last>
struct ConstexprArrayPackBack<pack, N, N, ConstexprArrayPack<Last...>, true> {
    using Type = ConstexprArrayPack<Last...>;
};

template <typename front, int middle, typename back>
struct ConstexprArrayPackMerge;

template <int... front, int middle, int... back>
struct ConstexprArrayPackMerge<ConstexprArrayPack<front...>, middle, ConstexprArrayPack<back...>> {
    using Type = ConstexprArrayPack<front..., middle, back...>;
};

template <int loc, int Constant, typename pack,
          typename = std::enable_if_t<(loc >= 0 && loc < pack::length)>>
struct ConstexprArrayPackAlter {
    using Type = typename ConstexprArrayPackMerge<
        typename ConstexprArrayPackFront<pack, loc>::Type, Constant,
        typename ConstexprArrayPackBack<pack, pack::length - 1 - loc>::Type>::Type;
};

template <int index, typename Tuple>
struct GetType : GetType<index - 1, typename Tuple::Next> {};

template <typename Tuple>
struct GetType<0, Tuple> {
    using Type = typename Tuple::CurrentType;
};

template <typename Current = void, typename... Types>
struct StaticTuple {
    Current value;
    using Next = StaticTuple<Types...>;
    Next next;
    using CurrentType = Current;

    StaticTuple(Current&& source, Types&&... pack)
        : value(std::move(source)), next(std::forward<Types>(pack)...) {}

    template <int index>
    inline typename GetType<index, StaticTuple>::Type& get() {
        if constexpr (index == 0)
            return value;
        else
            return next.template get<index - 1>();
    }

    template <int index>
    inline const typename GetType<index, StaticTuple>::Type& get() const {
        if constexpr (index == 0)
            return value;
        else
            return next.template get<index - 1>();
    }
};

template <typename Current>
struct StaticTuple<Current> {
    Current value;
    using CurrentType = Current;

    StaticTuple(Current&& source) : value(std::move(source)) {}

    template <int index, typename = std::enable_if_t<index == 0>>
    inline Current& get() {
        return value;
    }

    template <int index, typename = std::enable_if_t<index == 0>>
    inline const typename GetType<index, StaticTuple>::Type& get() const {
        return value;
    }
};

template <>
struct StaticTuple<void> {};

template <typename Tuple, typename T, int index = 0,
          bool = std::is_same_v<T, typename GetType<index, Tuple>::Type>,
          typename = std::enable_if_t<(index < Tuple::length)>>
struct FindType : public FindType<Tuple, T, index + 1> {};

template <typename Tuple, typename T, int Index>
struct FindType<Tuple, T, Index, true> {
    static constexpr int index = Index;
};

template <typename Current = void, typename... Types>
struct TypeTuple {
    using Next = TypeTuple<Types...>;
    using CurrentType = Current;
    static constexpr int length = sizeof...(Types) + 1;
};

template <typename Current>
struct TypeTuple<Current> {
    using CurrentType = Current;
    static constexpr int length = 1;
};

template <>
struct TypeTuple<void> {
    static constexpr int length = 0;
};

#endif
