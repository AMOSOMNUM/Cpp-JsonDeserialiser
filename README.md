# Qt-Based-JsonDeserialiser
Based on Qt's Json Library, it convert json into various types through static declaration by using template of Modern C++(Some traits require C++17). Simply Toy!!!
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
```c++
class Sample {
    TypeA a;
    TypeB b;
    ......
};
```
#### Once For All
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
### 3.For Map(that is embedded)
```json
//For Json:
{
    "Key":1,
    "Num":114514,
    "Name":"田所浩二"
}
```
```c++
//if you have a class like:
struct Person{
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
declare_object_map_deserialiser(namelist, "Key", holder);
JsonDeserialise::JsonDeserialiser deserialiser(holder);
deserialiser.deserialiseFile(FILENAME);
```
