This STL-compliant container is an _almost_-contiguous variant of `vector` that, unlike `vector`, maintains the validity of all iterators and references to existing elements during expansion.

This useful guarantee is achieved by avoiding the movement of existing elements into a larger buffer. Instead, this container keeps existing elements in-place during expansion, and only utilizes subsequent buffers for _subsequent_ elements. Unlike `deque`, this container maintains high performance by guaranteeing _at least_ exponential growth, ensuring that at most only O(log _n_) buffers are allocated for the storage of _n_ elements, and therefore that there are only O(log _n_) points of discontiguity in the container.

A few benefits of this container include the following:

- It enables an absolute worst-case O(1)-time `push_back`, which allows for smooth and consistent application performance, and which can considerably lower UI latency.
- It obviates the need to revalidate any pointers, allowing for pointers to elements to be freely stored & passed around concurrently with the expansion of the container. This can enable the usage of external libraries that utilize pointers, but which do not provide any mechanism for pointer re-validation.
- It can be used to more easily create zero-copy algorithms in cases where a growable buffer is needed, which can result in faster algorithms with lower memory usage (as the container can now be shared instead of copied).
- Like `vector` (and unlike `stable_vector`), it ensures high cache performance by ensuring that contiguity is still _almost always_ maintained.

Compared to `vector`, the downsides of this container (aside from lack of full contiguity) are generally minor:

- Iterators are slightly more expensive. (Generally, iterators are 2 pointers instead of 1.)
- Unless `STATIONARY_VECTOR_FAST_END_ITERATOR` is enabled, `end()` iterators may require indexing, in which case `end()` may perform a multiplication for non-power-of-2 sizes.
- `sizeof(stationary_vector)` is quite large (approximately 80 bytes on x64), as its storage includes space for pointers to all allocated buffers.
- Iterators (but not references) are invalidated when the container itself is moved (as iterators contain references to the container for traversal purposes).

One simple workaround for the last two downsides is to place the container on the heap and use `unique_ptr<stationary_vector>`.
