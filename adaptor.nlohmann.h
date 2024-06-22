#ifndef JSON_DESERIALISER_NLOHMANN_H
#define JSON_DESERIALISER_NLOHMANN_H

#include "type_deduction.nlohmann.h"

#include <cstring>
#include <fstream>
#include <string_view>
#include <type_traits>

#include <nlohmann/json.hpp>

namespace JsonDeserialise {
struct NlohmannJsonLib {

    // Essential alias

    template <typename Any>
    using Deserialisable = NlohmannJsonLibPrivate::Deserialisable<Any>;

    template <typename Any>
    using DeserialisableType = typename Deserialisable<Any>::Type;

    template <auto member_offset>
    using Customised = NlohmannJsonLibPrivate::Customised<member_offset>;

    template <typename T>
    struct StringConvertor {
        static constexpr bool value = false;
    };

    // Basic Types

    using String = std::string;
    using CString = String;
    using Json = nlohmann::json;
    using JsonArray = Json;
    using JsonObject = Json;
    using StringView = const std::string&;

    // Basic functions

    inline static bool is_null(const Json& json) {
        return json.is_null();
    }
    inline static bool is_array(const Json& json) {
        return json.is_array();
    }
    inline static bool is_object(const Json& json) {
        return json.is_object();
    }
    inline static bool is_string(const Json& json) {
        return json.is_string();
    }
    inline static bool is_number(const Json& json) {
        return json.is_number();
    }
    inline static bool is_bool(const Json& json) {
        return json.is_boolean();
    }

    // Get Methods may not be const ref, and return type could even be const ref,
    // which depends on the library's physical implementation.

    inline static const JsonArray& get_array(const Json& json) {
        return json;
    }
    inline static const JsonObject& get_object(const Json& json) {
        return json;
    }
    inline static String get_string(const Json& json) {
        return json.get<String>();
    }
    inline static double get_double(const Json& json) {
        return json.get<double>();
    }
    inline static int get_int(const Json& json) {
        return json.get<int>();
    }
    inline static unsigned get_uint(const Json& json) {
        return json.get<unsigned>();
    }
    inline static int get_int64(const Json& json) {
        return json.get<int64_t>();
    }
    inline static unsigned get_uint64(const Json& json) {
        return json.get<uint64_t>();
    }
    inline static bool get_bool(const Json& json) {
        return json.get<bool>();
    }

    inline static bool exists(const JsonObject& object, const String& key) {
        return object.contains(key);
    }

    inline static void insert(JsonObject& object, const String& key, Json&& json) {
        object.emplace(key, std::move(json));
    }

    inline static void append(JsonArray& array, Json&& json) {
        array.emplace_back(std::move(json));
    }

    inline static Json uint2json(unsigned integer) {
        return integer;
    }

    // String Contravariance

    inline static int str2int(const String& str) {
        return std::stoi(str);
    }
    inline static unsigned str2uint(const String& str) {
        return std::stoul(str);
    }
    inline static int str2int64(const String& str) {
        return std::stoll(str);
    }
    inline static unsigned str2uint64(const String& str) {
        return std::stoull(str);
    }
    inline static unsigned str2double(const String& str) {
        return std::stod(str);
    }

    inline static String tolower(const String& str) {
        String result;
        std::transform(str.cbegin(), str.cend(), std::back_inserter(result), ::tolower);
        return result;
    }
    inline static bool empty_str(const String& str) {
        return str.empty();
    }

    // Implementations

    static Json parse(const String& json) {
        return nlohmann::json::parse(json);
    }

    static Json parse_file(const String& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open())
            throw std::ios_base::failure("Failed to Open File!");
        std::string data{std::istreambuf_iterator(file), std::istreambuf_iterator<char>()};
        file.close();
        return parse(data);
    }

    static String print_json(Json&& data, bool compress) {
#ifdef _DEBUG
        if (!data.is_object() && !data.is_array() && !data.is_null())
            throw std::ios_base::failure("Invalid root JSON!");
#endif
        return data.dump(compress ? -1 : 4);
    }

    static void write_json(Json&& json, const String& filepath, bool compress) {
        auto data = print_json(std::move(json), compress);
        std::ofstream file(filepath);
        if (!file.is_open())
            throw std::ios_base::failure("Failed to Open File!");
        file << data;
        file.close();
    }

    template <size_t limit>
    static void char_array_write(char* des, String&& json) {
        std::string_view view{json};
        const auto length = json.length();
        auto size = length >= limit ? limit - 1 : length;
        std::strncpy(des, view.data(), size);
        des[size] = '\0';
    }
};

// String Convertors

template <>
struct NlohmannJsonLib::StringConvertor<char*> {
    static constexpr bool value = true;
    static inline char* convert(const String& str) {
        std::string_view view{str};
        const auto length = str.length();
        char* des = new char[length + 1];
        std::strncpy(des, view.data(), length);
        des[length] = '\0';
        return des;
    }
    static inline String deconvert(const char* src) {
        return src;
    }
};
template <>
struct NlohmannJsonLib::StringConvertor<const char*>
    : public NlohmannJsonLib::StringConvertor<char*> {};
template <>
struct NlohmannJsonLib::StringConvertor<std::string> {
    static constexpr bool value = true;
    static inline const std::string& convert(const std::string& str) {
        return str;
    }
    static inline const std::string& deconvert(const std::string& src) {
        return src;
    }
};

} // namespace JsonDeserialise

#endif // JSON_DESERIALISER_NLOHMANN_H
