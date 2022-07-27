## The Ultimate C++ `enum` Library (C++14 compatible, C++17 preferred)

This library facilitates the definition and conversion of C++ enumerations.

[Click here for the implementation (header-only).](../../../../include/cpp/vital/core/enum.hpp)

### Features:
- **All** valid scopes are supported (namespaces, classes, functions)
- **No limits** on the number or range of enumerators
- **External** enumerations are supported (for generated/third-party/etc. headers)
- `enum` &harr; `std::string` conversion
- `enum_traits<T>` providing the names, values, and ordering of the `enum` members
- `enum` and `enum class` are both supported
- Duplicate enum values are supported
- "**Flag**" support, which add bitwise operators to enums
- No extraneous or missing `enum` members
- _Exact_ scope as declared (e.g., there is no additional nesting of the `enum` in another class)
- Compile-time (`constexpr`) reflection
- Compile-time (`constexpr`) tests to validate correctness

### Not (yet) supported:
- Enums with embedded strings (e.g., `enum { value [[foo("")]] }`)
- C++11 (might be infeasible)
- Trailing commas in macros (might be infeasible)

### Example usage (assuming `using namespace vit::core`)

Efficient C++14 and C++17 APIs:
```
VIT_DEFINE_ENUM_T(
	enum, E, : long long,
	E_x = 1LL << 62,
	E_y
);

// C++17
std::cout << to_string(E_y) << std::endl;  // "E_y"
#if 201703L <= __cplusplus
std::cout << enum_traits<E>::find(E_x)->name() << std::endl;  // "E_x"
#endif

// C++14
auto const &members = enum_traits<E>::members();
auto const &members_info = enum_traits<E>::members_info();
auto const &value_lookup_table = enum_traits<E>::members_sorted_by_value();
auto const &first = members[0];
printf("0x%llX: %s\n", static_cast<long long>(first.value), members_info.name_c_str(first));  // "0x4000000000000000: E_x"
printf("%s\n", members_info.name_c_str(value_lookup_table.find(E_x)));  // "E_x"
```

Enum/flag &harr; string conversions and intelligent decomposition:
```
VIT_DEFINE_FLAG(
	/* enum class */ F,
	A = 0,
	B,
	C,
	D,
	E = ((void)A, 0) + B * C + D
);

std::cout << to_string(F::C | F::D | static_cast<F>(0x8)) << std::endl;  // "0x8 | B | C | D"
std::cout << to_string(parse_enum<F>("C | D | 0x8")) << std::endl;  // "0x8 | B | C | D"

std::vector<F> components;
for_each_enum_component(F::D, [&](F component) { components.push_back(component); });
assert(components == std::vector<F>({ F::B, F::C, F::D }));
```

External enums:
```
enum class G { a = 0, b, c, d };  // external header

#define X(F) F(a) F(b) F(c) F(d)  // just re-declare the member names
VIT_EXTERNAL_FLAG(G, X);
#undef  X

std::cout << to_string(G::c | G::d | static_cast<G>(0x8)) << std::endl;  // "0x8 | b | c | d"
```

### Credits:

Thanks to [The C++ Type Loophole](https://alexpolt.github.io/type-loophole.html) for inspiration.
