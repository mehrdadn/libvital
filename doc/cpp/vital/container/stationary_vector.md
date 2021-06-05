### [`stationary_vector`](../../../include/vital/container/stationary_vector.hpp): A **low-latency**, **parallelizable** in-situ alternative to `vector`

[Click here for the implementation (header-only).](../../../include/vital/container/stationary_vector.hpp)

`stationary_vector` is an alternative to `std::vector` that grows **without** moving elements, hence:
- Its growth does not introduce "hiccups" due to reallocation.
- It can be used while pushin, as insertion does not invalidate iterators or references to prior elements.

**Semantically**, it is _almost_ a drop-in replacement for `std::vector`, except:
- It is **piecewise-contiguous** (with geometrically larger chunks), not fully contiguous.
- Move & `swap()` operations drag iterator validity along with them, since iterators hold a reference to the containers for traversal purposes. (Pointers/references are unaffected.)

Adding an extra layer of indirection around the container (e.g., via `std::unique_ptr`) can help prevent iterator invalidation.

**Performance-wise**, `stationary_vector` is not as cheap as `std::vector`:
- `sizeof(stationary_vector<T>)` is large (around 8 × (2 × 48 + 2) ≈ 800 bytes on x64).
- Each iterator is _twice_ as big as a pointer.
- `end()` is slightly more expensive than `begin()`, as it can require multiplication for non-power-of-2 sizes.
- Overall, it is slower (naturally), although my aim is to keep the performance competitive.

**Exception-safety** is intended to be on par with that of `std::vector` (if not better).
I have tried to ensure this, but it has undergone limited testing.

**Compatibility** is aimed for C++11 through C++20 on Visual C++, GCC, and Clang.

**Testing** for single-threaded use is done against test suites for `std::vector` from standard libraries,
though please be aware that such tests don't catch everything; I've found and fixed bugs that haven't been caught by tests.

**Parallelization** is currently _untested_, but you should be able to `#define STATIONARY_VECTOR_ATOMICS 1` to try it out and see how it goes.
Alongside the normal APIs, I am also adapting an elegant but little-known pre-C++11 model known as
[`volatile`-correctness](https://www.drdobbs.com/cpp/volatile-the-multithreaded-programmers-b/184403766)
to help statically guarantee thread-safety. You can ignore the `volatile` methods.<sup>1</sup>

There is still quite a lot more optimization possible.
For example, overloading `std::copy` might be a good future optimization (despite being technically illegal C++).  
How much effort I direct toward this will probably depend on any feedback I receive.

<sub><sup>1</sup> Note that my use of `volatile` is **unrelated** to its
[well-known traditional *misuse* as an atomic](https://stackoverflow.com/q/8819095).
Volatile-correctness is an entirely orthogonal static _type-safety_ mechanism that can still be adapted to atomics in modern C++,
but it appears to have faded into history over the decades.
