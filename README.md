# JsonDeserialiser

Using Qt's Json Library(But it doesn't relies on, and may support more json libraries in the near future!), it converts json into various types or data structures through static declaration by using template of Modern C++(Some traits require C++17).

## Basic Types

|BasicType|Type in C++|
|:-|:-|
|Integer|signed、unsigned|
|Real|double|
|Boolean|bool|
|String|QString、char*、std::string、QByteArray|
|Object|struct/class|

## Advanced Types

|TypeName|Type in C++|
|:-|:-|
|Nullable|T*、std::optional\<T>|
|Array|std::vector\<T>、std::set\<T>、std::list\<T>、QList\<T>、QSet\<T> e.t.c.|
|LimitedArray|T[N]、std::array\<T, N>|
|MapArray|std::map\<KeyType, ValueType>|
|Pair|std::pair\<KeyType, ValueType>|
|EnumAs|Treated same as an Existing Type|
|DerivedObject|Support single inheritance|
|Extension|An Existing Type to Any|
|Ref|Ref of an Existing Type|
|VerientObject|Object to std::variant<br>(Experimental & Untested)|

## Functional API

EndoFunctor: allows you to check or transform the data deserialised

## Usage

### 1. For Trivial Types

```c++
TypeA a;
TypeB b;
declare_deserialiser("A", a, a_holder);
declare_deserialiser("B", b, b_holder);
JsonDeserialise::JsonDeserialiser deserialiser(a_holder, b_holder);
deserialiser.deserialiseFile(FILENAME);
```

### 2.For Normal struct/class

```json
{
    "A":"TypeAData",
    "B":"TypeBData"
}
```

```c++
struct Sample {
    TypeA a;
    TypeB b;
    ......
};
```

#### Simple Usage

```c++
QJsonObject json;
Sample obj;
declare_deserialiser("A", obj.a, a_holder);
declare_deserialiser("B", obj.b, b_holder);
declare_top_object_deserialiser(deserialiser, a_holder, b_holder);
deserialiser.deserialise(json);
```

#### Globally Register It!

```c++
//After Definition
register_object_member(Sample, "A", a);
register_object_member(Sample, "B", b);
declare_object(Sample,
    object_member(Sample, a),
    object_member(Sample, b)
);
......
//when using
Sample s;
declare_top_deserialiser(s, holder);
JsonDeserialise::JsonDeserialiser deserialiser(holder);
deserialiser.deserialiseFile(FILENAME);
```

#### For container

### 3.For Map(that is embedded)

```json
[
    {
        "Key":1,
        "Num":114514,
        "Name":"田所浩二"
    }
]
```

```c++
//if you have a class like:
struct Person {
    int num;
    QString name;
};
//A map like this
std::map<int, Person> namelist;
```

```c++
//After Definition
register_object_member(Person, "Num", num);
register_object_member(Person, "Name", name);
declare_object(Person,
    object_member(Person, num),
    object_member(Person, name)
);
......
//when using
declare_top_object_map_deserialiser(namelist, "Key", holder);
JsonDeserialise::JsonDeserialiser deserialiser(holder);
//From file
deserialiser.deserialiseFile(FILENAME);
//From json_string
deserialiser.deserialise("[{"Key":1,"Num":114514,"Name":"田所浩二"}]");
```

## Extension

Example I:

```c++
enum class Type {
    A, B, C
};
QString enum2str(Type);
Type str2enum(const QString&);
```

```json
"Type":"A"
```

```c++
Type sample;
declare_extension_deserialiser("Type", sample, holder, str2enum, enum2str);
JsonDeserialise::JsonDeserialiser deserialiser(holder);
deserialiser.deserialise("{\"Type\":\"A\"}");
```
