#ifndef JSON_DESERIALISER_QT_TYPES
#define JSON_DESERIALISER_QT_TYPES

// Additional Type Support

#include "basic_types.qt.hpp"

namespace JsonDeserialise::QtJsonLibPrivate {

    template <>
    struct Deserialisable<QtJsonLib::Json> {
        using Type = typename Implementation<QtJsonLib>::JSONWrap;
    };

    template <typename T>
    struct Deserialisable<QList<T>> : public ArrayTypeInfo<QList<T>, T> {};

    template <typename Key, typename Value>
    struct Deserialisable<QMap<Key, Value>> : public Impl::MapTypeInfo<QMap<Key, Value>, Key, Value> {
    };

    template <typename T>
    struct Deserialisable<QSet<T>> : public ArrayTypeInfo<QSet<T>, T> {};

} // namespace JsonDeserialise::QtJsonLibPrivate

#endif
