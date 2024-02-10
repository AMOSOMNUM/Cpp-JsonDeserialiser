#ifndef JSON_DESERIALISER_QT_TYPES
#define JSON_DESERIALISER_QT_TYPES

// Additional Type Support

#include "basic_types.qt.hpp"

inline namespace JsonDeserialise {

namespace QtAdaptation_ {

    template <>
    struct Deserialisable<QtAdaptation::Json> {
        using Type = typename Deserialiser<QtAdaptation>::JSONWrap;
    };

    template <typename T>
    struct Deserialisable<QList<T>> : public ArrayTypeInfo<QList<T>, T> {};

    template <typename Key, typename Value>
    struct Deserialisable<QMap<Key, Value>> : public Impl::MapTypeInfo<QMap<Key, Value>, Key, Value> {
    };

    template <typename T>
    struct Deserialisable<QSet<T>> : public ArrayTypeInfo<QSet<T>, T> {};

} // namespace QtAdaptation_

} // namespace JsonDeserialise

#endif
