#define vector vector_old
#include <vector>
#include <memory_resource>
#define STATIONARY_VECTOR_NAMESPACE std
#include "vital/container/stationary_vector.hpp"
#ifdef  vector
#undef  vector
#define vector stationary_vector
#endif

#include <testsuite_hooks.h>
#include <testsuite_allocator.cc>
#include <testsuite_hooks.cc>
