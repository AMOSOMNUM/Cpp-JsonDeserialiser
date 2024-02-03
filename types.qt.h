#ifndef JSON_DESERIALISER_QT_TYPES
#define JSON_DESERIALISER_QT_TYPES

// Additional Type Support

#include "type_deduction.qt.hpp"

inline namespace JsonDeserialise {

namespace QtAdaptation_ {

    template <typename Container, typename T>
    struct ArrayTypeInfo {
        using ArrayType = Container;
        using TypeInArray = T;
    };

    template <typename Container, typename Key, typename Value>
    struct MapTypeInfo {
        using MapType = Container;
        using KeyType = Key;
        using ValueType = Value;
        using _OBJECT_MAP_ =
            typename Deserialiser<QtAdaptation>::ObjectMap<Container, KeyType, ValueType>;
    };

    template <>
    struct Deserialisable<QtAdaptation::Json> {
        using Type = typename Deserialiser<QtAdaptation>::JSONWrap;
    };

    template <typename T>
    struct Deserialisable<QList<T>> : public ArrayTypeInfo<QList<T>, T> {
        using Type = _DeserialisableType<QList<T>, true, -1, T>;
    };

    template <typename Key, typename Value>
    struct Deserialisable<QMap<Key, Value>> : public MapTypeInfo<QMap<Key, Value>, Key, Value> {
        using Type = Deserialiser<QtAdaptation>::Map<QMap<Key, Value>, Key, Value>;
    };

    template <typename T>
    struct Deserialisable<QSet<T>> : public ArrayTypeInfo<QSet<T>, T> {
        using Type = _DeserialisableType<QSet<T>, true, -1, T>;
    };

} // namespace QtAdaptation_

} // namespace JsonDeserialise

#endif
