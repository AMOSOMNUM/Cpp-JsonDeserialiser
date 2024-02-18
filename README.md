# JsonDeserialiser

Serialise and Deserialise json from various types and data structures, performing *compile-time reflection* with TMP(*template metaprogramming*) of Modern C++(requires C++17 or later).  
Currently only support Qt Json Library and will support nlohmann soon.

Headers only!  
So You just need to add subdirectory and setup include path in cmake, then include autogen files "json_deserialise.*lib_name*.h", and "json_deserialise.h" for the default one(You should set this by setting *JSON_DESERIALISE_DEFAULT_JSON_LIBRARY* in cmake).  
Depending boost preprocessor library, please also add *boost_preprocessor/include* to your include path if your project does not config Boost.

## Basic Types

|Trait|Type in C++|
|:-|:-|
|Integer|signed、unsigned|
|Real|double|
|Boolean|bool|
|String|char[]、char*、QString、std::string、QByteArray|
|Object|struct/class with more than one fields|
|Json|QJsonValue|
|||

## Advanced

|Trait|Type in C++|
|:-|:-|
|Nullable|T*、std::optional\<T>|
|Ref|T&、const T&|
|Array|std::vector\<T>、std::set\<T>、std::list\<T>、QList\<T>、QSet\<T> e.t.c.|
|LimitedArray|T[N]、std::array\<T, N>|
|Map|std::map\<KeyType, ValueType>|
|Pair|std::pair\<KeyType, ValueType>|
|PairArray|array of std::pair<StringType, ValueType>|
|AsTrivial|Treated same as an Existing Type<br>e.g. enum as integer|
|DerivedObject|Support single inheritance|
|SelfDeserialise|A Class with json constructor and to_json method|
|Extension|An Existing Type to and from Any<br>e.g. enum to string<br>One-direction is also supported|
|Optional|An Optional Field|
|Variant<br>(Preview)|std::variant|
|ProcessControl<br>(Developing)|Actions such as Lock, Self-Examination, Log e.t.c. before or after a desrialisation/serialisation.|
|||

## Simple Usage

Supose you have a json like below:

```json
{
    "A":"TypeAData",
    "B":"TypeBData"
}
```

### 1. For Discrete Data

```c++
QJsonObject json;
TypeA a;
TypeB b;
Field a("A", obj.a);
Field b("B", obj.b);
ObjectDeserialiser deserialiser(a, b);
// From File
deserialiser.deserialise_file(FILENAME);
// From String
deserialiser.deserialise(R"({
    "A":"TypeAData",
    "B":"TypeBData"
})");
// From Json
deserialiser.deserialise(json);
// To File
deserialise.serialise_to_file(FILENAME);
// To String
std::cout << deserialiser.serialise();
// To Json
json = deserialise.serialise_to_json().toObject();
```

### 2. For Class

```c++
struct Simple {
    TypeA a;
    TypeB b;
};

QJsonObject json;
Simple obj;
Field a("A", obj.a);
Field b("B", obj.b);
ObjectDeserialiser deserialiser(a, b);
deserialiser.deserialise(json); // And two other methods.
std::cout << deserialiser.serialise(); // And two other methods.
```

#### Try to Globally Register It

If you want to enable global refelction.  
You should do this in global namespace since our reflection informations are all staticly placed in a namespace called JsonDeserialise.

```c++
// Class Sample Definition Here
declare_object(Sample,
    object_member("A", a),
    object_member("B", b)
);

// Somewhere
Sample s;
QJsonObject json;
Deserialiser holder(s);
holder.from_json(json);
holder.from_file(FILENAME);
json = holder.to_json().toObject();
//Also
json = Serialise(s).toObject();
```

### 3. For Enum

#### as string

An Enum like below:

```c++
enum class Type {
    A, B, C
};
QString enum2str(Type);
Type str2enum(const QString&);
```

A field like below

```json
"Enum":"A"
```

```c++
Type sample;
declare_extension_deserialiser("Enum", sample, holder, str2enum, enum2str);
qDebug() << holder.to_json();
```

##### Globally Register It

```c++
// This declaration enables reflection to all fields of this enum type.
declare_global_extension(Type, str2enum, enum2str);

struct XXX {
    //Other fields...
    Type enum_field;
};

declare_object(XXX, object_member("key", enum_field)
//Other fields...
);
```

#### as integer

An Enum like below:

```c++
enum class Type : int {
    A, B, C
} sample;
declare_as_trivial(Type, int);

qDebug() << Serialise(sample);
```
