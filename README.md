# JsonDeserialiser

Serialise and Deserialise json from various types and data structures, performing *compile-time reflection* with TMP(*template metaprogramming*) of Modern C++(requires C++17 or later).  
Currently only support Qt Json Library and will support nlohmann soon.

Headers only!  
You just need to set includepath in cmake and include autogen file "json_deserialise.h".

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
|Ref|T&、const T&、T&&|
|Array|std::vector\<T>、std::set\<T>、std::list\<T>、QList\<T>、QSet\<T> e.t.c.|
|LimitedArray|T[N]、std::array\<T, N>|
|Map|std::map\<KeyType, ValueType>|
|Pair|std::pair\<KeyType, ValueType>|
|AsTrivial|Treated same as an Existing Type<br>e.g. enum as integer|
|DerivedObject|Support single inheritance|
|SelfDeserialise|A Class with json constructor and to_json method|
|Extension|An Existing Type to and from Any<br>e.g. enum to string<br>One-direction is also supported|
|EndoFunctor|A special Extension used for further actions right after deserialisation|
|Optional|An Optional Field|
|VerientObject<br>(Experimental)|[finished]std::variant<br>[TODO]support downcast|
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
declare_deserialiser("A", a, a_holder);
declare_deserialiser("B", b, b_holder);
declare_top_object_deserialiser(deserialiser, a_holder, b_holder);
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
Sample obj;
declare_deserialiser("A", obj.a, a_holder);
declare_deserialiser("B", obj.b, b_holder);
declare_top_object_deserialiser(deserialiser, a_holder, b_holder);
deserialiser.deserialise(json); // And two other methods.
std::cout << deserialiser.serialise(); // And two other methods.
```

#### Try to Globally Register It

If you want to enable global refelction.  
You should do this in global namespace since our reflection informations are all staticly placed in an inline namespace called JsonDeserialise.

```c++
// Class Sample Definition Here
register_object_member(Sample, "A", a);
register_object_member(Sample, "B", b);
declare_object(Sample,
    object_member(Sample, a),
    object_member(Sample, b)
);

// Somewhere
Sample s;
QJsonObject json;
declare_top_deserialiser(s, deserialiser);
deserialiser.assign(json); // json only
json = deserialiser.to_json().toObject(); // json only
//Also
declare_top_object_deserialiser(s, serialiser);
json = s.deserialise_to_json(); // And two other methods.
s.deserialise_file(FILENAME); // And two other methods.
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

declare_object(XXX, object_member(XXX, enum_field)
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

declare_deserialiser("Enum", sample, holder);
qDebug() << holder.to_json();
```
