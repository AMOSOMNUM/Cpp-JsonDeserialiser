#ifndef JSON_DESERIALISE_UTILITIES_H
#define JSON_DESERIALISE_UTILITIES_H

#include <type_traits>

inline namespace JsonDeserialise {

// unwrap a pack that has only one type
template <typename Front, typename...>
struct PackToType;

template <typename Front>
struct PackToType<Front> {
    using Type = Front;
};

template <typename Function>
struct ArgTypeDeduction {
    using Type = typename ArgTypeDeduction<decltype(&Function::operator())>::Type;
};

template <typename R, class C, typename Arg>
struct ArgTypeDeduction<R (C::*)(Arg) const> {
    using Type = Arg;
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

template <typename Current = void, typename... Types>
struct ConstexprStaticTuple {
    Current value;
    using Next = ConstexprStaticTuple<Types...>;
    Next next;
    using CurrentType = std::decay_t<Current>;

    constexpr ConstexprStaticTuple(Current source, Types... pack) : value{source}, next{pack...} {}

    template <int index>
    constexpr inline const typename GetType<index, ConstexprStaticTuple>::Type get() const {
        if constexpr (index == 0)
            return value;
        else
            return next.template get<index - 1>();
    }
};

template <typename Current>
struct ConstexprStaticTuple<Current> {
    Current value;
    using CurrentType = std::decay_t<Current>;

    constexpr ConstexprStaticTuple(Current source) : value{source} {}

    template <int index, typename = std::enable_if_t<index == 0>>
    constexpr inline const CurrentType get() const {
        return value;
    }
};

template <>
struct ConstexprStaticTuple<void> {
    template <int index, typename = std::enable_if_t<index == 0>>
    constexpr inline decltype(auto) get() const {
        return nullptr;
    }
};

template <typename... Types>
ConstexprStaticTuple(Types...)->ConstexprStaticTuple<Types...>;

} // namespace JsonDeserialise

#endif
