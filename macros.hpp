#ifndef JSON_DESERIALISER_GLOBAL_MACROS
#define JSON_DESERIALISER_GLOBAL_MACROS

#include <boost/preprocessor.hpp>

#if __cplusplus >= 202002L
#define JSON_DESERIALISER_DEFAULT_BYTE_TYPE char8_t
#else
#define JSON_DESERIALISER_DEFAULT_BYTE_TYPE char
#endif
#define JSON_DESERIALISER_MACRO_WRAP_0(x) x
#define JSON_DESERIALISER_MACRO_WRAP_1(x) x

#define register_object_member_info_extension_(base, member_ptr, ...)                              \
    template <>                                                                                    \
    struct RegisteredExtensionBase<member_ptr> {                                                   \
        using Type = decltype(Impl::base(                                                          \
            __VA_ARGS__, *((typename MemberPtrToType<member_ptr>::Type*)nullptr)));                \
    };                                                                                             \
    template <>                                                                                    \
    struct RegisteredExtension<member_ptr> : public RegisteredExtensionBase<member_ptr>::Type {    \
        using Base = typename RegisteredExtensionBase<member_ptr>::Type;                           \
        template <typename... Args>                                                                \
        RegisteredExtension(Args&&... args) : Base(__VA_ARGS__, std::forward<Args>(args)...) {}    \
    };                                                                                             \
    template <>                                                                                    \
    struct Customised<member_ptr> {                                                                \
        using Type = RegisteredExtension<member_ptr>;                                              \
    };
#define register_object_member_info_style(member_ptr, style, ...)                                  \
    template <>                                                                                    \
    struct RegisteredStyleInfo<member_ptr>                                                         \
        : public Deserialisable<typename MemberPtrToType<member_ptr>::Type>::Style<(style)> {};    \
    template <int... pack>                                                                         \
    struct RegisteredStyle<member_ptr, ConstexprArrayPack<pack...>>                                \
        : public RegisteredStyleInfo<member_ptr>::Type {                                           \
        using Base = typename RegisteredStyleInfo<member_ptr>::Type;                               \
        static constexpr ConstexprTuple args{__VA_ARGS__};                                         \
        template <typename... Args>                                                                \
        RegisteredStyle(Args&&... args)                                                            \
            : Base(args.template get<pack>()..., std::forward<Args>(args)...) {}                   \
    };                                                                                             \
    template <>                                                                                    \
    struct Customised<member_ptr> {                                                                \
        using Type = RegisteredStyle<member_ptr>;                                                  \
    };
#define register_object_member_info_normal(...)
#define register_object_member_info_extension(member_ptr, functor1, functor2)                      \
    register_object_member_info_extension_(Extension, member_ptr, functor1, functor2)
#define register_object_member_info_deserialise_only_extension(member_ptr, functor)                \
    register_object_member_info_extension_(DeserialiseOnlyExtension, member_ptr, functor)
#define register_object_member_info_serialise_only_extension(member_ptr, functor)                  \
    register_object_member_info_extension_(SerialiseOnlyExtension, member_ptr, functor)

#define register_object_member_info_expand_body(x0, x1, x2, x3, x4, ...)                           \
    JSON_DESERIALISER_MACRO_WRAP_1(register_object_member_info_##x0(x2, ##__VA_ARGS__););          \
    template <>                                                                                    \
    struct RegisteredJsonKey<x2> {                                                                 \
        static constexpr JSON_DESERIALISER_DEFAULT_BYTE_TYPE value[] = x1;                         \
    };
#define object_member_info_expand_body(x0, x1, x2, x3, x4, ...)                                    \
    Impl::ObjectMember<RegisteredJsonKey<x2>, x2, x3, x4>
#define register_object_member_info_expand(z, n, tuple)                                            \
    JSON_DESERIALISER_MACRO_WRAP_0(                                                                \
        register_object_member_info_expand_body BOOST_PP_TUPLE_ELEM(n, tuple))
#define object_member_info_expand(z, n, tuple)                                                     \
    JSON_DESERIALISER_MACRO_WRAP_0(object_member_info_expand_body BOOST_PP_TUPLE_ELEM(n, tuple))
#define register_object_member_info(tuple)                                                         \
    BOOST_PP_REPEAT(BOOST_PP_TUPLE_SIZE(tuple), register_object_member_info_expand, tuple)
#define object_member_info(tuple)                                                                  \
    BOOST_PP_ENUM(BOOST_PP_TUPLE_SIZE(tuple), object_member_info_expand, tuple)

#define declare_object_body(object_type, tuple)                                                    \
    register_object_member_info(tuple);                                                            \
    template <>                                                                                    \
    struct DefinedObject<object_type> {                                                            \
        using Type = Impl::Object<object_type, object_member_info(tuple)>;                         \
    };                                                                                             \
    template <>                                                                                    \
    struct Deserialisable<object_type> {                                                           \
        using Type = typename DefinedObject<object_type>::Type;                                    \
    };
#define declare_object_with_base_class_body(object_type, base_type, tuple)                         \
    register_object_member_info(tuple);                                                            \
    template <>                                                                                    \
    struct DefinedObject<object_type> {                                                            \
        using Type = Impl::DerivedObject<base_type, object_type, object_member_info(tuple)>;       \
    };                                                                                             \
    template <>                                                                                    \
    struct Deserialisable<object_type> {                                                           \
        using Type = typename DefinedObject<object_type>::Type;                                    \
    };

#define declare_object_process_replace_body(tuple, object_type)                                    \
    BOOST_PP_TUPLE_REPLACE(tuple, 2, &object_type::BOOST_PP_TUPLE_ELEM(2, tuple))
#define declare_object_process_replace(z, n, tuple)                                                \
    declare_object_process_replace_body(BOOST_PP_TUPLE_ELEM(BOOST_PP_INC(n), tuple),               \
                                        BOOST_PP_TUPLE_ELEM(0, tuple))
#define declare_object_process(object_type, ...)                                                   \
    (BOOST_PP_ENUM(BOOST_PP_TUPLE_SIZE((__VA_ARGS__)), declare_object_process_replace,             \
                   (object_type, __VA_ARGS__)))

#define object_member_auto(x) object_member(#x, x)
#define optional_object_member_auto(x) optional_object_member(#x, x)
#define object_member(json_name, member_name) (normal, json_name, member_name, false, void)
#define optional_object_member(json_name, member_name) (normal, json_name, member_name, true, void)
#define object_member_with_named_extension(json_name, member_name, extension)                      \
    (normal, json_name, member_name, false, Extension::extension)
#define optionl_object_member_with_named_extension(json_name, member_name, extension)              \
    (normal, json_name, member_name, true, Extension::extension)
#define object_member_with_extension(json_name, member_name, deserialise_functor,                  \
                                     serialise_functor)                                            \
    (extension, json_name, member_name, false, void, deserialise_functor, serialise_functor)
#define optional_object_member_with_extension(json_name, member_name, deserialise_functor,         \
                                              serialise_functor)                                   \
    (extension, json_name, member_name, true, void, deserialise_functor, serialise_functor)
#define object_member_with_deserialise_only_extension(json_name, member_name, functor)             \
    (deserialise_only_extension, json_name, member_name, false, void, functor)
#define optional_object_member_with_deserialise_only_extension(json_name, member_name, functor)    \
    (deserialise_only_extension, json_name, member_name, true, void, functor)
#define object_member_with_serialise_only_extension(json_name, member_name, functor)               \
    (serialise_only_extension, json_name, member_name, false, void, functor)
#define optional_object_member_with_serialise_only_extension(json_name, member_name, functor)      \
    (serialise_only_extension, json_name, member_name, true, void, functor)
#define object_member_with_map_style(json_name, member_name, style_name, ...)                      \
    (style, json_name, member_name, false, void, JsonDeserialise::MapStyle::style_name, ##__VA_ARGS__)
#endif // JSON_DESERIALISER_GLOBAL_MACRO
