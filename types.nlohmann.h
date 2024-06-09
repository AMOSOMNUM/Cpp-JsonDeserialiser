#ifndef JSON_DESERIALISER_NLOHMANN_TYPES
#define JSON_DESERIALISER_NLOHMANN_TYPES

// Additional Type Support

#include "basic_types.nlohmann.hpp"

namespace JsonDeserialise::NlohmannJsonLibPrivate {

    template <>
    struct Deserialisable<NlohmannJsonLib::Json> {
        using Type = typename Implementation<NlohmannJsonLib>::JSONWrap;
    };

} // namespace JsonDeserialise::NlohmannJsonLibPrivate

#endif
