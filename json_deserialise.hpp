#ifndef JSON_DESERIALISER_H
#define JSON_DESERIALISER_H

#include <cstddef>
#include <cstdlib>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <type_traits>
#include <variant>
#include <vector>

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

namespace JsonDeserialise {
    template <typename T>
    struct StringConvertor {
        static constexpr bool value = false;
    };

    template <>
    struct StringConvertor<char*> {
        static constexpr bool value = true;
        static inline char* convert(const QString& str) {
            const auto& src = str.toUtf8();
            char* des = new char[src.length() + 1];
            strncpy(des, src.constData(), src.length());
            des[src.length()] = '\0';
            return des;
        }
        static inline QString deconvert(char* const& src) {
            return src;
        }
    };

    template <>
    struct StringConvertor<const char*> {
        static constexpr bool value = true;
        static inline const char* convert(const QString& str) {
            return StringConvertor<char*>::convert(str);
        }
        static inline QString deconvert(const char* const& src) {
            return src;
        }
    };

    template <>
    struct StringConvertor<QString> {
        static constexpr bool value = true;
        static inline const QString& convert(const QString& str) {
            return str;
        }
        static inline const QString& deconvert(const QString& src) {
            return src;
        }
    };

    template <>
    struct StringConvertor<std::string> {
        static constexpr bool value = true;
        static inline std::string convert(const QString& str) {
            return str.toStdString();
        }
        static inline QString deconvert(const std::string& src) {
            return QString::fromStdString(src);
        }
    };

    template <>
    struct StringConvertor<QByteArray> {
        static constexpr bool value = true;
        static inline QByteArray convert(const QString& str) {
            return str.toUtf8();
        }
        static inline QString deconvert(const QByteArray& src) {
            return QString::fromUtf8(src);
        }
    };

    template <typename T>
    struct is_string {
        static constexpr bool value = StringConvertor<std::decay_t<T>>::value;
    };

    template <typename T, typename Nullable>
    struct NullableHandler;

    template <typename T>
    struct NullableHandler<T, std::optional<T>> {
        inline static decltype(auto) convert(const T& value) {
            return value;
        }
        constexpr inline static decltype(auto) make_empty() {
            return std::nullopt;
        }
    };

    template <typename T>
    struct NullableHandler<T, T*> {
        inline static T* convert(const T& value) {
            return new T(value);
        }
        constexpr inline static T* make_empty() {
            return nullptr;
        }
    };

    template <>
    struct NullableHandler<const char*, const char*> {
        inline static const char* convert(const char* value) {
            return value;
        }
        constexpr inline static const char* make_empty() {
            return nullptr;
        }
    };


    template <>
    struct NullableHandler<char*, char*> {
        inline static char* convert(char* value) {
            return value;
        }
        constexpr inline static char* make_empty() {
            return nullptr;
        }
    };

    template <typename T>
    struct is_nullable {
        using Type = void;
        static constexpr bool value = false;
    };

    template <typename T>
    struct is_nullable<T*> {
        using Type = T;
        static constexpr bool value = true;
    };

    template <typename T>
    struct is_nullable<std::optional<T>> {
        using Type = T;
        static constexpr bool value = true;
    };

    enum class Trait : uint8_t {
        NUL = 0,
        OBJECT = 1,
        ARRAY = 2,
        NULLABLE = 4,
        OPTIONAL = 8,
    };

    class DeserialisableBase {
    public:
        const bool anonymous = false;
        const Trait flag;
        const QString identifier;

    protected:
        DeserialisableBase(Trait _as = Trait::NUL) : anonymous(true), flag(_as) {}
        DeserialisableBase(const QString& name, Trait _as = Trait::NUL) : flag(_as), identifier(name) {}
        DeserialisableBase(const DeserialisableBase&) = delete;
        DeserialisableBase& operator=(const DeserialisableBase&) = delete;

    public:
        virtual ~DeserialisableBase() {}

        virtual void assign(const QJsonValue& value) = 0;
        virtual QJsonValue to_json() const = 0;
        void append(QJsonObject& parent) const {
            if (anonymous)
                throw std::ios_base::failure("JSON Structure Invalid!");
            parent.insert(identifier, to_json());
        }
        void append(QJsonArray& parent) const {
            if (!anonymous)
                throw std::ios_base::failure("JSON Structure Invalid!");
            parent.append(to_json());
        }
    };

    template <typename T = void, typename... Args>
    constexpr bool isValid() {
        return std::is_same_v<T, void> || std::is_base_of_v<DeserialisableBase, T>&& isValid<Args...>();
    }

    class JsonDeserialiser {
        template <int N>
        struct FixedArray {
            DeserialisableBase* arr[N];

            template <typename... Args>
            FixedArray(Args... args) : arr{ args... } {}
        };
        union _ {
            DeserialisableBase** value;
            DeserialisableBase* front;

            _(DeserialisableBase** value) : value(value) {}
            _(DeserialisableBase* front) : front(front) {}
        } data;
        const int complexity;
        const bool is_array;
        const bool delete_after_used;

    public:
        template <typename First, typename Second, typename... Args>
        JsonDeserialiser(First* first, Second* second, Args*... args)
            : data((new FixedArray<sizeof...(Args) + 2>{ first, second, args... })->arr),
            complexity(sizeof...(Args) + 2), is_array(false), delete_after_used(true)
        {}
        template <typename First, typename Second, typename... Args>
        JsonDeserialiser(First& first, Second& second, Args&... args)
            : data((new FixedArray<sizeof...(Args) + 2>{ &first, &second, &args... })->arr),
            complexity(sizeof...(Args) + 2), is_array(false), delete_after_used(false)
        {}
        JsonDeserialiser(DeserialisableBase* front)
            : data(front), complexity(0), is_array(uint8_t(front->flag)& uint8_t(Trait::ARRAY)),
            delete_after_used(true)
        {}
        JsonDeserialiser(DeserialisableBase& front)
            : data(&front), complexity(0), is_array(uint8_t(front.flag)& uint8_t(Trait::ARRAY)),
            delete_after_used(false)
        {}
        ~JsonDeserialiser() {
            if (complexity) {
                if (delete_after_used)
                    for (auto i = 0; i < complexity; ++i)
                        delete data.value[i];
                free(data.value);
            }
            else if (delete_after_used)
                delete data.front;
        }

        void deserialiseFile(const QString& filepath) {
            QFile file(filepath);
            if (!file.open(QFile::ReadOnly))
                throw std::ios_base::failure("Failed to Open File!");
            QJsonParseError parser;
            QJsonDocument data = QJsonDocument::fromJson(file.readAll(), &parser);
            if (parser.error != QJsonParseError::NoError)
                throw std::ios_base::failure("JSON Parsing Failed!");
            if (data.isArray())
                deserialise(data.array());
            else if (data.isObject())
                deserialise(data.object());
            file.close();
        }
        inline void deserialise(const char* json) {
            deserialise(QByteArray(json));
        }
        inline void deserialise(const QString& json) {
            deserialise(json.toUtf8());
        }
        void deserialise(const QByteArray& json) {
            QJsonParseError parser;
            QJsonDocument result = QJsonDocument::fromJson(json, &parser);
            if (parser.error != QJsonParseError::NoError)
                throw std::ios_base::failure("JSON Parsing Failed!");
            if (result.isArray())
                deserialise(result.array());
            else if (result.isObject())
                deserialise(result.object());
        }
        void deserialise(const QJsonObject& object) {
            if (is_array)
                throw std::ios_base::failure("JSON Structure Incompatible!");
            if (complexity)
                for (auto i = 0; i < complexity; ++i) {
                    auto deseriliser = data.value[i];
                    bool contain;
                    if (deseriliser->anonymous || !((contain = object.contains(deseriliser->identifier)) || (uint8_t(deseriliser->flag) & uint8_t(Trait::OPTIONAL))))
                        throw std::ios_base::failure("JSON Structure Incompatible!");
                    if (contain)
                        deseriliser->assign(object[deseriliser->identifier]);
                }
            else if (data.front->anonymous)
                data.front->assign(object);
            else
                throw std::ios_base::failure("JSON Structure Incompatible!");
        }
        void deserialise(const QJsonArray& json) {
            if (!is_array)
                throw std::ios_base::failure("JSON Structure Incompatible!");
            data.front->assign(json);
        }
        QByteArray serialise(bool compress = false) const {
            QJsonDocument json;
            if (is_array)
                json.setArray(data.front->to_json().toArray());
            else if (!complexity)
                json.setObject(data.front->to_json().toObject());
            else {
                QJsonObject obj;
                for (auto i = 0; i < complexity; ++i) {
                    const auto seriliser = data.value[i];
                    obj.insert(seriliser->identifier, seriliser->to_json());
                }
                json.setObject(obj);
            }
            return json.toJson(QJsonDocument::JsonFormat(compress));
        }
        QJsonValue serialise_to_json() const {
            if (complexity) {
                QJsonObject obj;
                for (auto i = 0; i < complexity; ++i) {
                    const auto seriliser = data.value[i];
                    obj.insert(seriliser->identifier, seriliser->to_json());
                }
                return obj;
            }
            return data.front->to_json();
        }
        void serialise_to_file(const QString& filepath) const {
            const auto data = serialise();
            QFile file(filepath);
            if (!file.open(QFile::WriteOnly))
                throw std::ios_base::failure("Failed to Open File!");
            file.write(data);
            file.close();
        }
    };

    struct JsonSerialiser {
        const JsonDeserialiser serialiser;

        template <typename First, typename Second, typename... Args>
        JsonSerialiser(const First* first, const Second* second, const Args*... args)
            : serialiser(const_cast<First*>(first), const_cast<Second*>(second),
                const_cast<Args*>(args)...) {}
        template <typename First, typename Second, typename... Args>
        JsonSerialiser(const First& first, const Second& second, const Args&... args)
            : serialiser(const_cast<First&>(first), const_cast<Second&>(second),
                const_cast<Args&>(args)...) {}
        JsonSerialiser(const DeserialisableBase* front)
            : serialiser(const_cast<DeserialisableBase*>(front)) {}
        JsonSerialiser(const DeserialisableBase& front)
            : serialiser(const_cast<DeserialisableBase&>(front)) {}

        inline QByteArray serialise(bool compress = false) const {
            return serialiser.serialise(compress);
        }
        inline QJsonValue serialise_to_json() const {
            return serialiser.serialise_to_json();
        }
        inline void serialise_to_file(const QString& filepath) const {
            serialiser.serialise_to_file(filepath);
        }
    };

    class Boolean : public DeserialisableBase {
        using Target = bool;
        bool& value;

    public:
        Boolean(bool& source) : value(source) {}
        Boolean(const QString& name, bool& source, bool optional = false) : DeserialisableBase(name, optional ? Trait::OPTIONAL : Trait::NUL), value(source) {}

        virtual void assign(const QJsonValue& data) override {
            if (data.isString()) {
                auto str = data.toString().toLower();
                if (str == "true")
                    value = true;
                else if (str == "false")
                    value = false;
                else if (str.isEmpty())
                    value = false;
                throw std::ios_base::failure("Type Unmatch!");
            } else if (data.isNull())
                value = false;
            else if (data.isBool())
                value = data.toBool();
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        virtual QJsonValue to_json() const override {
            return value;
        }
    };

    template <typename T = int32_t, bool sign = true, size_t size = 4>
    class Integer;

    template <typename T>
    class Integer<T, true, 4> : public DeserialisableBase {
        using Target = int32_t;
        int32_t& value;

    public:
        Integer(int32_t& source) : value(source) {}
        Integer(const QString& name, int32_t& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait::OPTIONAL : Trait::NUL), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (data.isString())
                value = data.toString().toInt();
            else if (data.isDouble())
                value = data.toInt();
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        virtual QJsonValue to_json() const override {
            return value;
        }
    };

    template <typename T>
    class Integer<T, false, 4> : public DeserialisableBase {
        using Target = uint32_t;
        uint32_t& value;

    public:
        Integer(uint32_t& source) : value(source) {}
        Integer(const QString& name, uint32_t& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait::OPTIONAL : Trait::NUL), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (data.isString())
                value = data.toString().toUInt();
            else if (data.isDouble())
                value = data.toVariant().toUInt();
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        virtual QJsonValue to_json() const override {
            return (qint64)value;
        }
    };

    template <typename T = double, size_t size = 8>
    class Real;

    template <typename T>
    class Real<T, 8> : public DeserialisableBase {
        using Target = double;
        double& value;

    public:
        Real(double& source) : value(source) {}
        Real(const QString& name, double& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait::OPTIONAL : Trait::NUL), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (data.isString())
                value = data.toString().toDouble();
            else if (data.isDouble())
                value = data.toDouble();
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        virtual QJsonValue to_json() const override {
            return value;
        }
    };

    template <typename T>
    class String : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;

    public:
        String(Target& source) : value(source) {}
        String(const QString& name, Target& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait::OPTIONAL : Trait::NUL), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isString() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            value = StringConvertor<Target>::convert(data.toString());
        }
        virtual QJsonValue to_json() const override {
            return StringConvertor<Target>::deconvert(value);
        }
    };

    template <size_t length>
    class String<char[length]> : public DeserialisableBase {
        using Target = char[length];
        Target& value;

    public:
        String(Target& source) : value(source) {}
        String(const QString& name, Target& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait::OPTIONAL : Trait::NUL), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isString() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            auto str = data.toString().toUtf8();
            const char* c_str = str.constData();
            size_t size = length < data.toString().length() ? length : data.toString().length();
            strncpy(value, c_str, size);
            value[size] = '\0';
        }
        virtual QJsonValue to_json() const override {
            return StringConvertor<Target>::deconvert(value);
        }
    };

    template <typename T, typename StringType>
    class NullableString : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;

    public:
        NullableString(Target& source) : DeserialisableBase(Trait::NULLABLE), value(source) {}
        NullableString(const QString& name, Target& source)
            : DeserialisableBase(name, Trait::NULLABLE), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isString() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            if (!data.isNull()) 
                value = NullableHandler<StringType, T>::convert(StringConvertor<StringType>::convert(data.toString()));
        }
        virtual QJsonValue to_json() const override {
            if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<StringType>>, char>)
                return value ? value : QJsonValue();
            else
                return value ? StringConvertor<StringType>::deconvert(*value) : QJsonValue();
        }
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
        template <typename U, typename V, typename = decltype(std::declval<U>().push_back(std::declval<V>()))>
        static constexpr bool is_pushback(int* p) {
            return true;
        }
        template <typename...>
        static constexpr bool is_pushback(...) {
            return false;
        }
        template <typename U, typename V, typename = decltype(std::declval<U>().append(std::declval<V>()))>
        static constexpr bool is_append(int* p) {
            return true;
        }
        template <typename...>
        static constexpr bool is_append(...) {
            return false;
        }
        template <typename U, typename V, typename = decltype(std::declval<U>().insert(std::declval<V>()))>
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

    template <typename T, typename StringType>
    class StringArray : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;

    public:
        StringArray(Target& source) : DeserialisableBase(Trait::ARRAY), value(source) {}
        StringArray(const QString& name, Target& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(Trait::ARRAY)) : Trait::ARRAY), value(source)
        {}

        virtual std::enable_if_t<bool(GetArrayInsertWay<T, StringType>::value)> assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray())
                if constexpr (uint8_t(GetArrayInsertWay<T, StringType>::value) & uint8_t(ArrayInsertWay::Push_Back))
                    value.push_back(StringConvertor<StringType>::convert(i.toString()));
                else if constexpr (uint8_t(GetArrayInsertWay<T, StringType>::value) & uint8_t(ArrayInsertWay::Append))
                    value.append(StringConvertor<StringType>::convert(i.toString()));
                else if constexpr (uint8_t(GetArrayInsertWay<T, StringType>::value) & uint8_t(ArrayInsertWay::Insert))
                    value.insert(StringConvertor<StringType>::convert(i.toString()));
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value)
                array.append(StringConvertor<StringType>::deconvert(i));
            return array;
        }
    };

    template <typename T, typename NullableStringType, typename StringType>
    class NullableStringArray : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;

    public:
        constexpr static Trait flag = Trait(uint8_t(Trait::NULLABLE) | uint8_t(Trait::ARRAY));
        NullableStringArray(Target& source) : DeserialisableBase(flag), value(source) {}
        NullableStringArray(const QString& name, Target& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(flag)) : flag), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray())
                if (!i.isNull()) {
                    if constexpr (uint8_t(GetArrayInsertWay<T, StringType>::value) & uint8_t(ArrayInsertWay::Push_Back))
                        value.push_back(NullableHandler<NullableStringType, StringType>::convert(StringConvertor<StringType>::convert(i.toString())));
                    else if constexpr (uint8_t(GetArrayInsertWay<T, StringType>::value) & uint8_t(ArrayInsertWay::Append))
                        value.append(NullableHandler<NullableStringType, StringType>::convert(StringConvertor<StringType>::convert(i.toString())));
                    else if constexpr (uint8_t(GetArrayInsertWay<T, StringType>::value) & uint8_t(ArrayInsertWay::Insert))
                        value.insert(NullableHandler<NullableStringType, StringType>::convert(StringConvertor<StringType>::convert(i.toString())));
                }
                else {
                    if constexpr (uint8_t(GetArrayInsertWay<T, StringType>::value) & uint8_t(ArrayInsertWay::Push_Back))
                        value.push_back(NullableHandler<NullableStringType, StringType>::make_empty());
                    else if constexpr (uint8_t(GetArrayInsertWay<T, StringType>::value) & uint8_t(ArrayInsertWay::Append))
                        value.append(NullableHandler<NullableStringType, StringType>::make_empty());
                    else if constexpr (uint8_t(GetArrayInsertWay<T, StringType>::value) & uint8_t(ArrayInsertWay::Insert))
                        value.insert(NullableHandler<NullableStringType, StringType>::make_empty());
                }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value)
                if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<StringType>>, char>)
                    array.append(i ? i : QJsonValue());
                else
                    array.append(i ? StringConvertor<StringType>::deconvert(*i) : QJsonValue());
            return array;
        }
    };

    template <typename T, typename StringType, std::size_t N>
    class LimitedStringArray : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;

    public:
        LimitedStringArray(Target& source) : DeserialisableBase(Trait::ARRAY), value(source) {}
        LimitedStringArray(const QString& name, Target& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(Trait::ARRAY)) : Trait::ARRAY), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            int count = 0;
            for (const auto& i : data.toArray()) {
                if (count > N - 1)
                    throw std::ios_base::failure("Array Out of Range!");
                value[count++] = StringConvertor<StringType>::convert(i.toString());
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value) {
                QString str = StringConvertor<StringType>::deconvert(i);
                array.append(str);
            }
            return array;
        }
    };

    template <typename T, typename NullableStringType, typename StringType, std::size_t N>
    class LimitedNullableStringArray : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;

    public:
        constexpr static Trait flag = Trait(uint8_t(Trait::NULLABLE) | uint8_t(Trait::ARRAY));
        LimitedNullableStringArray(Target& source) : DeserialisableBase(flag), value(source) {}
        LimitedNullableStringArray(const QString& name, Target& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(flag)) : flag), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            int count = 0;
            for (const auto& i : data.toArray()) {
                if (count > N - 1)
                    throw std::ios_base::failure("Array Out of Range!");
                if (!i.isNull())
                    value[count++] = NullableHandler<NullableStringType, StringType>::convert(StringConvertor<StringType>::convert(i.toString()));
                else
                    value[count++] = NullableHandler<NullableStringType, StringType>::make_empty();
            }
        }
        virtual QJsonValue to_json() const {
            QJsonArray array;
            for (const auto& i : value) {
                if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<StringType>>, char>)
                    array.append(i ? i : QJsonValue());
                else
                    array.append(i ? StringConvertor<StringType>::deconvert(*i) : QJsonValue());
            }
            return array;
        }
    };

    // Primary Template
    template <typename Any, bool isArray = false, int size = -1, typename TypeInArray = void, int isNullable = -1, typename TypeInNullable = void, int isString = -1>
    struct _Deserialisable;

    template <typename Any, bool isArray = false, int size = -1, typename TypeInArray = void, int isNullable = -1, typename TypeInNullable = void, int isString = -1>
    using _DeserialisableType = typename _Deserialisable<std::decay_t<Any>, isArray, size, TypeInArray, isNullable, TypeInNullable, isString>::Type;

    template <typename Any>
    struct Deserialisable {
        using Type = _DeserialisableType<Any>;
    };

    template <typename Any>
    using DeserialisableType = typename Deserialisable<std::decay_t<Any>>::Type;

    template <class T>
    class Object : public DeserialisableBase {
        using Type = T;
        JsonDeserialiser deserialiser;

    public:
        template <typename... WrappedMembers>
        Object(T* _, WrappedMembers*... members) : DeserialisableBase(Trait::OBJECT), deserialiser(members...) {}
        template <typename... WrappedMembers>
        Object(const QString& name, T* _, WrappedMembers*... members) : DeserialisableBase(name, Trait::OBJECT), deserialiser(members...) {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isObject() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            if (!data.isNull())
                deserialiser.deserialise(data.toObject());
        }
        virtual QJsonValue to_json() const override {
            return deserialiser.serialise_to_json();
        }
    };

    struct ObjectArrayInfo {
        const char* name;
        size_t offset;

        ObjectArrayInfo(const char* id, size_t offset) : name(id), offset(offset) {}
    };

    template <typename>
    struct Info : ObjectArrayInfo {
        Info(const char* id, size_t offset) : ObjectArrayInfo(id, offset) {}
    };

    template <typename T, class ObjectType, typename... Members>
    class ObjectArray : public DeserialisableBase {
        using Target = T;
        using Prototype = Object<ObjectType>;
        T& value;
        static constexpr int size = sizeof...(Members);
        ObjectArrayInfo info[size];

    public:
        ObjectArray(T& source, ObjectType* object_ptr, Info<Members>&&... members)
            : DeserialisableBase(Trait::ARRAY), value(source), info{ std::move(members)... } {}
        ObjectArray(const QString& name, T& source, ObjectType* object_ptr, Info<Members>&&... members)
            : DeserialisableBase(name, Trait::ARRAY), value(source), info{ std::move(members)... } {}

        virtual std::enable_if_t<bool(GetArrayInsertWay<T, ObjectType>::value)> assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray()) {
                if constexpr (!GetArrayInsertWay<T, ObjectType>::insert_only) {
                    auto ptr = &GetArrayInsertWay<T, ObjectType>::push_back(value);
                    int nameCount = size, dataCount = size;
                    Prototype deserialiser((ObjectType*)nullptr, new DeserialisableType<Members>(info[--nameCount].name, *reinterpret_cast<Members*>(reinterpret_cast<uint8_t*>(ptr) + info[--dataCount].offset))...);
                    deserialiser.assign(i);
                }
                else {
                    ObjectType tmp;
                    int nameCount = size, dataCount = size;
                    Prototype deserialiser((ObjectType*)nullptr, new DeserialisableType<Members>(info[--nameCount].name, *reinterpret_cast<Members*>(reinterpret_cast<uint8_t*>(&tmp) + info[--dataCount].offset))...);
                    deserialiser.assign(i);
                    value.insert(std::move(tmp));
                }
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value) {
                const auto ptr = &i;
                int nameCount = size, dataCount = size;
                const Prototype serialiser((ObjectType*)nullptr, new DeserialisableType<Members>(info[--nameCount].name, const_cast<Members&>(*reinterpret_cast<const Members*>(reinterpret_cast<const uint8_t*>(ptr) + info[--dataCount].offset)))...);
                array.append(serialiser.to_json());
            }
            return array;
        }
    };

    template <typename T, typename TrivialType>
    class Array : public DeserialisableBase {
        using Target = std::decay_t<T>;
        using Prototype = DeserialisableType<TrivialType>;
        Target& value;

    public:
        Array(Target& source) : DeserialisableBase(Trait::ARRAY), value(source) {}
        Array(const QString& name, Target& source, bool optional = false) : DeserialisableBase(name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(Trait::ARRAY)) : Trait::ARRAY), value(source) {}

        virtual std::enable_if_t<bool(GetArrayInsertWay<T, TrivialType>::value)> assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray()) {
                if constexpr (!GetArrayInsertWay<T, TrivialType>::insert_only) {
                    Prototype deserialiser(GetArrayInsertWay<T, TrivialType>::push_back(value));
                    deserialiser.assign(i);
                }
                else {
                    TrivialType tmp;
                    Prototype deserialiser(tmp);
                    deserialiser.assign(i);
                    value.insert(std::move(tmp));
                }
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value) {
                const Prototype deserialiser(const_cast<TrivialType&>(i));
                array.append(deserialiser.to_json());
            }
            return array;
        }
    };

    template <typename T, typename TrivialType, std::size_t N>
    class LimitedArray : public DeserialisableBase {
        using Target = std::decay_t<T>;
        using Prototype = DeserialisableType<TrivialType>;
        Target& value;

    public:
        LimitedArray(Target& source) : DeserialisableBase(Trait::ARRAY), value(source) {}
        LimitedArray(const QString& name, Target& source, bool optional = false) : DeserialisableBase(name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(Trait::ARRAY)) : Trait::ARRAY), value(source) {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray()) {
                if (value.size() == N)
                    throw std::ios_base::failure("Array Out of Range!");
                if constexpr (!GetArrayInsertWay<T, TrivialType>::insert_only) {
                    Prototype deserialiser(GetArrayInsertWay<T, TrivialType>::push_back(value));
                    deserialiser.assign(i);
                }
                else {
                    TrivialType tmp;
                    Prototype deserialiser(tmp);
                    deserialiser.assign(i);
                    value.insert(std::move(tmp));
                }
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value) {
                const Prototype deserialiser(const_cast<TrivialType&>(i));
                array.append(deserialiser.to_json());
            }
            return array;
        }
    };

    template <typename T, typename TypeInNullable>
    class Nullable : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;

    public:
        Nullable(Target& source) : DeserialisableBase(Trait::NULLABLE), value(source) {}
        Nullable(const QString& name, T& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(Trait::NULLABLE)) : Trait::NULLABLE), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (data.isNull()) {
                value = NullableHandler<TypeInNullable, Target>::make_empty();
                return;
            }
            TypeInNullable tmp;
            DeserialisableType<TypeInNullable> deserialiser(tmp);
            deserialiser.assign(data);
            value = NullableHandler<TypeInNullable, Target>::convert(tmp);
        }
        virtual QJsonValue to_json() const override {
            return value ? DeserialisableType<TypeInNullable>(*value).to_json() : QJsonValue();
        }
    };

    template <typename T, typename As>
    class NonTrivial : public DeserialisableBase {
        using Target = std::decay_t<T>;
        DeserialisableType<As> value;

    public:
        NonTrivial(Target& source) : value(reinterpret_cast<As&>(source)) {}
        NonTrivial(const QString& name, Target& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait::OPTIONAL : Trait::NUL), value(name, reinterpret_cast<As&>(source))
        {}

        virtual void assign(const QJsonValue& data) override {
            value.assign(data);
        }
        virtual QJsonValue to_json() const override {
            return value.to_json();
        }
    };

    template<typename T, const char* json_name, size_t member_offset>
    struct Customised {
        using Type = DeserialisableType<T>;
    };

    template <typename T, const char* json_name, size_t member_offset, bool optional_member = false, typename Custom = Customised<T, json_name, member_offset>::Type>
    struct ReinforcedInfo {
        using Type = T;
        using Prototype = Custom;
        static constexpr const char* name = json_name;
        static constexpr size_t offset = member_offset;
        static constexpr bool optional = optional_member;
    };

    template <typename ObjectType, typename... MemberInfo>
    class DeserialisableObject : public DeserialisableBase {
        using Target = ObjectType;
        Object<ObjectType> deserialiser;

    public:
        DeserialisableObject(ObjectType& value)
            : DeserialisableBase(Trait::OBJECT),
            deserialiser(&value,
                new typename MemberInfo::Prototype(MemberInfo::name, (typename MemberInfo::Type&)* ((uint8_t*)(&value) + MemberInfo::offset), MemberInfo::optional)...)
        {}
        DeserialisableObject(QString json_name, ObjectType& value, bool optional = false)
            : DeserialisableBase(json_name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(Trait::OBJECT)) : Trait::OBJECT),
            deserialiser(json_name, &value,
                new typename MemberInfo::Prototype(MemberInfo::name, (typename MemberInfo::Type&)* ((uint8_t*)(&value) + MemberInfo::offset), MemberInfo::optional)...)
        {}

        virtual void assign(const QJsonValue& data) override {
            deserialiser.assign(data);
        }
        virtual QJsonValue to_json() const override {
            return deserialiser.to_json();
        }
    };

    template <typename T>
    class SelfDeserialisableObject : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;

    public:
        SelfDeserialisableObject(Target& source) : DeserialisableBase(Trait::OBJECT), value(source) {}
        SelfDeserialisableObject(QString name, Target& source, bool optional = false)
            : DeserialisableBase(name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(Trait::OBJECT)) : Trait::OBJECT), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isObject() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            if (data.isObject())
                value = std::move(Target(data.toObject()));
        }
        virtual QJsonValue to_json() const override {
            return value.to_json();
        }
    };

    template <typename T, typename KeyType, typename ValueType, const char* key = nullptr>
    class MapArray : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;

    public:
        MapArray(Target& source) : DeserialisableBase(Trait::ARRAY), value(source) {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray()) {
                KeyType key_value;
                ValueType value_value;
                DeserialisableType<KeyType> key_deserialiser(key_value);
                DeserialisableType<ValueType> value_deserialiser(value_value);
                key_deserialiser.assign(i.toObject()[key]);
                value_deserialiser.assign(i);
                value[key_value] = value_value;
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& [key_, value_] : value) {
                KeyType key_value = key_;
                ValueType value_value = value_;
                DeserialisableType<KeyType> key_deserialiser(key_value);
                DeserialisableType<ValueType> value_deserialiser(value_value);
                QJsonObject obj = value_deserialiser.to_json().toObject();
                obj.insert(key, key_deserialiser.to_json());
                array.append(obj);
            }
            return array;
        }
    };

    template <typename T, typename KeyType, typename ValueType>
    class MapArray<T, KeyType, ValueType, nullptr> : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;
        QString key;
        std::optional<QString> val_name;

    public:
        MapArray(Target& source, QString key_name)
            : DeserialisableBase(Trait::ARRAY), value(source), key(key_name) {}
        MapArray(Target& source, QString key_name, QString val_json_name)
            : DeserialisableBase(Trait::ARRAY), value(source), key(key_name), val_name(val_json_name) {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray()) {
                KeyType key_value;
                ValueType value_value;
                DeserialisableType<KeyType> key_deserialiser(key_value);
                DeserialisableType<ValueType> value_deserialiser(value_value);
                key_deserialiser.assign(i.toObject()[key]);
                if (val_name)
                    value_deserialiser.assign(i.toObject()[*val_name]);
                else
                    value_deserialiser.assign(i);
                value[key_value] = value_value;
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& [key_, value_] : value) {
                KeyType key_value = key_;
                ValueType value_value = value_;
                DeserialisableType<KeyType> key_deserialiser(key_value);
                DeserialisableType<ValueType> value_deserialiser(value_value);
                QJsonObject obj;
                if (val_name)
                    obj = value_deserialiser.to_json().toObject();
                else
                    obj.insert(*val_name, value_deserialiser.to_json());
                obj.insert(key, key_deserialiser.to_json());
                array.append(obj);
            }
            return array;
        }
    };

    template <class Base, class T, typename... MemberInfo>
    class DerivedObject : public DeserialisableBase {
        using Target = T;
        Object<T> deserialiser;
        std::enable_if_t<std::is_base_of_v<Base, T>, DeserialisableType<Base>> base_deserialiser;

    public:
        DerivedObject(T& value)
            : DeserialisableBase(Trait::OBJECT),
            deserialiser(&value,
                new DeserialisableType<typename MemberInfo::Type>(MemberInfo::name, (typename MemberInfo::Type&)* ((uint8_t*)(&value) + MemberInfo::offset), MemberInfo::optional)...),
            base_deserialiser(static_cast<Base&>(value))
        {}
        DerivedObject(const QString& json_name, T& value, bool optional = false)
            : DeserialisableBase(json_name, optional ? Trait(uint8_t(Trait::OPTIONAL) | uint8_t(Trait::OBJECT)) : Trait::OBJECT),
            deserialiser(json_name, &value,
                new DeserialisableType<typename MemberInfo::Type>(MemberInfo::name, (typename MemberInfo::Type&)* ((uint8_t*)(&value) + MemberInfo::offset), MemberInfo::optional)...),
            base_deserialiser(static_cast<Base&>(value))
        {}

        virtual void assign(const QJsonValue& data) override {
            deserialiser.assign(data);
            base_deserialiser.assign(data);
        }
        virtual QJsonValue to_json() const override {
            QJsonObject result = deserialiser.to_json().toObject();
            QJsonObject base = base_deserialiser.to_json().toObject();
            for (auto i = base.begin(); i != base.end(); i++)
                result.insert(i.key(), i.value());
            return result;
        }
    };

    template <typename T, typename Type1, typename Type2>
    class Pair : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;
        QString name[2];

    public:
        Pair(Target& source, const QString& name1, const QString& name2)
            : DeserialisableBase(Trait::OBJECT), value(source), name{ name1, name2 } {}
        Pair(const QString& json_name, Target& source, const QString& name1, const QString& name2)
            : DeserialisableBase(json_name, Trait::OBJECT), value(source), name{ name1, name2 } {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isObject() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            Type1& element1 = value.first;
            Type2& element2 = value.second;
            DeserialisableType<Type1> deserialiser1(element1);
            DeserialisableType<Type2> deserialiser2(element2);
            deserialiser1.assign(data.toObject()[name[0]]);
            deserialiser2.assign(data.toObject()[name[1]]);
        }
        virtual QJsonValue to_json() const override {
            QJsonObject pair;
            Type1& element1 = value.first;
            Type2& element2 = value.second;
            DeserialisableType<Type1> deserialiser1(element1);
            DeserialisableType<Type2> deserialiser2(element2);
            pair.insert(name[0], deserialiser1.to_json());
            pair.insert(name[1], deserialiser2.to_json());
            return pair;
        }
    };

    template <typename T, typename PairType>
    class PairArray : public DeserialisableBase {
        using Target = std::decay_t<T>;
        Target& value;
        QString name[2];

    public:
        PairArray(Target& source, const QString& name1, const QString& name2)
            : DeserialisableBase(Trait::ARRAY), value(source), name{ name1, name2 } {}

        virtual std::enable_if_t<bool(GetArrayInsertWay<T, PairType>::value)>
            assign(const QJsonValue& data) override {
            if (!data.isArray() && !data.isNull())
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : data.toArray()) {
                if constexpr (!GetArrayInsertWay<T, PairType>::insert_only) {
                    DeserialisableType<PairType> deserialiser(GetArrayInsertWay<T, PairType>::push_back(value), name[0], name[1]);
                    deserialiser.assign(i);
                }
                else {
                    PairType tmp;
                    DeserialisableType<PairType> deserialiser(tmp, name[0], name[1]);
                    deserialiser.assign(i);
                    value.insert(std::move(tmp));
                }
            }
        }
        virtual QJsonValue to_json() const override {
            QJsonArray array;
            for (const auto& i : value) {
                DeserialisableType<PairType> deserialiser(i, name[0], name[1]);
                array.append(deserialiser.to_json());
            }
            return array;
        }
    };

    template <typename Des, typename ConvertFunctor = Des(const QJsonValue&), typename DeconvertFunctor = QJsonValue(const Des&), typename BasicType = decltype(std::declval<DeconvertFunctor>()(std::declval<Des>()))>
    struct Convertor {
        using Type = BasicType;
        using Target = std::decay_t<Des>;
        using Source = std::decay_t<BasicType>;
        decltype(std::function(std::declval<ConvertFunctor>())) convertor;
        decltype(std::function(std::declval<DeconvertFunctor>())) deconvertor;
        Convertor(ConvertFunctor& convertor, DeconvertFunctor& deconvertor)
            : convertor(convertor), deconvertor(deconvertor) {}
        explicit Convertor()
            : convertor{[](const QJsonValue& data) {
                  using T = DeserialisableType<Des>;
                  Des result;
                  T(result).assign(data);
                  return result;
            }},
            deconvertor{[](const Des& src) {
                using T = DeserialisableType<Des>;
                return T(const_cast<Des&>(src)).to_json();
            }}
        {}
        template <typename OtherConvertFunctor, typename otherDeconvertFunctor>
        Convertor(Convertor<Des, OtherConvertFunctor, otherDeconvertFunctor, BasicType>&& other)
            : convertor(std::move(other.convertor)), deconvertor(std::move(other.deconvertor)) {}
        template <typename OtherConvertFunctor, typename otherDeconvertFunctor>
        Convertor(const Convertor<Des, OtherConvertFunctor, otherDeconvertFunctor, BasicType>& other)
            : convertor(other.convertor), deconvertor(other.deconvertor) {}
    };

    template <typename Convertor>
    class Extension : public DeserialisableBase {
    public:
        using Type = DeserialisableType<typename Convertor::Type>;
        using Target = typename Convertor::Target;

    private:
        Target& value;
        mutable typename Convertor::Source tmp;
        std::decay_t<Convertor> convertor;
        Type prototype;

    public:
        Extension(Convertor&& convertor, Target& source)
            : value(source), convertor(convertor), prototype(tmp) {}
        Extension(Convertor&& convertor, const QString& json_name, Target& source, bool optional = false)
            : DeserialisableBase(json_name, optional ? Trait::OPTIONAL : Trait::NUL), value(source), convertor(convertor), prototype(tmp)
        {}
        Extension(const Convertor& convertor, Target& source)
            : value(source), convertor(convertor), prototype(tmp) {}
        Extension(const Convertor& convertor, const QString& json_name, Target& source, bool optional = false)
            : DeserialisableBase(json_name, optional ? Trait::OPTIONAL : Trait::NUL), value(source), convertor(convertor), prototype(tmp)
        {}

        virtual void assign(const QJsonValue& data) override {
            prototype.assign(data);
            value = convertor.convertor(tmp);
        }
        virtual QJsonValue to_json() const override {
            tmp = convertor.deconvertor(value);
            return prototype.to_json();
        }
    };

    template <typename Function>
    struct ArgTypeDeduction {
        using Type = typename ArgTypeDeduction<decltype(&Function::operator())>::Type;
    };

    template <typename R, class C, typename Arg>
    struct ArgTypeDeduction<R(C::*)(Arg) const> {
        using Type = Arg;
    };

    template <typename Des, typename ConvertFunctor, typename BasicType = std::decay_t<typename ArgTypeDeduction<decltype(std::function(std::declval<ConvertFunctor>()))>::Type>>
    struct DeserialiseOnlyConvertor {
        using Type = BasicType;
        using Target = std::decay_t<Des>;
        using Source = std::decay_t<BasicType>;
        decltype(std::function(std::declval<ConvertFunctor>())) convertor;
        DeserialiseOnlyConvertor(ConvertFunctor& convertor) : convertor(convertor) {}
        template <typename OtherConvertFunctor>
        DeserialiseOnlyConvertor(DeserialiseOnlyConvertor<Des, OtherConvertFunctor, BasicType>&& other) : convertor(std::move(other.convertor)) {}
    };

    template <typename Convertor>
    class DeserialiseOnlyExtension : public DeserialisableBase {
    public:
        using Type = DeserialisableType<typename Convertor::Type>;
        using Target = typename Convertor::Target;

    private:
        Target& value;
        mutable typename Convertor::Source tmp;
        std::decay_t<Convertor> convertor;
        Type prototype;

    public:
        DeserialiseOnlyExtension(Convertor&& convertor, Target& source)
            : value(source), convertor(convertor), prototype(tmp) {}
        DeserialiseOnlyExtension(Convertor&& convertor, const QString& json_name, Target& source)
            : DeserialisableBase(json_name), value(source), convertor(convertor), prototype(tmp) {}

        virtual void assign(const QJsonValue& data) override {
            prototype.assign(data);
            value = convertor.convertor(tmp);
        }
        virtual QJsonValue to_json() const override {
            throw std::ios_base::failure("Serialisation Unsupportted!");
        }
    };

    template <typename T, typename ConvertFunctor, typename Des = std::decay_t<decltype(std::declval<ConvertFunctor>()(std::declval<T>()))>>
    struct SerialiseOnlyConvertor {
        using Type = std::decay_t<T>;
        using Target = Des;
        using Source = std::decay_t<T>;
        decltype(std::function(std::declval<ConvertFunctor>())) convertor;
        SerialiseOnlyConvertor(ConvertFunctor& convertor) : convertor(convertor) {}
        template <typename OtherConvertFunctor>
        SerialiseOnlyConvertor(DeserialiseOnlyConvertor<T, OtherConvertFunctor, Des>&& other) : convertor(std::move(other.convertor)) {}
    };

    template <typename Convertor>
    class SerialiseOnlyExtension : public DeserialisableBase {
    public:
        using Type = DeserialisableType<typename Convertor::Target>;
        using Target = typename Convertor::Target;
        using Source = typename Convertor::Type;

    private:
        const Source& src;
        mutable Target value;
        std::decay_t<Convertor> convertor;
        Type prototype;

    public:
        SerialiseOnlyExtension(Convertor&& convertor, const Source& source)
            : src(source), convertor(convertor), prototype(value) {}
        SerialiseOnlyExtension(Convertor&& convertor, const QString& json_name, const Source& source)
            : DeserialisableBase(json_name), src(source), convertor(convertor), prototype(value) {}

        virtual void assign(const QJsonValue& data) override {
            throw std::ios_base::failure("Deserialisation Unsupportted!");
        }
        virtual QJsonValue to_json() const override {
            value = convertor.convertor(src);
            return prototype.to_json();
        }
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

        StaticTuple(Current&& source, Types&&... pack) : value(std::move(source)), next(std::forward<Types>(pack)...) {}

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

    struct FAM {
        virtual void run() = 0;
        virtual ~FAM() {}
    };

    template <typename Convertor, typename Target>
    struct Assign : public FAM {
        const QJsonValue& data;
        Convertor& convertor;
        Target& value;

        void run() override {
            value = std::move(convertor.convertor(data));
        }

        Assign(const QJsonValue& data, Convertor& convertor, Target& source)
            : data(data), convertor(convertor), value(source) {}
    };

    template <typename Convertor, typename Source>
    struct To_Json : public FAM {
        QJsonObject& result;
        const Convertor& convertor;
        const Source& value;

        void run() override {
            result = std::move(
                convertor.deconvertor(std::get<typename Convertor::Target>(value)).toObject());
        }

        To_Json(QJsonObject& data, const Convertor& convertor, const Source& source)
            : result(data), convertor(convertor), value(source) {}
    };

    template <int... pack>
    struct ConstexprArrayPack {
        constexpr static int length = sizeof...(pack);
        constexpr static int value[sizeof...(pack)] = { pack... };
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

    template <typename Standard, typename Tuple, int current = 0,
        typename Result = typename ConstexprConstantArray<Standard::length, -1>::Type,
        bool = Tuple::length <= 1>
    struct ConvertorAdapterReorganise;

    template <typename... Targets, int current, typename Last, typename Current, typename... Left>
    struct ConvertorAdapterReorganise<TypeTuple<Targets...>, TypeTuple<Current, Left...>, current, Last,
        false> {
        static constexpr int index = FindType<TypeTuple<Targets...>, typename Current::Target>::index;
        using Next =
            ConvertorAdapterReorganise<TypeTuple<Targets...>, TypeTuple<Left...>, current + 1,
            typename ConstexprArrayPackAlter<index, current, Last>::Type>;
        using Type = typename Next::Type;
    };

    template <typename... Targets, int current, typename Last, typename Current>
    struct ConvertorAdapterReorganise<TypeTuple<Targets...>, TypeTuple<Current>, current, Last, true> {
        static constexpr int index = FindType<TypeTuple<Targets...>, typename Current::Target>::index;
        using Type = typename ConstexprArrayPackAlter<index, current, Last>::Type;
    };

    template <typename... Targets, typename Result>
    struct ConvertorAdapterReorganise<TypeTuple<Targets...>, TypeTuple<void>, 0, Result, true> {
        using Type = Result;
    };

    template <int N, typename Target, typename Given>
    inline Convertor<Target> ConvertorGenerator(Given& given) {
        if constexpr (N == -1)
            return std::move(Convertor<Target>());
        else
            return std::move(given.template get<N>());
    };

    template <typename Target, typename Inc, typename... Given>
    struct ConvertorAdapter;

    template <typename... Targets, int... pack, typename... Given>
    struct ConvertorAdapter<std::variant<Targets...>, ConstexprArrayPack<pack...>, Given...> {
        using Array = typename ConvertorAdapterReorganise<TypeTuple<Targets...>, TypeTuple<Given...>>::Type;
        using Tuple = TypeTuple<Given...>;
        using Target = TypeTuple<Targets...>;
        StaticTuple<Convertor<Given>...> givens;
        StaticTuple<Convertor<Targets>...> convertors;

        ConvertorAdapter(Given&... given)
            : givens(given...), convertors(ConvertorGenerator<Array::value[pack], typename GetType<pack, Target>::Type>(givens)...)
        {}
    };

    template <typename Variant>
    struct GetNum;

    template <typename... Types>
    struct GetNum<std::variant<Types...>> {
        constexpr static int value = sizeof...(Types);
    };

    // Deductor = Convertor<int, ConvertFunctor, DeconvertFunctor, BasicType>;
    template <typename Deductor, typename Target, typename Inc = typename ConstexprIncArray<GetNum<Target>::value>::Type>
    class VarientObject;

    template <typename Deductor, typename... Targets, int... pack>
    class VarientObject<Deductor, std::variant<Targets...>, ConstexprArrayPack<pack...>>
        : public DeserialisableBase {
        using Target = std::variant<Targets...>;
        mutable int result = -1;
        Extension<Deductor> deductor;
        StaticTuple<Convertor<Targets>...> convertors;
        Target& value;

    public:
        template <typename... Given_Convertors>
        VarientObject(const QString& key, Deductor&& deduction, Target& source, Given_Convertors&&... convertors)
            : DeserialisableBase(Trait::OBJECT), deductor(std::move(deduction), key, result), convertors(ConvertorAdapter<Target, ConstexprArrayPack<pack...>, Given_Convertors...>(convertors...).convertors), value(source)
        {}
        template <typename... Given_Convertors>
        VarientObject(const QString& name, const QString& key, Deductor&& deduction, Target& source, Given_Convertors&&... convertors)
            : DeserialisableBase(name, Trait::OBJECT), deductor(std::move(deduction), key, result), convertors(ConvertorAdapter<Target, ConstexprArrayPack<pack...>, Given_Convertors...>(convertors...).convertors), value(source)
        {}

        virtual void assign(const QJsonValue& data) override {
            if (!data.isObject() || !data.toObject().contains(deductor.identifier))
                throw std::ios_base::failure("Type Unmatch!");
            deductor.assign(data.toObject()[deductor.identifier]);
            if (result == -1)
                throw std::ios_base::failure("Type Unmatch!");
            FAM* machines[sizeof...(Targets)] = { (new Assign(data, convertors.template get<pack>(), value))... };
            machines[result]->run();
            for (auto i : machines)
                delete i;
        }
        virtual QJsonValue to_json() const override {
            QJsonObject object;
            result = value.index();
            FAM* machines[sizeof...(Targets)] = { (new To_Json(object, convertors.template get<pack>(), value))... };
            machines[result]->run();
            for (auto i : machines)
                delete i;
            return object;
        }
    };

    struct JSONWrap : public DeserialisableBase {
        QJsonValue& value;
        JSONWrap(QJsonValue& src) : value(src) {}
        JSONWrap(const QString& name, QJsonValue& src, bool optional = false)
            : DeserialisableBase(name, optional ? Trait::OPTIONAL : Trait::NUL), value(src)
        {}

        virtual void assign(const QJsonValue& data) override {
            value = data;
        }
        virtual QJsonValue to_json() const override {
            return value;
        }
    };

    template <typename Any>
    struct _Deserialisable<Any, false, -1, void, -1, void, -1> {
        using Type = _DeserialisableType<Any, false, -1, void, is_nullable<Any>::value, typename is_nullable<Any>::Type, -1>;
    };

    template <typename Any>
    struct _Deserialisable<Any, false, -1, void, false, void, -1> {
        using Type = _DeserialisableType<Any, false, -1, void, false, void, is_string<Any>::value>;
    };

    template <typename T, typename TypeInNullable>
    struct _Deserialisable<T, false, -1, void, true, TypeInNullable, -1> {
        using Type = _DeserialisableType<T, false, -1, void, true, TypeInNullable, is_string<TypeInNullable>::value>;
    };

    template <typename T, typename TypeInArray>
    struct _Deserialisable<T, true, -1, TypeInArray, -1, void, -1> {
        using Type = _DeserialisableType<T, true, -1, TypeInArray, is_nullable<TypeInArray>::value, typename is_nullable<TypeInArray>::Type, -1>;
    };

    template <typename T, typename TypeInArray>
    struct _Deserialisable<T, true, -1, TypeInArray, false, void, -1> {
        using Type = _DeserialisableType<T, true, -1, TypeInArray, false, void, is_string<TypeInArray>::value>;
    };

    template <typename T, typename TypeInArray, typename TypeInNullable>
    struct _Deserialisable<T, true, -1, TypeInArray, true, TypeInNullable, -1> {
        using Type = _DeserialisableType<T, true, -1, TypeInArray, true, TypeInNullable, is_string<TypeInNullable>::value>;
    };

    template <typename StringType>
    struct _Deserialisable<StringType, false, -1, void, false, void, true> {
        using Type = String<StringType>;
    };

    template <typename T, typename StringType>
    struct _Deserialisable<T, false, -1, void, true, StringType, true> {
        using Type = NullableString<T, StringType>;
    };

    template <typename T, typename TypeInNullable>
    struct _Deserialisable<T, false, -1, void, true, TypeInNullable, false> {
        using Type = Nullable<T, TypeInNullable>;
    };

    template <typename T>
    struct Deserialisable<std::vector<T>> {
        using Type = _DeserialisableType<std::vector<T>, true, -1, T>;
    };

    template <typename T>
    struct Deserialisable<QList<T>> {
        using Type = _DeserialisableType<QList<T>, true, -1, T>;
    };

    template <typename T>
    struct Deserialisable<std::list<T>> {
        using Type = _DeserialisableType<std::list<T>, true, -1, T>;
    };

    template <typename T>
    struct Deserialisable<std::set<T>> {
        using Type = _DeserialisableType<std::set<T>, true, -1, T>;
    };

    template <typename T>
    struct Deserialisable<QSet<T>> {
        using Type = _DeserialisableType<QSet<T>, true, -1, T>;
    };

    template <typename TypeInArray, std::size_t N>
    struct Deserialisable<TypeInArray[N]> {
        using Type = _DeserialisableType<TypeInArray[N], true, N, TypeInArray>;
    };

    template <typename TypeInArray, std::size_t N>
    struct Deserialisable<std::array<TypeInArray, N>> {
        using Type = _DeserialisableType<std::array<TypeInArray, N>, true, N, TypeInArray>;
    };

    template <typename T, typename StringType>
    struct _Deserialisable<T, true, -1, StringType, false, void, true> {
        using Type = StringArray<T, StringType>;
    };

    template <typename T, typename Nullable, typename StringType>
    struct _Deserialisable<T, true, -1, Nullable, true, StringType, true> {
        using Type = NullableStringArray<T, Nullable, StringType>;
    };

    template <typename ArrayType, typename T>
    struct _Deserialisable<ArrayType, true, -1, T, false, void, false> {
        using Type = Array<ArrayType, T>;
    };

    template <typename ArrayType, typename T, int N>
    struct _Deserialisable<ArrayType, true, N, T, -1, void, -1> {
        using Type = LimitedArray<ArrayType, T, N>;
    };

    template <typename T, typename StringType, int N>
    struct _Deserialisable<T, true, N, StringType, false, void, true> {
        using Type = LimitedStringArray<T, StringType, N>;
    };

    template <typename T, typename Nullable, typename StringType, int N>
    struct _Deserialisable<T, true, N, Nullable, true, StringType, true> {
        using Type = LimitedNullableStringArray<T, Nullable, StringType, N>;
    };

    template <typename Key, typename Value>
    struct Deserialisable<std::map<Key, Value>> {
        using Type = MapArray<std::map<Key, Value>, Key, Value>;
    };

    template <typename Key, typename Value>
    struct Deserialisable<QMap<Key, Value>> {
        using Type = MapArray<QMap<Key, Value>, Key, Value>;
    };

    template <typename First, typename Second>
    struct Deserialisable<std::pair<First, Second>> {
        using Type = Pair<std::pair<First, Second>, First, Second>;
    };

    template <>
    struct Deserialisable<QJsonValue> {
        using Type = JSONWrap;
    };

    template <>
    struct Deserialisable<bool> {
        using Type = Boolean;
    };

    template <>
    struct Deserialisable<int> {
        using Type = Integer<int>;
    };

    template <>
    struct Deserialisable<unsigned> {
        using Type = Integer<unsigned, false>;
    };

    template <>
    struct Deserialisable<double> {
        using Type = Real<double>;
    };

    template <size_t length>
    struct Deserialisable<char[length]> {
        using Type = String<char[length]>;
    };

    template <>
    struct Deserialisable<const char*> {
        using Type = NullableString<const char*, const char*>;
    };

    template <>
    struct Deserialisable<char*> {
        using Type = NullableString<char*, char*>;
    };

} // namespace JsonDeserialise

// Local
#define declare_top_deserialiser(data_name, var_name)                                              \
    JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(data_name);
#define declare_deserialiser(json_name, data_name, var_name)                                       \
    JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(json_name, data_name);
#define declare_top_serialiser(data_name, var_name)                                                \
    const JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(                       \
        const_cast<std::decay_t<decltype(data_name)>&>(data_name));
#define declare_serialiser(json_name, data_name, var_name)                                         \
    const JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(                       \
        (json_name), const_cast<std::decay_t<decltype(data_name)>&>(data_name));
#define declare_extension_deserialiser(json_name, data_name, var_name, convertor, deconvertor)     \
    JsonDeserialise::Extension var_name(                                                           \
        JsonDeserialise::Convertor<decltype(data_name), decltype(convertor),                       \
                                   decltype(deconvertor)>((convertor), (deconvertor)),             \
        json_name, data_name);
#define declare_one_direction_extension_deserialiser(json_name, data_name, var_name, convertor)    \
    JsonDeserialise::DeserialiseOnlyExtension var_name(                                            \
        JsonDeserialise::DeserialiseOnlyConvertor<decltype(data_name), decltype(convertor)>(       \
            (convertor)),                                                                          \
        json_name, data_name);
#define declare_one_direction_extension_serialiser(json_name, data_name, var_name, deconvertor)    \
    JsonDeserialise::SerialiseOnlyExtension var_name(                                              \
        JsonDeserialise::SerialiseOnlyConvertor<decltype(data_name), decltype(deconvertor)>(       \
            (deconvertor)),                                                                        \
        json_name, data_name);
#define declare_top_simple_map_deserialiser(data_name, key_name, val_name, var_name)               \
    JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(data_name, key_name,         \
                                                                      val_name);
#define declare_simple_map_deserialiser(json_name, data_name, key_name, val_name, var_name)        \
    JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(json_name, data_name,        \
                                                                      key_name, val_name);
#define declare_top_object_map_object_deserialiser(data_name, key_name, var_name)                  \
    JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(data_name, key_name);
#define declare_object_map_deserialiser(json_name, data_name, key_name, var_name)                  \
    JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(json_name, data_name,        \
                                                                      key_name);
#define declare_top_pair_deserialiser(json_name1, json_name2, data_name, var_name)                 \
    JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(data_name, json_name1,       \
                                                                      json_name2);
#define declare_pair_deserialiser(json_name, json_name1, json_name2, data_name, var_name)          \
    JsonDeserialise::DeserialisableType<decltype(data_name)> var_name(json_name, data_name,        \
                                                                      json_name1, json_name2);
#define declare_top_object_deserialiser(var_name, ...)                                             \
    JsonDeserialise::JsonDeserialiser var_name(__VA_ARGS__);
#define declare_top_object_array_deserialiser(data_name, var_name, object_type, ...)               \
    JsonDeserialise::ObjectArray var_name(data_name, (object_type*)nullptr, __VA_ARGS__);
#define declare_object_array_deserialiser(json_name, data_name, var_name, object_type, ...)        \
    JsonDeserialise::ObjectArray var_name(json_name, data_name, (object_type*)nullptr, __VA_ARGS__);
#define array_field(object_type, json_name, member_name)                                           \
    JsonDeserialise::Info<decltype(((object_type*)nullptr)->member_name)>(json_name, offsetof(object_type, member_name))

// Global
#define set_object_alias(object_type, alias)                                                       \
    namespace JsonDeserialise {                                                                    \
        using alias = ::object_type;                                                               \
    };
#define register_object_member(object_type, json_name, member_name)                                \
    namespace JsonDeserialise {                                                                    \
        constexpr char object_type##_##member_name[] = json_name;                                  \
    };
#define register_object_member_with_extension(object_type, json_name, member_name, json_type,      \ 
                                              convertor, deconvertor)                              \
    namespace JsonDeserialise {                                                                    \
        constexpr char object_type##_##member_name[] = json_name;                                  \
        using object_type##_##member_name##_##json_type##_##Extension_Convertor_ =                 \
            Convertor<decltype(((object_type*)nullptr)->member_name), decltype(convertor),         \
                      decltype(deconvertor), json_type>;                                           \
        using object_type##_##member_name##_##json_type##_##Extension_Base_ =                      \
            Extension<object_type##_##member_name##_##json_type##_##Extension_Convertor_>;         \
        struct object_type##_##member_name##_##json_type##_##Extension_                            \
            : public object_type##_##member_name##_##json_type##_##Extension_Base_ {               \
            using Base = object_type##_##member_name##_##json_type##_##Extension_Base_;            \
            using TargetType = decltype(((object_type*)nullptr)->member_name);                     \
            inline static const object_type##_##member_name##_##json_type##_##Extension_Convertor_ \
                _CONVERTOR_ = { (convertor), (deconvertor) };                                      \
            object_type##_##member_name##_##json_type##_##Extension_(const QString& name,          \
                                                                     TargetType& source,           \
                                                                     bool optional = false)        \
                : Base(_CONVERTOR_, name, source, optional) {}                                     \
        };                                                                                         \
    };                                                                                             \
    template<>                                                                                     \
    struct JsonDeserialise::Customised<decltype(((object_type*)nullptr)->member_name),             \
                                       JsonDeserialise::object_type##_##member_name,               \
                                       (offsetof(object_type, member_name))> {                     \
        using Type = JsonDeserialise::object_type##_##member_name##_##json_type##_##Extension_;    \
    };
#define object_member(object_type, member_name)                                                    \
    JsonDeserialise::ReinforcedInfo<decltype(((object_type*)nullptr)->member_name),                \
                                    JsonDeserialise::object_type##_##member_name,                  \
                                    (offsetof(object_type, member_name))>
#define optional_object_member(object_type, member_name)                                           \
    JsonDeserialise::ReinforcedInfo<decltype(((object_type*)nullptr)->member_name),                \
                                    JsonDeserialise::object_type##_##member_name,                  \
                                    (offsetof(object_type, member_name)), true>
#define declare_object(object_type, ...)                                                           \
    template <>                                                                                    \
    struct JsonDeserialise::Deserialisable<object_type> {                                          \
        using Type = DeserialisableObject<object_type, __VA_ARGS__>;                               \
    };
#define declare_class_with_json_constructor_and_serialiser(object_type)                            \
    template <>                                                                                    \
    struct JsonDeserialise::Deserialisable<object_type> {                                          \
        using Type = SelfDeserialisableObject<object_type>;                                        \
    };
#define register_map_with_key(container_type, key_type, value_type, key_name)                      \
    namespace JsonDeserialise {                                                                    \
        constexpr char map_##container_type##_##key_type##_##value_type##_##json_key[] = key_name; \
        template <>                                                                                \
        struct Deserialisable<container_type<key_type, value_type>> {                              \
            using Type = MapArray<container_type<key_type, value_type>, key_type, value_type,      \
                                  map_##container_type##_##key_type##_##value_type##_##json_key>;  \
        };                                                                                         \
    }
#define declare_object_with_base_class(object_type, base_type, ...)                                \
    template <>                                                                                    \
    struct JsonDeserialise::Deserialisable<object_type> {                                          \
        using Type = DerivedObject<base_type, object_type, __VA_ARGS__>;                           \
    };
#define declare_non_trivial_as(type_name, as)                                                      \
    template <>                                                                                    \
    struct JsonDeserialise::Deserialisable<type_name> {                                            \
        using Type = JsonDeserialise::NonTrivial<type_name, as>;                                   \
    };
#define declare_global_extension(target_type, json_type, convertor, deconvertor)                   \
    namespace JsonDeserialise {                                                                    \
        using target_type##_##json_type##_##GlobalExtension_Base_Convertor_ =                      \
            Convertor<target_type, decltype(convertor), decltype(deconvertor), json_type>;         \
        using target_type##_##json_type##_##GlobalExtension_Base_ =                                \
            Extension<target_type##_##json_type##_##GlobalExtension_Base_Convertor_>;              \
        struct target_type##_##json_type##_##GlobalExtension_                                      \
            : public target_type##_##json_type##_##GlobalExtension_Base_ {                         \
            using Base = target_type##_##json_type##_##GlobalExtension_Base_;                      \
            inline static const target_type##_##json_type##_##GlobalExtension_Base_Convertor_      \
                _CONVERTOR_ = { (convertor), (deconvertor) };                                      \
            target_type##_##json_type##_##GlobalExtension_(target_type& source)                    \
                : Base(_CONVERTOR_, source) {}                                                     \
            target_type##_##json_type##_##GlobalExtension_(const QString& name,                    \
                                                           target_type& source,                    \
                                                           bool optional = false)                  \
                : Base(_CONVERTOR_, name, source, optional) {}                                     \
        };                                                                                         \
        template <>                                                                                \
        struct Deserialisable<target_type> {                                                       \
            using Type = target_type##_##json_type##_##GlobalExtension_;                           \
        };                                                                                         \
    }
#define declare_extension(name, target_type, json_type, convertor, deconvertor)                    \
    namespace JsonDeserialise {                                                                    \
        using name##_##Extension_Base_Convertor_ =                                                 \
            Convertor<target_type, decltype(convertor), decltype(deconvertor), json_type>;         \
        using name##_##Extension_Base_ =                                                           \
            Extension<name##_##Extension_Base_Convertor_>;                                         \
        struct name##_##Extension_ : public name##_##Extension_Base_ {                             \
            using Base = name##_##Extension_Base_;                                                 \
            inline static const name##_##Extension_Base_Convertor_                                 \
                _CONVERTOR_ = { (convertor), (deconvertor) };                                      \
            name##_##Extension_(const QString& name, target_type& source, bool optional = false)   \
                : Base(_CONVERTOR_, name, source, optional) {}                                     \
        };                                                                                         \    
    }
#define object_member_with_extension(object_type, member_name, extension)                          \
    JsonDeserialise::ReinforcedInfo<decltype(((object_type*)nullptr)->member_name),                \
                                    JsonDeserialise::object_type##_##member_name,                  \
                                    (offsetof(object_type, member_name)), false,                   \
                                    JsonDeserialise::extension##_##Extension_>
#define optional_object_member_with_extension(object_type, member_name, extension)                 \
    JsonDeserialise::ReinforcedInfo<decltype(((object_type*)nullptr)->member_name),                \
                                    JsonDeserialise::object_type##_##member_name,                  \
                                    (offsetof(object_type, member_name)), true,                   \
                                    JsonDeserialise::extension##_##Extension_>

#endif // JSON_DESERIALISER_H
