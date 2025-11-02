This document describes the design of the Joy programming language.

## Syntax

Syntactically, Joy is very similar to TypeScript, Swift, or Kotlin. The goal is to make Joy easy to learn.

```swift
// the following function adds two integers
func add(x: Int, y: Int): Int {
    return x + y;
}
```

## Methods

Joy does not have methods but the combination of function overloading and [UFCS](https://en.wikipedia.org/wiki/Uniform_function_call_syntax) make it possible to use functions just like you would use methods in other languages.

Additionally, the method call syntax enables automatic dispatch on union types, as demostrated by the following example:

```swift
struct A {}
struct B {}
func f(this: A) {}
func f(this: B) {}
func g(u: A | B) {
    u.f(); // automatically dispatches to one of the two implementations of f above
}
```

This is the preferred way of doing virtual dispatch, since Joy does not have interfaces or inheritance.

## Templates

Templates in Joy work like in C++. Template arguments can only be types, non-type template arguments are not allowed.

## Operator Overloading

In Joy, operators get dispatched to functions, which can be overloaded.

| Operator  | Equivalent Function Call |
|-----------|--------------------------|
| `a + b`   | `plus(a, b)`             |
| `a - b`   | `minus(a, b)`            |
| `a * b`   | `times(a, b)`            |
| `-a`      | `minus(a)`               |
| `a(b, c)` | `call(a, b, c)`          |

## Memory Management

Most programming languages can be put into two categories. Either they have a garbage collector or they don't. Joy doesn't really fit in either of these categories. Joy's automatic memory management is much more lightweight and predictable than what is commonly known as garbage collection.

Strings and arrays are allocated on the heap. They use reference counting and copy-on-write. Each string or array uses a single contiguous allocation to store the reference count, the size, the capacity, and the data.

Integers, structs, and tuples use direct allocation. For function arguments and variables, they are allocated on the stack. Otherwise they are allocated directly within the allocation of their parent. An array of structs for example uses a single contiguous heap allocation.

Union types also use direct allocation. Internally, they are stored as tagged unions.

Optionally, `Ref` can be used to allocate structs on the heap with reference counting and copy-on-write. This can be useful for dealing with large structs.

Regardless of whether a type was allocated on the heap or not, all types in Joy behave like values. There is no semantic difference between a struct behind a `Ref` and a regular struct. It is therefore impossible to create a reference cycle.

All in all, this makes Joy a simple and efficient memory-safe language that doesn't need a traditional garbage collector or a borrow checker.
