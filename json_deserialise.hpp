#ifndef HPP_JSON_DESERIALISER_
#define HPP_JSON_DESERIALISER_

#include <functional>
#include <ios>
#include <optional>
#include <variant>

#include "utilities.hpp"

inline namespace JsonDeserialise {
enum class Trait : unsigned {
    NUL = 0,
    OBJECT = 1,
    ARRAY = 2,
    FIELD = 4,
    OPTIONAL = 8,
};

enum class ArrayInsertWay : uint8_t {
    Unknown = 0,
    Emplace_Back = 1,
    Push_Back = 2,
    Append = 4,
    Insert = 8,
};

template <typename T, typename Element>
struct GetArrayInsertWay {
    template <typename = decltype(std::declval<T>().emplace_back())>
    static constexpr bool is_emplaceback(int*) {
        return true;
    }
    template <typename...>
    static constexpr bool is_emplaceback(...) {
        return false;
    }
    template <typename = decltype(std::declval<T>().push_back(std::declval<Element>()))>
    static constexpr bool is_pushback(int*) {
        return true;
    }
    template <typename...>
    static constexpr bool is_pushback(...) {
        return false;
    }
    template <typename = decltype(std::declval<T>().append(std::declval<Element>()))>
    static constexpr bool is_append(int*) {
        return true;
    }
    template <typename...>
    static constexpr bool is_append(...) {
        return false;
    }
    template <typename = decltype(std::declval<T>().insert(std::declval<Element>()))>
    static constexpr bool is_insert(int*) {
        return true;
    }
    template <typename...>
    static constexpr bool is_insert(...) {
        return false;
    }
    template <typename = decltype(std::declval<T>().reserve(std::declval<std::size_t>()))>
    static constexpr bool is_reservable(int*) {
        return true;
    }
    template <typename...>
    static constexpr bool is_reservable(...) {
        return false;
    }

    static constexpr uint8_t calculate() {
        uint8_t result = uint8_t(ArrayInsertWay::Unknown);

        if (is_emplaceback(nullptr))
            result |= uint8_t(ArrayInsertWay::Emplace_Back);
        if (is_pushback(nullptr))
            result |= uint8_t(ArrayInsertWay::Push_Back);
        if (is_append(nullptr))
            result |= uint8_t(ArrayInsertWay::Append);
        if (is_insert(nullptr))
            result |= uint8_t(ArrayInsertWay::Insert);

        return result;
    }

    static constexpr uint8_t value = calculate();
    static constexpr bool insert_only = value == uint8_t(ArrayInsertWay::Insert);

    inline static Element& push_back(T& container) {
        static_assert(value && !insert_only);
        if constexpr (is_emplaceback(nullptr))
            return container.emplace_back();
        else {
            if constexpr (is_pushback(nullptr))
                container.push_back(Element());
            else if constexpr (is_append(nullptr))
                container.append(Element());
            return container.back();
        }
    }
};

template <typename Lib>
struct Deserialiser {
    template <typename Any>
    using DeserialisableType = typename Lib::template DeserialisableType<Any>;

    template <typename T, const char* json_name, auto member_offset>
    using Customised = typename Lib::template Customised<T, json_name, member_offset>;

    using StringRvalueRef = typename Lib::String&&;
    using StringConstRef = const typename Lib::String&;
    using StringConst = const typename Lib::String;
    using Json = typename Lib::Json;

    template <typename T>
    using StringConvertor = typename Lib::template StringConvertor<T>;

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
        inline static T* convert(T&& value) {
            return new T(std::move(value));
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

    class DeserialisableBase {
    public:
        void* const data_ptr;
        union INFO {
            const Trait flag;
            const void* const const_ptr;

            INFO(Trait flag) : flag(flag) {}
            INFO(const void* ptr) : const_ptr(ptr) {}
        } info;
        StringConst identifier;

    protected:
        DeserialisableBase(void* ptr, Trait _as = Trait::NUL) : data_ptr(ptr), info(_as) {}
        DeserialisableBase(void* ptr, StringConstRef name, bool optional = false)
            : data_ptr(ptr), info(optional ? Trait::OPTIONAL : Trait::FIELD), identifier(name) {}
        DeserialisableBase(void* ptr, StringRvalueRef name, bool optional = false)
            : data_ptr(ptr), info(optional ? Trait::OPTIONAL : Trait::FIELD),
              identifier(std::move(name)) {}
        DeserialisableBase(const void* ptr) : data_ptr(nullptr), info(ptr) {}
        DeserialisableBase(const void* ptr, StringConstRef name)
            : data_ptr(nullptr), info(ptr), identifier(name) {}
        DeserialisableBase(const void* ptr, StringRvalueRef name)
            : data_ptr(nullptr), info(ptr), identifier(std::move(name)) {}
        DeserialisableBase(const DeserialisableBase&) = delete;
        DeserialisableBase& operator=(const DeserialisableBase&) = delete;

        template <typename T>
        inline T& value() noexcept {
            return *reinterpret_cast<T*>(data_ptr);
        }

        template <typename T>
        inline const T& value() const noexcept {
            return *reinterpret_cast<const T*>(data_ptr ? data_ptr : info.const_ptr);
        }
    };

    template <typename T = void, typename... Args>
    inline static constexpr bool isValid() {
        return std::is_same_v<T, void> ||
               std::is_base_of_v<DeserialisableBase, T> && isValid<Args...>();
    }

    template <typename T>
    inline static void deserialise_each(const typename Lib::JsonObject& object,
                                        DeserialisableBase&& each) {
#ifdef _DEBUG
        if (!unsigned(each.info.flag))
            throw std::ios_base::failure("JSON Structure Declaration Invalid!");
#endif
        bool contain = Lib::exists(object, each.identifier);
        if (!(contain || (unsigned(each.info.flag) & unsigned(Trait::OPTIONAL))))
            throw std::ios_base::failure("JSON Structure Incompatible!");
        if (contain)
            static_cast<T&>(each).assign(object[each.identifier]);
    }

    template <typename T>
    inline static void insert_each(typename Lib::JsonObject& object,
                                   const DeserialisableBase& each) {
        object.insert(each.identifier, static_cast<const T&>(each).to_json());
    }

    template <typename T>
    inline static void append_each(typename Lib::JsonArray& array, const DeserialisableBase& each) {
        Lib::append(array, static_cast<const T&>(each).to_json());
    }

    template <typename... Args>
    struct JsonDeserialiser {
        static constexpr int N = sizeof...(Args);
        const std::enable_if_t<N && isValid<Args...>(), DeserialisableBase*> data[N];

        JsonDeserialiser(Args&... args) : data{&args...} {}

        inline void deserialise_file(StringConstRef filepath) {
            deserialise(Lib::parse_file(filepath));
        }
        inline void deserialise(typename Lib::StringView json) {
            deserialise(Lib::parse(json));
        }
        inline void serialise_to_file(StringConstRef filepath, bool compress = false) const {
            Lib::write_json(serialise_to_json(), filepath, compress);
        }
        inline typename Lib::CString serialise(bool compress = false) const {
            Lib::print_json(serialise_to_json(), compress);
        }

        void deserialise(const Json& json) {
            if constexpr (N == 1)
                ((typename PackToType<Args...>::Type*)data[0])->assign(json);
            else {
                auto ptr = data;
                (deserialise_each<Args>(json, **ptr++), ...);
            }
        }

        inline void deserialise(const typename Lib::JsonObject& object) {
            deserialise(static_cast<const Json&>(object));
        }

        inline std::enable_if_t<N == 1> deserialise(const typename Lib::JsonArray& array) {
            deserialise(static_cast<const Json&>(array));
        }

        Json serialise_to_json() const {
            if constexpr (N == 1)
                return ((typename PackToType<Args...>::Type*)data[0])->to_json();
            else {
                typename Lib::JsonObject obj;
                auto ptr = data;
                ((insert_each<Args>(obj, **ptr++)), ...);
                return obj;
            }
        }
    };

    template <typename T>
    struct DeserialisableBaseHelper : public DeserialisableBase {
        DeserialisableBaseHelper(T& source) : DeserialisableBase(&source) {}
        DeserialisableBaseHelper(StringConstRef name, T& source, bool optional = false)
            : DeserialisableBase(&source, name, optional) {}
        DeserialisableBaseHelper(StringRvalueRef name, T& source, bool optional = false)
            : DeserialisableBase(&source, std::move(name), optional) {}
        DeserialisableBaseHelper(const T& source) : DeserialisableBase(&source) {}
        DeserialisableBaseHelper(StringConstRef name, const T& source)
            : DeserialisableBase(&source, name) {}
        DeserialisableBaseHelper(StringRvalueRef name, const T& source)
            : DeserialisableBase(&source, std::move(name)) {}
    };

    struct Boolean : public DeserialisableBaseHelper<bool> {
        using Base = DeserialisableBaseHelper<bool>;
        using Target = bool;

        template <typename... Args>
        Boolean(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (Lib::is_bool(json))
                this->template value<Target>() = Lib::get_bool(json);
            else if (Lib::is_string(json)) {
                auto str = Lib::get_string(json).toLower();
                if (str == "true")
                    this->template value<Target>() = true;
                else if (str == "false")
                    this->template value<Target>() = false;
                else if (str == "1")
                    this->template value<Target>() = true;
                else if (str == "0")
                    this->template value<Target>() = false;
                else if (str.isEmpty())
                    this->template value<Target>() = false;
                else
                    throw std::ios_base::failure("Type Unmatch!");
            } else if (Lib::is_null(json))
                this->template value<Target>() = false;
            else if (Lib::is_number(json)) {
                int val = Lib::get_int(json);
                if (val & -2)
                    throw std::ios_base::failure("Type Unmatch!");
                this->template value<Target>() = val;
            } else
                throw std::ios_base::failure("Type Unmatch!");
        }
        Json to_json() const {
            return this->template value<Target>();
        }
    };

    template <bool sign, size_t size>
    struct Integer;

    template <>
    struct Integer<true, sizeof(int)> : public DeserialisableBaseHelper<int> {
        using Base = DeserialisableBaseHelper<int>;
        using Target = int;

        template <typename... Args>
        Integer(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (Lib::is_number(json))
                this->template value<Target>() = Lib::get_int(json);
            else if (Lib::is_string(json))
                this->template value<Target>() = Lib::str2int(Lib::get_string(json));
            else if (Lib::is_null(json))
                this->template value<Target>() = 0;
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        Json to_json() const {
            return this->template value<Target>();
        }
    };

    template <>
    struct Integer<false, sizeof(unsigned)> : public DeserialisableBaseHelper<unsigned> {
        using Base = DeserialisableBaseHelper<unsigned>;
        using Target = unsigned;

        template <typename... Args>
        Integer(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (Lib::is_number(json))
                this->template value<Target>() = Lib::get_uint(json);
            else if (Lib::is_string(json))
                this->template value<Target>() = Lib::str2uint(Lib::get_string(json));
            else if (Lib::is_null(json))
                this->template value<Target>() = 0;
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        Json to_json() const {
            return Lib::uint2json(this->template value<Target>());
        }
    };

    template <typename T>
    struct Real;

    template <>
    struct Real<double> : public DeserialisableBaseHelper<double> {
        using Base = DeserialisableBaseHelper<double>;
        using Target = double;

        template <typename... Args>
        Real(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (Lib::is_number(json))
                this->template value<Target>() = Lib::get_double(json);
            else if (Lib::is_string(json))
                this->template value<Target>() = Lib::str2double(Lib::get_string(json));
            else
                throw std::ios_base::failure("Type Unmatch!");
        }
        Json to_json() const {
            return this->template value<Target>();
        }
    };

    template <typename T>
    struct String : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        String(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (!Lib::is_string(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            this->template value<Target>() =
                StringConvertor<Target>::convert(Lib::get_string(json));
        }
        Json to_json() const {
            return StringConvertor<Target>::deconvert(this->template value<Target>());
        }
    };

    template <size_t length>
    struct String<char[length]> : public DeserialisableBaseHelper<char[length]> {
        using Base = DeserialisableBaseHelper<char[length]>;
        using Target = char[length];

        template <typename... Args>
        String(Args&&... args) : Base(std::forward<Args>(args)...) {}

        inline char* value() const noexcept {
            return reinterpret_cast<char*>(DeserialisableBase::data_ptr);
        }
        inline const char* const_value() const noexcept {
            return reinterpret_cast<const char*>(DeserialisableBase::data_ptr
                                                     ? DeserialisableBase::data_ptr
                                                     : DeserialisableBase::info.const_ptr);
        }

        inline void assign(const Json& json) {
            if (!Lib::is_string(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            Lib::template char_array_write<length>(value(), Lib::get_string(json));
        }
        Json to_json() const {
            return StringConvertor<const char*>::deconvert(const_value());
        }
    };

    template <typename T, typename StringType>
    struct NullableString : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        NullableString(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (!Lib::is_string(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            if (!Lib::is_null(json))
                this->template value<Target>() = NullableHandler<StringType, T>::convert(
                    StringConvertor<StringType>::convert(Lib::get_string(json)));
        }
        Json to_json() const {
            if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<StringType>>, char>)
                return this->template value<Target>() ? this->template value<Target>() : Json();
            else
                return this->template value<Target>()
                           ? StringConvertor<StringType>::deconvert(*this->template value<Target>())
                           : Json();
        }
    };

    template <typename T, typename StringType>
    struct StringArray : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        StringArray(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            static_assert(GetArrayInsertWay<T, StringType>::value);
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            const auto& array = Lib::get_array(json);
            this->template value<Target>().clear();
            if constexpr (GetArrayInsertWay<T, StringType>::is_reservable())
                this->template value<Target>().reserve(array.size());
            for (const auto& i : array) {
                if (!Lib::is_string(i) && !Lib::is_null(i))
                    throw std::ios_base::failure("Type Unmatch!");
                if constexpr (GetArrayInsertWay<T, StringType>::is_pushback(nullptr))
                    this->template value<Target>().push_back(
                        StringConvertor<StringType>::convert(Lib::get_string(i)));
                else if constexpr (GetArrayInsertWay<T, StringType>::is_append(nullptr))
                    this->template value<Target>().append(
                        StringConvertor<StringType>::convert(Lib::get_string(i)));
                else
                    this->template value<Target>().insert(
                        StringConvertor<StringType>::convert(Lib::get_string(i)));
            }
        }
        Json to_json() const {
            typename Lib::JsonArray array;
            for (const auto& i : this->template value<Target>())
                Lib::append(array, StringConvertor<StringType>::deconvert(i));
            return array;
        }
    };

    template <typename T, typename NullableStringType, typename StringType>
    struct NullableStringArray : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        NullableStringArray(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            const auto& array = Lib::get_array(json);
            this->template value<Target>().clear();
            if constexpr (GetArrayInsertWay<T, StringType>::is_reservable())
                this->template value<Target>().reserve(array.size());
            for (const auto& i : array)
                if (Lib::is_string(i)) {
                    if constexpr (GetArrayInsertWay<T, StringType>::is_pushback(nullptr))
                        this->template value<Target>().push_back(
                            NullableHandler<NullableStringType, StringType>::convert(
                                StringConvertor<StringType>::convert(Lib::get_string(i))));
                    else if constexpr (GetArrayInsertWay<T, StringType>::is_append(nullptr))
                        this->template value<Target>().append(
                            NullableHandler<NullableStringType, StringType>::convert(
                                StringConvertor<StringType>::convert(Lib::get_string(i))));
                    else
                        this->template value<Target>().insert(
                            NullableHandler<NullableStringType, StringType>::convert(
                                StringConvertor<StringType>::convert(Lib::get_string(i))));
                } else if (Lib::is_null(i)) {
                    if constexpr (GetArrayInsertWay<T, StringType>::is_pushback(nullptr))
                        this->template value<Target>().push_back(
                            NullableHandler<NullableStringType, StringType>::make_empty());
                    else if constexpr (GetArrayInsertWay<T, StringType>::is_append(nullptr))
                        this->template value<Target>().append(
                            NullableHandler<NullableStringType, StringType>::make_empty());
                    else
                        this->template value<Target>().insert(
                            NullableHandler<NullableStringType, StringType>::make_empty());
                } else
                    throw std::ios_base::failure("Type Unmatch!");
        }
        Json to_json() const {
            typename Lib::JsonArray array;
            for (const auto& i : this->template value<Target>())
                if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<StringType>>,
                                             char>)
                    Lib::append(array, i ? i : Json());
                else
                    Lib::append(array, i ? StringConvertor<StringType>::deconvert(*i) : Json());
            return array;
        }
    };

    template <typename T, typename StringType, std::size_t N>
    struct LimitedStringArray : public StringArray<T, StringType> {
        using Base = StringArray<T, StringType>;
        using Target = T;

        template <typename... Args>
        LimitedStringArray(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            int count = 0;
            for (const auto& i : Lib::get_array(json)) {
                if (!Lib::is_string(i) && !Lib::is_null(i))
                    throw std::ios_base::failure("Type Unmatch!");
                if (count == N)
                    throw std::ios_base::failure("Array Out of Range!");
                this->template value<Target>()[count++] =
                    StringConvertor<StringType>::convert(Lib::get_string(i));
            }
        }
    };

    template <typename T, typename NullableStringType, typename StringType, std::size_t N>
    struct LimitedNullableStringArray
        : public NullableStringArray<T, NullableStringType, StringType> {
        using Base = NullableStringArray<T, NullableStringType, StringType>;
        using Target = T;

        template <typename... Args>
        LimitedNullableStringArray(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            int count = 0;
            for (const auto& i : Lib::get_array(json)) {
                if (count == N)
                    throw std::ios_base::failure("Array Out of Range!");
                if (!Lib::is_null(i))
                    this->template value<Target>()[count++] =
                        NullableHandler<NullableStringType, StringType>::convert(
                            StringConvertor<StringType>::convert(Lib::get_string(i)));
                else
                    this->template value<Target>()[count++] =
                        NullableHandler<NullableStringType, StringType>::make_empty();
            }
        }
    };

    template <typename T, auto member_offset>
    struct ObjectArrayInfo : public Lib::String {
        using Prototype = DeserialisableType<decltype(((T*)nullptr)->member_offset)>;
    };

    template <typename T, auto... Members>
    struct ObjectArray : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        ObjectArray(Args&&... args) : Base(std::forward<Args>(args)...) {}

        using ObjectType = typename Lib::template Deserialisable<Target>::TypeInArray;
        StringConst identifiers[sizeof...(Members)];

        void assign(const Json& json) {
            static_assert(GetArrayInsertWay<T, ObjectType>::value);
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            const auto& array = Lib::get_array(json);
            this->template value<Target>().clear();
            if constexpr (GetArrayInsertWay<T, ObjectType>::is_reservable())
                this->template value<Target>().reserve(array.size());
            for (const auto& i : array) {
                if (!Lib::is_object(i))
                    throw std::ios_base::failure("Type Unmatch!");
                StringConst* ptr = identifiers;
                if constexpr (!GetArrayInsertWay<T, ObjectType>::insert_only) {
                    ObjectType& obj =
                        GetArrayInsertWay<T, ObjectType>::push_back(this->template value<Target>());
                    (deserialise_each<typename ObjectArrayInfo<ObjectType, Members>::Prototype>(
                         Lib::get_object(i),
                         typename ObjectArrayInfo<ObjectType, Members>::Prototype(*ptr++,
                                                                                  obj.*Members)),
                     ...);
                } else {
                    ObjectType obj;
                    (deserialise_each<typename ObjectArrayInfo<ObjectType, Members>::Prototype>(
                         Lib::get_object(i),
                         typename ObjectArrayInfo<ObjectType, Members>::Prototype(*ptr++,
                                                                                  obj.*Members)),
                     ...);
                    this->template value<Target>().insert(std::move(obj));
                }
            }
        }
        Json to_json() const {
            typename Lib::JsonArray array;
            for (const auto& i : this->template value<Target>()) {
                StringConst* ptr = identifiers;
                (append_each<typename ObjectArrayInfo<ObjectType, Members>::Prototype>(
                     array,
                     typename ObjectArrayInfo<ObjectType, Members>::Prototype(*ptr++, i.*Members)),
                 ...);
            }
            return array;
        }
    };

    template <typename T, typename TypeInArray>
    struct Array : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        Array(Args&&... args) : Base(std::forward<Args>(args)...) {}

        using Prototype = DeserialisableType<TypeInArray>;

        void assign(const Json& json) {
            static_assert(GetArrayInsertWay<T, TypeInArray>::value);
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            const auto& array = Lib::get_array(json);
            this->template value<Target>().clear();
            if constexpr (GetArrayInsertWay<T, TypeInArray>::is_reservable())
                this->template value<Target>().reserve(array.size());
            for (const auto& i : array) {
                if constexpr (!GetArrayInsertWay<T, TypeInArray>::insert_only) {
                    Prototype deserialiser(GetArrayInsertWay<T, TypeInArray>::push_back(
                        this->template value<Target>()));
                    deserialiser.assign(i);
                } else {
                    TypeInArray tmp;
                    Prototype deserialiser(tmp);
                    deserialiser.assign(i);
                    this->template value<Target>().insert(std::move(tmp));
                }
            }
        }
        Json to_json() const {
            typename Lib::JsonArray array;
            for (const auto& i : this->template value<Target>()) {
                Prototype serialiser(i);
                Lib::append(array, serialiser.to_json());
            }
            return array;
        }
    };

    template <typename T, typename KeyType, typename ValueType>
    struct Array<T, std::pair<KeyType, ValueType>> : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;
        using TypeInArray = std::pair<KeyType, ValueType>;

        template <typename... Args>
        Array(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            static_assert(StringConvertor<KeyType>::value &&
                          GetArrayInsertWay<T, TypeInArray>::value);
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            const auto& obj = Lib::get_object(json);
            this->template value<Target>().clear();
            for (const auto& [_key, _value] : obj) {
                KeyType key;
                ValueType value_;
                DeserialisableType<KeyType> key_deserialiser(key);
                DeserialisableType<ValueType> value_deserialiser(value_);
                key_deserialiser.assign(_key);
                value_deserialiser.assign(_value);
                if constexpr (GetArrayInsertWay<T, TypeInArray>::is_emplace_back())
                    this->template value<Target>().emplace_back(key, value_);
                else
                    this->template value<Target>().emplace(key, value_);
            }
        }
        Json to_json() const {
            typename Lib::JsonObject obj;
            for (const auto& [key, value] : this->template value<Target>()) {
                DeserialisableType<KeyType> key_deserialiser(key);
                DeserialisableType<ValueType> value_deserialiser(value);
                obj.insert(Lib::get_string(key_deserialiser.to_json()),
                           value_deserialiser.to_json());
            }
            return obj;
        }
    };

    template <typename T, typename TypeInArray, std::size_t N>
    struct LimitedArray : public Array<T, TypeInArray> {
        using Base = Array<T, TypeInArray>;
        using Target = T;
        using Prototype = DeserialisableType<TypeInArray>;

        template <typename... Args>
        LimitedArray(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            int count = 0;
            for (const auto& i : Lib::get_array(json)) {
                if (count == N)
                    throw std::ios_base::failure("Array Out of Range!");
                Prototype deserialiser(this->template value<Target>()[count++]);
                deserialiser.assign(i);
            }
        }
    };

    template <typename T, typename TypeInNullable>
    struct Nullable : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        Nullable(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (Lib::is_null(json)) {
                this->template value<Target>() =
                    NullableHandler<TypeInNullable, Target>::make_empty();
                return;
            }
            TypeInNullable tmp;
            DeserialisableType<TypeInNullable> deserialiser(tmp);
            deserialiser.assign(json);
            this->template value<Target>() =
                NullableHandler<TypeInNullable, Target>::convert(std::move(tmp));
        }
        Json to_json() const {
            return this->template value<Target>()
                       ? DeserialisableType<TypeInNullable>(*this->template value<Target>())
                             .to_json()
                       : Json();
        }
    };

    template <typename T, typename As>
    struct AsTrivial : public DeserialisableType<As> {
        using Base = DeserialisableType<As>;
        using Target = T;

        AsTrivial(Target& source) : Base(reinterpret_cast<As&>(source)) {}
        AsTrivial(StringConstRef name, Target& source, bool optional = false)
            : Base(name, reinterpret_cast<As&>(source), optional) {}
        AsTrivial(StringRvalueRef name, Target& source, bool optional = false)
            : Base(std::move(name), reinterpret_cast<As&>(source), optional) {}
        AsTrivial(const Target& source) : Base(reinterpret_cast<const As&>(source)) {}
        AsTrivial(StringConstRef name, const Target& source)
            : Base(name, reinterpret_cast<const As&>(source)) {}
        AsTrivial(StringRvalueRef name, const Target& source)
            : Base(std::move(name), reinterpret_cast<const As&>(source)) {}
    };

    template <typename T, const char* json_name, size_t json_name_length, auto member_offset,
              bool optional_member = false,
              typename Custom = typename Customised<T, json_name, member_offset>::Type>
    struct ReinforcedInfo {
        using Type = T;
        using Prototype = Custom;
        static constexpr const char* name = json_name;
        static constexpr size_t name_length = json_name_length;
        static constexpr auto member_ptr = member_offset;
        static constexpr bool optional = optional_member;
    };

    template <typename T, typename... MemberInfo>
    struct Object : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        Object(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (!Lib::is_object(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            (deserialise_each<typename MemberInfo::Prototype>(
                 Lib::get_object(json),
                 typename MemberInfo::Prototype(MemberInfo::name, this->template value<Target>().*
                                                                      MemberInfo::member_ptr)),
             ...);
        }
        Json to_json() const {
            typename Lib::JsonObject obj;
            (insert_each<const typename MemberInfo::Prototype>(
                 obj,
                 typename MemberInfo::Prototype(MemberInfo::name, this->template value<Target>().*
                                                                      MemberInfo::member_ptr)),
             ...);
            return obj;
        }
    };

    template <class BaseType, class Derived, typename... MemberInfo>
    struct DerivedObject : public DeserialisableType<BaseType> {
        using Base = DeserialisableType<BaseType>;
        using Target = Derived;

        template <typename... Args>
        DerivedObject(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            static_assert(std::is_base_of_v<BaseType, Derived>);
            if (!Lib::is_object(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            Base::assign(json);
            (deserialise_each<typename MemberInfo::Prototype>(
                 Lib::get_object(json),
                 typename MemberInfo::Prototype(MemberInfo::name, this->template value<Target>().*
                                                                      MemberInfo::member_ptr)),
             ...);
        }

        Json to_json() const {
            auto obj = Lib::get_object(Base::to_json());
            (insert_each<const typename MemberInfo::Prototype>(
                 obj,
                 typename MemberInfo::Prototype(MemberInfo::name, this->template value<Target>().*
                                                                      MemberInfo::member_ptr)),
             ...);
            return obj;
        }
    };

    template <typename T>
    struct SelfDeserialisableObject : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        SelfDeserialisableObject(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            if (!Lib::is_object(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            this->template value<Target>() = std::move(Target(Lib::get_object(json)));
        }
        Json to_json() const {
            return this->template value<Target>().to_json();
        }
    };

    template <typename T, typename KeyType, typename ValueType>
    struct PairArrayMap : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename String1, typename String2, typename... Args>
        PairArrayMap(String1&& key_name, String2&& value_name, Args&&... args)
            : Base(std::forward<Args>(args)...),
              key{std::forward<String1>(key_name), std::forward<String2>(value_name)} {}
        template <typename String1, typename String2>
        PairArrayMap(T& source, String1&& key_name, String2&& value_name)
            : Base(source),
              key{std::forward<String1>(key_name), std::forward<String2>(value_name)} {}
        template <typename String, typename String1, typename String2>
        PairArrayMap(String&& name, T& source, String1&& key_name, String2&& value_name,
                     bool optional = false)
            : Base(std::forward<String>(name), source, optional),
              key{std::forward<String1>(key_name), std::forward<String2>(value_name)} {}

        StringConst key[2];

        void assign(const Json& json) {
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : Lib::get_array(json)) {
                const auto& obj = Lib::get_object(i);
                if (!Lib::exists(obj, key[0]) || !Lib::exists(obj, key[1]))
                    throw std::ios_base::failure("Type Unmatch!");
                KeyType key_field;
                DeserialisableType<KeyType> key_deserialiser(key_field);
                key_deserialiser.assign(obj[key[0]]);
                ValueType value_obj;
                DeserialisableType<ValueType> value_deserialiser(value_obj);
                value_deserialiser.assign(obj[key[1]]);
                this->template value<Target>()[key_field] = std::move(value_obj);
            }
        }
        Json to_json() const {
            typename Lib::JsonArray array;
            for (const auto& [key_, value_] : this->template value<Target>()) {
                typename Lib::JsonObject obj;
                DeserialisableType<KeyType> key_deserialiser(key_);
                obj.insert(key[0], key_deserialiser.to_json());
                DeserialisableType<ValueType> value_deserialiser(value_);
                obj.insert(key[1], value_deserialiser.to_json());
                Lib::append(array, obj);
            }
            return array;
        }
    };

    template <typename T, typename KeyType, typename ValueType>
    struct ObjectArrayMap : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename String, typename... Args>
        ObjectArrayMap(String&& key_name, Args&&... args)
            : Base(std::forward<Args>(args)...), key{std::forward<String>(key_name)} {}
        template <typename String>
        ObjectArrayMap(T& source, String&& key_name)
            : Base(source), key{std::forward<String>(key_name)} {}
        template <typename String1, typename String2>
        ObjectArrayMap(String1&& name, T& source, String2&& key_name, bool optional = false)
            : Base(std::forward<String1>(name), source, optional),
              key{std::forward<String2>(key_name)} {}

        StringConst key;

        void assign(const Json& json) {
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& i : Lib::get_array(json)) {
                const auto& obj = Lib::get_object(i);
                if (!Lib::exists(obj, key))
                    throw std::ios_base::failure("Type Unmatch!");
                KeyType key_field;
                DeserialisableType<KeyType> key_deserialiser(key_field);
                key_deserialiser.assign(obj[key]);
                ValueType value_obj;
                DeserialisableType<ValueType> value_deserialiser(value_obj);
                value_deserialiser.assign(obj);
                this->template value<Target>()[key_field] = std::move(value_obj);
            }
        }
        Json to_json() const {
            typename Lib::JsonArray array;
            for (const auto& [key_, value_] : this->template value<Target>()) {
                DeserialisableType<ValueType> value_deserialiser(value_);
                auto obj = Lib::get_object(value_deserialiser.to_json());
                DeserialisableType<KeyType> key_deserialiser(key_);
                obj.insert(key, key_deserialiser.to_json());
                Lib::append(array, std::move(obj));
            }
            return array;
        }
    };

    template <typename T, typename KeyType, typename ValueType>
    struct StringMap : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename... Args>
        StringMap(Args&&... args) : Base(std::forward<Args>(args)...) {}

        void assign(const Json& json) {
            static_assert(StringConvertor<KeyType>::value);
            if (!Lib::is_array(json) && !Lib::is_null(json))
                throw std::ios_base::failure("Type Unmatch!");
            for (const auto& [_key, _value] : Lib::get_object(json)) {
                KeyType key;
                ValueType value_;
                DeserialisableType<KeyType> key_deserialiser(key);
                DeserialisableType<ValueType> value_deserialiser(value_);
                key_deserialiser.assign(_key);
                value_deserialiser.assign(_value);
                this->template value<Target>()[key] = std::move(value_);
            }
        }
        Json to_json() const {
            typename Lib::JsonObject obj;
            for (const auto& [key, value] : this->template value<Target>()) {
                DeserialisableType<KeyType> key_deserialiser(key);
                DeserialisableType<ValueType> value_deserialiser(value);
                obj.insert(Lib::get_string(key_deserialiser.to_json()),
                           value_deserialiser.to_json());
            }
            return obj;
        }
    };

    template <typename T, typename Type1, typename Type2>
    struct Pair : public DeserialisableBaseHelper<T> {
        using Base = DeserialisableBaseHelper<T>;
        using Target = T;

        template <typename String1, typename String2, typename... Args>
        Pair(String1&& key_name, String2&& value_name, Args&&... args)
            : Base(std::forward<Args>(args)...),
              key{std::forward<String1>(key_name), std::forward<String2>(value_name)} {}

        StringConst key[2];

        void assign(const Json& json) {
            if (!Lib::is_object(json))
                throw std::ios_base::failure("Type Unmatch!");
            Type1& element1 = this->template value<Target>().first;
            Type2& element2 = this->template value<Target>().second;
            DeserialisableType<Type1> deserialiser1(element1);
            DeserialisableType<Type2> deserialiser2(element2);
            deserialiser1.assign(Lib::get_object(json)[key[0]]);
            deserialiser2.assign(Lib::get_object(json)[key[1]]);
        }
        Json to_json() const {
            typename Lib::JsonObject pair;
            const DeserialisableType<Type1> serialiser1(this->template value<Target>().first);
            const DeserialisableType<Type2> serialiser2(this->template value<Target>().second);
            pair.insert(key[0], serialiser1.to_json());
            pair.insert(key[1], serialiser2.to_json());
            return pair;
        }
    };

    template <typename Des, typename ConvertFunctor,
              typename BasicType = typename ArgTypeDeduction<decltype(std::function(
                  std::declval<ConvertFunctor>()))>::Type>
    struct DeserialiseOnlyConvertor : decltype(std::function(std::declval<ConvertFunctor>())) {
        using Type = BasicType;
        using Target = std::decay_t<Des>;
        using Source = std::decay_t<BasicType>;
        using FunctionType = decltype(std::function(std::declval<ConvertFunctor>()));

        template <typename T>
        DeserialiseOnlyConvertor(T&& convertor) : FunctionType(convertor) {}
    };

    template <typename Convertor>
    struct DeserialiseOnlyExtension : public DeserialisableBase {
        using Target = typename Convertor::Target;
        using Source = typename Convertor::Source;
        using Prototype = DeserialisableType<typename Convertor::Type>;

        template <typename T>
        DeserialiseOnlyExtension(T&& convertor, Target& source)
            : DeserialisableBase(&source), convertor(std::forward<T>(convertor)) {}
        template <typename T, typename String>
        DeserialiseOnlyExtension(T&& convertor, String&& name, Target& source,
                                 bool optional = false)
            : DeserialisableBase(&source, std::forward<String>(name), optional),
              convertor(std::forward<T>(convertor)) {}

        const std::decay_t<Convertor> convertor;

        void assign(const Json& json) {
            Source tmp;
            Prototype(tmp).assign(json);
            this->template value<Target>() = convertor(tmp);
        }
    };
    template <typename Des, typename ConvertFunctor>
    DeserialiseOnlyExtension(ConvertFunctor&&, Des&)
        -> DeserialiseOnlyExtension<DeserialiseOnlyConvertor<Des, ConvertFunctor>>;
    template <typename Des, typename ConvertFunctor, typename String>
    DeserialiseOnlyExtension(ConvertFunctor&&, String&&, Des&, bool)
        -> DeserialiseOnlyExtension<DeserialiseOnlyConvertor<Des, ConvertFunctor>>;

    template <typename Des, typename ConvertFunctor,
              typename BasicType = decltype(std::declval<ConvertFunctor>()(std::declval<Des>()))>
    struct SerialiseOnlyConvertor : decltype(std::function(std::declval<ConvertFunctor>())) {
        using Type = BasicType;
        using Target = std::decay_t<Des>;
        using Source = std::decay_t<BasicType>;
        using FunctionType = decltype(std::function(std::declval<ConvertFunctor>()));

        template <typename T>
        SerialiseOnlyConvertor(T&& convertor) : FunctionType(convertor) {}
    };

    template <typename Convertor>
    struct SerialiseOnlyExtension : public DeserialisableBase {
        using Target = typename Convertor::Target;
        using Source = typename Convertor::Source;
        using Prototype = DeserialisableType<typename Convertor::Type>;

        template <typename T>
        SerialiseOnlyExtension(T&& convertor, const Target& source)
            : DeserialisableBase(&source), convertor(std::forward<T>(convertor)) {}
        template <typename T, typename String>
        SerialiseOnlyExtension(T&& convertor, String&& name, const Target& source)
            : DeserialisableBase(&source, std::forward<String>(name)),
              convertor(std::forward<T>(convertor)) {}

        const std::decay_t<Convertor> convertor;

        Json to_json() const {
            auto tmp = convertor(this->template value<Target>());
            return Prototype(tmp).to_json();
        }
    };
    template <typename Des, typename ConvertFunctor>
    SerialiseOnlyExtension(ConvertFunctor&&, const Des&)
        -> SerialiseOnlyExtension<SerialiseOnlyConvertor<Des, ConvertFunctor>>;
    template <typename Des, typename ConvertFunctor, typename String>
    SerialiseOnlyExtension(ConvertFunctor&&, String&&, const Des&)
        -> SerialiseOnlyExtension<SerialiseOnlyConvertor<Des, ConvertFunctor>>;

    template <typename Des, typename ConvertFunctor = Des(const Json&),
              typename DeconvertFunctor = Json(const Des&),
              typename BasicType = decltype(std::declval<DeconvertFunctor>()(std::declval<Des>()))>
    struct Convertor {
        using Type = BasicType;
        using Target = std::decay_t<Des>;
        using Source = std::decay_t<BasicType>;
        DeserialiseOnlyConvertor<Des, ConvertFunctor, BasicType> convertor;
        SerialiseOnlyConvertor<Des, DeconvertFunctor, BasicType> deconvertor;

        template <typename U, typename V>
        Convertor(U&& convertor, V&& deconvertor)
            : convertor(std::forward<U>(convertor)), deconvertor(std::forward<V>(deconvertor)) {}
    };

    template <typename Convertor>
    class Extension : public DeserialisableBase {
    public:
        using Type = DeserialisableType<typename Convertor::Type>;
        using Target = typename Convertor::Target;
        using Source = typename Convertor::Source;

    protected:
        std::decay_t<Convertor> convertor;

    public:
        template <typename U, typename V>
        Extension(U&& convertor, V&& deconvertor, Target& source)
            : DeserialisableBase(&source),
              convertor(std::forward<U>(convertor), std::forward<V>(deconvertor)) {}
        template <typename U, typename V, typename String>
        Extension(U&& convertor, V&& deconvertor, String&& name, Target& source,
                  bool optional = false)
            : DeserialisableBase(&source, std::forward<String>(name), optional),
              convertor(std::forward<U>(convertor), std::forward<V>(deconvertor)) {}

        void assign(const Json& json) {
            Source tmp;
            Type(tmp).assign(json);
            this->template value<Target>() = convertor.convertor(tmp);
        }
        Json to_json() const {
            auto tmp = convertor.deconvertor(this->template value<Target>());
            return Type(tmp).to_json();
        }
    };

    template <typename Des, typename ConvertFunctor, typename DeconvertFunctor>
    Extension(ConvertFunctor&&, DeconvertFunctor&&, Des&)
        -> Extension<Convertor<Des, ConvertFunctor, DeconvertFunctor>>;
    template <typename Des, typename ConvertFunctor, typename DeconvertFunctor, typename String>
    Extension(ConvertFunctor&&, DeconvertFunctor&&, String&&, Des&, bool)
        -> Extension<Convertor<Des, ConvertFunctor, DeconvertFunctor>>;

    template <typename Needed, typename Given,
              typename Result = typename TypeTupleMap<Needed, Lib::template Deserialisable>::Type>
    struct VariantTupleGenerator;

    template <typename Needed, typename Current, typename... Left, typename Last>
    struct VariantTupleGenerator<Needed, TypeTuple<Current, Left...>, Last> {
        static constexpr int index = FindType<Needed, typename Current::Target>::index;
        using Next = VariantTupleGenerator<Needed, TypeTuple<Left...>,
                                           typename TypeTupleAlter<index, Last, Current>::Type>;
        using Type = typename Next::Type;
    };

    template <typename Needed, typename Last>
    struct VariantTupleGenerator<Needed, TypeTuple<void>, Last> {
        using Type = Last;
    };

    template <typename Variant>
    struct GetTuple;

    template <typename... Types>
    struct GetTuple<std::variant<Types...>> {
        using Type = TypeTuple<Types...>;
    };

    template <typename T, typename Inc, typename Convertors>
    struct VariantImpl;

    template <typename T, typename... Given_Convertors>
    using Variant =
        VariantImpl<T, typename ConstexprIncArray<GetTuple<T>::Type::length>::Type,
                    typename VariantTupleGenerator<typename GetTuple<T>::Type,
                                                   TypeTuple<Given_Convertors...>>::Type>;

    template <typename T, int... pack, typename PrototypeTuple>
    struct VariantImpl<T, ConstexprArrayPack<pack...>, PrototypeTuple>
        : public DeserialisableBaseHelper<T> {
        using Target = T;
        using Base = DeserialisableBaseHelper<T>;
        const std::function<int(const Json&)> deductor;

        template <typename Deductor, typename... Args>
        VariantImpl(Deductor&& deduction, Args&&... args)
            : Base(std::forward<Args>(args)...), deductor(std::forward<Deductor>(deduction)) {}

        template <int N>
        inline void assign_if_eq(int index, const Json& json) {
            if (N == index) {
                this->template value<Target>().template emplace<N>();
                typename GetType<N, PrototypeTuple>::Type(
                    std::get<N>(this->template value<Target>()))
                    .assign(json);
            }
        }
        template <int N>
        inline void serialise_if_eq(int index, Json& json) {
            if (N == index)
                json = typename GetType<N, PrototypeTuple>::Type(
                           std::get<N>(this->template value<Target>()))
                           .to_json();
        }

        void assign(const Json& json) {
            int index = deductor(json);
            if (index == -1)
                throw std::ios_base::failure("Type Unmatch!");
            (assign_if_eq<pack>(index, json), ...);
        }
        Json to_json() const {
            Json result;
            int index = this->template value<Target>().index();
            (serialise_if_eq<pack>(index, result), ...);
        }
    };

    template <typename T, typename Prototype = DeserialisableType<T>>
    struct EndoExtension : public Prototype {
        const std::function<void(T&)> functor;

        template <typename Functor>
        EndoExtension(Functor&& f, T& source)
            : Prototype(source), functor(std::forward<Functor>(f)) {}
        template <typename Functor, typename String>
        EndoExtension(Functor&& f, String&& name, T& source, bool optional = false)
            : Prototype(std::forward<String>(name), source, optional),
              functor(std::forward<Functor>(f)) {}

        inline void assign(const Json& json) {
            Prototype::assign(json);
            functor(this->template value<T>());
        }
    };

    struct JSONWrap : public DeserialisableBaseHelper<Json> {
        using Base = DeserialisableBaseHelper<Json>;
        using Target = Json;

        template <typename... Args>
        JSONWrap(Args&&... args) : Base(std::forward<Args>(args)...) {}

        inline void assign(const Json& json) {
            this->template value<Target>() = json;
        }
        inline Json to_json() const {
            return this->template value<Target>();
        }
    };

    enum class MapStyle {
        STRING_MAP,
        OBJECT_ARRAY,
        PAIR_ARRAY,
    };
    template <typename Container, typename Key, typename Value>
    struct MapTypeInfo {
        using MapType = Container;
        using KeyType = Key;
        using ValueType = Value;
        using _STRING_MAP_ = StringMap<Container, KeyType, ValueType>;
        using _OBJECT_ARRAY_ = ObjectArrayMap<Container, KeyType, ValueType>;
        using _PAIR_ARRAY_ = PairArrayMap<Container, KeyType, ValueType>;
        using Type = _STRING_MAP_;

        using EnumType = MapStyle;
        template <EnumType style = EnumType::STRING_MAP>
        struct Style;

        template <>
        struct Style<MapStyle::STRING_MAP> {
            using Type = _STRING_MAP_;
            static constexpr int argc = 0;
        };

        template <>
        struct Style<MapStyle::OBJECT_ARRAY> {
            using Type = _OBJECT_ARRAY_;
            static constexpr int argc = 1;
        };

        template <>
        struct Style<MapStyle::PAIR_ARRAY> {
            using Type = _PAIR_ARRAY_;
            static constexpr int argc = 2;
        };
    };
};

} // namespace JsonDeserialise

#endif // HPP_JSON_DESERIALISER_
