#ifndef JSON_DESERIALISE_UTILITIES_H
#define JSON_DESERIALISE_UTILITIES_H

#include <type_traits>

namespace JsonDeserialise {

// unwrap a pack that has only one type
template <typename TypeTupleFront, typename...>
struct PackToType;

template <typename TypeTupleFront>
struct PackToType<TypeTupleFront> {
    using Type = TypeTupleFront;
};

template <auto member_ptr, typename = decltype(member_ptr)>
struct MemberPtrToType;

template <typename T, typename ObjectType, auto member_ptr>
struct MemberPtrToType<member_ptr, T ObjectType::*> {
    using Type = T;
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
struct ConstexprIncArray : public ConstexprIncArray<N, (current + 1), pack..., current> {};

template <int N, int... pack>
struct ConstexprIncArray<N, N, pack...> : public ConstexprArrayPack<pack...> {
    using Type = ConstexprArrayPack<pack...>;
};

template <int index, typename Tuple>
struct GetType : GetType<(index - 1), typename Tuple::Next> {};

template <typename Tuple>
struct GetType<0, Tuple> {
    using Type = typename Tuple::CurrentType;
};

template <typename Tuple, typename T, int index = 0,
          bool = std::is_same_v<T, typename GetType<index, Tuple>::Type>,
          typename = std::enable_if_t<(index < Tuple::length)>>
struct FindType : public FindType<Tuple, T, (index + 1)> {};

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

template <typename Current, template <typename> typename F>
struct TypeTupleMap;

template <typename... Args, template <typename> typename F>
struct TypeTupleMap<TypeTuple<Args...>, F> {
    using Type = TypeTuple<typename F<Args>::Type...>;
};

template <int n, typename Source,
          typename Result = std::enable_if_t<(n >= 0 && n < Source::length), TypeTuple<void>>>
struct TypeTupleFront;

template <int n, typename T, typename... Args, typename... Last>
struct TypeTupleFront<n, TypeTuple<T, Args...>, TypeTuple<Last...>> {
    using Right = TypeTuple<Args...>;
    using Left = TypeTuple<Last..., T>;
    using Type = typename TypeTupleFront<(n - 1), Right, Left>::Type;
};

template <int n, typename T, typename... Args>
struct TypeTupleFront<n, TypeTuple<T, Args...>, TypeTuple<void>> {
    using Right = TypeTuple<Args...>;
    using Left = TypeTuple<T>;
    using Type = typename TypeTupleFront<(n - 1), Right, Left>::Type;
};

template <typename T, typename... Args, typename... Last>
struct TypeTupleFront<0, TypeTuple<T, Args...>, TypeTuple<Last...>> {
    using Right = TypeTuple<Args...>;
    using Left = TypeTuple<Last..., T>;
    using Type = TypeTupleFront;
};

template <typename T, typename... Args>
struct TypeTupleFront<0, TypeTuple<T, Args...>, TypeTuple<void>> {
    using Right = TypeTuple<Args...>;
    using Left = TypeTuple<T>;
    using Type = TypeTupleFront;
};

template <typename Left, typename Right>
struct TypeTupleMerge;

template <typename... Left, typename... Right>
struct TypeTupleMerge<TypeTuple<Left...>, TypeTuple<Right...>> {
    using Type = TypeTuple<Left..., Right...>;
};

template <typename... Left>
struct TypeTupleMerge<TypeTuple<Left...>, TypeTuple<void>> {
    using Type = TypeTuple<Left...>;
};

template <typename... Right>
struct TypeTupleMerge<TypeTuple<void>, TypeTuple<Right...>> {
    using Type = TypeTuple<Right...>;
};

template <>
struct TypeTupleMerge<TypeTuple<void>, TypeTuple<void>> {
    using Type = TypeTuple<void>;
};

template <int n, typename Source, typename Value,
          typename = std::enable_if_t<(n < Source::length && n >= 0)>>
struct TypeTupleAlter;

template <int n, typename Value, typename... Args>
struct TypeTupleAlter<n, TypeTuple<Args...>, Value> {
    using Loc = typename TypeTupleFront<(n - 1), TypeTuple<Args...>>::Type;
    using Type = typename TypeTupleMerge<typename Loc::Left,
                                typename TypeTupleAlter<0, typename Loc::Right, Value>::Type>::Type;
};

template <typename Value, typename T, typename... Args>
struct TypeTupleAlter<0, TypeTuple<T, Args...>, Value> {
    using Type = TypeTuple<Value, Args...>;
};

template <typename Current = void, typename... Types>
struct ConstexprTuple {
    Current value;
    using Next = ConstexprTuple<Types...>;
    Next next;
    using CurrentType = std::decay_t<Current>;

    constexpr ConstexprTuple(Current source, Types... pack) : value{source}, next{pack...} {}

    template <int index>
    constexpr inline const typename GetType<index, ConstexprTuple>::Type get() const {
        if constexpr (index == 0)
            return value;
        else
            return next.template get<index - 1>();
    }
};

template <typename Current>
struct ConstexprTuple<Current> {
    Current value;
    using CurrentType = std::decay_t<Current>;

    constexpr ConstexprTuple(Current source) : value{source} {}

    template <int index, typename = std::enable_if_t<index == 0>>
    constexpr inline const CurrentType get() const {
        return value;
    }
};

template <>
struct ConstexprTuple<void> {
    template <int index, typename = std::enable_if_t<index == 0>>
    constexpr inline decltype(auto) get() const {
        return nullptr;
    }
};

template <typename... Types>
ConstexprTuple(Types...) -> ConstexprTuple<Types...>;

} // namespace JsonDeserialise

#endif
