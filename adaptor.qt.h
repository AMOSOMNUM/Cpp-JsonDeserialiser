#ifndef JSON_DESERIALISER_QT_H
#define JSON_DESERIALISER_QT_H

#include "type_deduction.qt.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <cstring>

namespace JsonDeserialise {
struct QtJsonLib {

    // Essential alias

    template <typename Any>
    using Deserialisable = QtJsonLibPrivate::Deserialisable<Any>;

    template <typename Any>
    using DeserialisableType = typename Deserialisable<Any>::Type;

    template <auto member_offset>
    using Customised = QtJsonLibPrivate::Customised<member_offset>;

    // Basic Types

    using String = QString;
    using CString = QByteArray;
    using Json = QJsonValue;
    using JsonArray = QJsonArray;
    using JsonObject = QJsonObject;
    using StringView = QByteArrayView;

    // Basic functions

    inline static bool is_null(const Json& json) {
        return json.isNull();
    }
    inline static bool is_array(const Json& json) {
        return json.isArray();
    }
    inline static bool is_object(const Json& json) {
        return json.isObject();
    }
    inline static bool is_string(const Json& json) {
        return json.isString();
    }
    inline static bool is_number(const Json& json) {
        return json.isDouble();
    }
    inline static bool is_bool(const Json& json) {
        return json.isBool();
    }

    // Get Methods may not be const ref, and return type could even be const ref,
    // which depends on the library's physical implementation.

    inline static JsonArray get_array(const Json& json) {
        return json.toArray();
    }
    inline static JsonObject get_object(const Json& json) {
        return json.toObject();
    }
    inline static String get_string(const Json& json) {
        return json.toString();
    }
    inline static double get_double(const Json& json) {
        return json.toDouble();
    }
    inline static int get_int(const Json& json) {
        return json.toInt();
    }
    inline static unsigned get_uint(const Json& json) {
        return json.toInteger();
    }
    inline static bool get_bool(const Json& json) {
        return json.toBool();
    }

    inline static bool exists(const JsonObject& object, const String& key) {
        return object.contains(key);
    }

    inline static void insert(JsonObject& object, const String& key, Json&& json) {
        object.insert(key, std::move(json));
    }

    inline static void append(JsonArray& array, Json&& json) {
        array.append(std::move(json));
    }

    inline static Json uint2json(unsigned integer) {
        return (qint64)integer;
    }

    // String Contravariance

    inline static int str2int(const String& str) {
        bool ok;
        int result = str.toInt(&ok);
        if (!ok)
            throw std::ios_base::failure("Type Unmatch!");
        return result;
    }
    inline static unsigned str2uint(const String& str) {
        bool ok;
        unsigned result = str.toUInt(&ok);
        if (!ok)
            throw std::ios_base::failure("Type Unmatch!");
        return result;
    }
    inline static unsigned str2double(const String& str) {
        bool ok;
        double result = str.toDouble(&ok);
        if (!ok)
            throw std::ios_base::failure("Type Unmatch!");
        return result;
    }
    
    inline static String tolower(const String& str) {
        return str.toLower();
    }
    inline static bool empty_str(const String& str) {
        return str.isEmpty();
    }

    // String Convertors

    template <typename T>
    struct StringConvertor {
        static constexpr bool value = false;
    };
    template <>
    struct StringConvertor<char*> {
        static constexpr bool value = true;
        static inline char* convert(const String& str) {
            const auto& src = str.toUtf8();
            const auto length = src.length();
            char* des = new char[length + 1];
            std::strncpy(des, src.constData(), length);
            des[length] = '\0';
            return des;
        }
        static inline String deconvert(const char* src) {
            return src;
        }
    };
    template <>
    struct StringConvertor<const char*> : public StringConvertor<char*> {};
    template <>
    struct StringConvertor<QString> {
        static constexpr bool value = true;
        static inline const QString& convert(const String& str) {
            return str;
        }
        static inline const String& deconvert(const QString& src) {
            return src;
        }
    };
    template <>
    struct StringConvertor<std::string> {
        static constexpr bool value = true;
        static inline std::string convert(const String& str) {
            return str.toStdString();
        }
        static inline String deconvert(const std::string& src) {
            return QString::fromStdString(src);
        }
    };
    template <>
    struct StringConvertor<QByteArray> {
        static constexpr bool value = true;
        static inline QByteArray convert(const String& str) {
            return str.toUtf8();
        }
        static inline String deconvert(const QByteArray& src) {
            return QString::fromUtf8(src);
        }
    };

    // Implementations

    static Json parse(const CString& json) {
        QJsonParseError parser;
        QJsonDocument result = QJsonDocument::fromJson(json, &parser);
        if (parser.error != QJsonParseError::NoError)
            throw std::ios_base::failure("JSON Parsing Failed!");
        if (result.isObject())
            return result.object();
        if (result.isArray())
            return result.array();
        return Json();
    }

    static Json parse_file(const String& filepath) {
        QFile file(filepath);
        if (!file.open(QFile::ReadOnly))
            throw std::ios_base::failure("Failed to Open File!");
        auto data = file.readAll();
        file.close();
        return parse(data);
    }

    static CString print_json(Json&& data, bool compress) {
#ifdef _DEBUG
        if (!data.isObject() && !data.isArray() && !data.isNull())
            throw std::ios_base::failure("Invalid root JSON!");
#endif
        QJsonDocument json;
        if (data.isObject())
            json.setObject(data.toObject());
        else
            json.setArray(data.toArray());
        return json.toJson(QJsonDocument::JsonFormat(compress));
    }

    static void write_json(Json&& json, const String& filepath, bool compress) {
        auto data = print_json(std::move(json), compress);
        QFile file(filepath);
        if (!file.open(QFile::WriteOnly))
            throw std::ios_base::failure("Failed to Open File!");
        file.write(data);
        file.close();
    }

    template <size_t limit>
    static void char_array_write(char* des, String&& json) {
        auto str = json.toUtf8();
        const char* c_str = str.constData();
        const auto length = str.length();
        auto size = length >= limit ? limit - 1 : length;
        std::strncpy(des, c_str, size);
        des[size] = '\0';
    }
};

} // namespace JsonDeserialise

#endif // JSON_DESERIALISER_QT_H
