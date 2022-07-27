# Introduction

libvital is my attempt to create a (hopefully) useful library for various programming languages.

This a newly-launched project I've worked on in my spare time during COVID in an effort to do something
useful. Given that, its progress will likely depend on:
1. The level of interest/usage (if any) I see from the community (i.e. you!), and
2. How much free time I have, and
3. How much of that free time I can afford to spend on it.

## C++

My current focus is on (modern) C++.

### `stationary_vector`: A **low-latency**, **parallelizable** in-situ alternative to `vector`

[`vit::container::stationary_vector`](doc/cpp/vital/container/stationary_vector.md)
fills the niche between [`std::vector`](https://en.cppreference.com/w/cpp/container/vector)
& [`boost::container::stable_vector`](https://www.boost.org/doc/libs/1_75_0/doc/html/container/non_standard_containers.html).

### Other ideas I've had:

Ideas I've had for useful C++ APIs include, but are not limited to:

- A library for process management (a more lightweight `Boost.Process` alternative)
- A more modern and lightweight I/O library

# Bugs

1. **If you haven't tested the code, assume it's buggy.**  
   I've found (and fixed) many silly bugs in my code despite passing tests. More may still exist.
2. If you discover a bug, please let me know.

Everything here is "as-is". I disclaim any and all warranties/liability/etc.
