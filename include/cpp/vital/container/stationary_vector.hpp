#pragma once

#ifndef STATIONARY_VECTOR_HPP
#define STATIONARY_VECTOR_HPP

#include <limits.h>  // CHAR_BIT

#ifndef assert
#include <cassert>
#endif

// This isn't quite ready for use; don't enable it for now
#if !defined(STATIONARY_VECTOR_ATOMICS)
#define STATIONARY_VECTOR_ATOMICS 0  // Untested
#endif

// We allow customizing the namespace to make it easier to run tests intended for containers in other namespaces
#if defined(STATIONARY_VECTOR_NAMESPACE)
#if !defined(STATIONARY_VECTOR_BEGIN_NAMESPACE)
#define STATIONARY_VECTOR_BEGIN_NAMESPACE namespace STATIONARY_VECTOR_NAMESPACE {
#endif
#if !defined(STATIONARY_VECTOR_END_NAMESPACE)
#define STATIONARY_VECTOR_END_NAMESPACE }
#endif
#endif

#if defined(STATIONARY_VECTOR_ATOMICS) && (STATIONARY_VECTOR_ATOMICS)
#include <atomic>
#endif
#include <algorithm>  // std::swap
#include <iterator>
#include <limits>
#include <memory>
#include <new>  // std::bad_alloc
#include <stdexcept>  // std::length_error
#include <vector>

#if defined(_MSC_VER) && defined(_CPPLIB_VER)
#include <malloc.h>
#endif

#if defined(_CPPLIB_VER) && _CPPLIB_VER <= 650 && !defined(_MSVC_STL_VERSION)
namespace std { template<class> struct _Wrap_alloc; }
#endif

// We use noexcept(true) so that people can #define the noexcept(...) out easily in case they have issues on their compilers
#if defined(_MSC_VER) && _MSC_VER < 1900
#ifdef _NOEXCEPT_OP
#define noexcept _NOEXCEPT_OP
#else
#define noexcept(X)
#endif
#endif

namespace stdext
{
	template<class>
	class checked_array_iterator;

	template<class>
	class unchecked_array_iterator;
}

#if defined(STATIONARY_VECTOR_BEGIN_NAMESPACE)
STATIONARY_VECTOR_BEGIN_NAMESPACE
#else
namespace vit
{
namespace container
{
#endif

template<class It>
using is_quickly_traversible = std::integral_constant<bool, (sizeof(typename std::iterator_traits<It>::value_type) &(sizeof(typename std::iterator_traits<It>::value_type) - 1)) == 0>;

namespace detail
{
	typedef std::integral_constant<bool,
#if defined(NDEBUG)
		true
#else
		false
#endif
	> optimized_build;

	typedef std::integral_constant<bool,
#if defined(_DEBUG)
		true
#else
		false
#endif
	> debug_build;

	template<size_t Value> struct log2 : std::integral_constant<size_t, 1 + log2<Value / 2>::value> { };
	template<> struct log2<1> : std::integral_constant<size_t, 0> { };
	template<> struct log2<0> : std::integral_constant<size_t, ~log2<1>::value> { };

	template<class T> static typename std::enable_if<std::is_unsigned<T>::value, unsigned char>::type bsr(unsigned int *i, T const &value);
#if defined(__GNUC__)
	template<class T>
	static inline typename std::enable_if<std::is_unsigned<T>::value, unsigned char>::type bsr(unsigned int *i, T const &value)
	{
		unsigned char r;
		if (value)
		{
			r = 1;
			*i = static_cast<unsigned>(sizeof(value)) * CHAR_BIT - 1 - static_cast<unsigned>(__builtin_clzll(value));
		}
		else
		{
			r = 0;
		}
		return r;
	}
#elif defined(_MSC_VER)
#if defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64)
	template<> inline unsigned char bsr<unsigned long long>(unsigned int *i, unsigned long long const &value)
	{
		return _BitScanReverse64(reinterpret_cast<unsigned long *>(i), value);
	}
#endif
#if defined(_M_X64) || defined(_M_AMD64) || defined(_M_IA64) || defined(_M_IX86)
	template<> inline unsigned char bsr<unsigned int>(unsigned int *i, unsigned int const &value)
	{
		return _BitScanReverse(reinterpret_cast<unsigned long *>(i), value);
	}
#endif
#endif

	template<class It>
	static void fast_advance(It &i, typename std::iterator_traits<It>::difference_type d)
	{
		bool has_fast_advance = is_quickly_traversible<It>::value;
		if (!has_fast_advance && !d) { }
		else if (!has_fast_advance && d == 1)
		{
			++i;
		}
		else if (!has_fast_advance && d == -1)
		{
			--i;
		}
		else { std::advance(i, d); }
	}

	inline void throw_length_error(char const *msg)
	{
		throw std::length_error(msg);
	}

#define X_MEMBER_TYPE_DETECTOR(Member)  \
	template<class T, class F, class = void> struct extract_nested_##Member : std::enable_if<true, F> { };  \
	template<class T, class F> struct extract_nested_##Member<T, F, typename std::conditional<false, typename T::Member, void>::type> : std::enable_if<true, typename T::Member> { }
	X_MEMBER_TYPE_DETECTOR(reference);
	X_MEMBER_TYPE_DETECTOR(const_reference);
	X_MEMBER_TYPE_DETECTOR(volatile_reference);
	X_MEMBER_TYPE_DETECTOR(const_volatile_reference);
	X_MEMBER_TYPE_DETECTOR(volatile_pointer);
	X_MEMBER_TYPE_DETECTOR(const_volatile_pointer);
	X_MEMBER_TYPE_DETECTOR(is_always_equal);
#undef  X_MEMBER_TYPE_DETECTOR

	template<class T, class Base>
	struct is_defined;

	template<>
	struct is_defined<void, void>
	{
		template<class T>
		static unsigned char(&check(T(*)()))[sizeof(T) + 2];
		static unsigned char(&check(...))[1];
	};

	template<class T, class Base = std::integral_constant<bool, sizeof(is_defined<void, void>::check(static_cast<typename std::enable_if<true, T(*)()>::type>(nullptr))) != 1> >
	struct is_defined : Base { };

	template<class T>
	struct is_vc_array_iterator : std::false_type { };

	template<class T>
	struct is_vc_array_iterator<stdext::checked_array_iterator<T> > : std::true_type { };

	template<class T>
	struct is_vc_array_iterator<stdext::unchecked_array_iterator<T> > : std::true_type { };

	template<class It>
	struct checked_iterator_traits
	{
		typedef typename std::iterator_traits<It>::reference reference;
		static reference deref_unchecked(It const &i)
		{
			return *i;
		}
	};

	template<class It>
	struct checked_iterator_traits<stdext::checked_array_iterator<It> >
	{
		typedef typename std::iterator_traits<It>::reference reference;
		static reference deref_unchecked(It const &i)
		{
			It r = i.base();
			r += i.index();
			return *r;
		}
	};

	template<class It>
	struct checked_iterator_traits<stdext::unchecked_array_iterator<It> >
	{
		typedef typename std::iterator_traits<It>::reference reference;
		static reference deref_unchecked(It const &i)
		{
			return *i.base();
		}
	};

	template<class Tr, class Ax = typename Tr::allocator_type>
	struct extract_traits_select_on_container_copy_construction : std::false_type
	{
		static Ax invoke(Ax const &ax)
		{
			return ax;
		}
	};

#if !(defined(_MSC_VER) && _MSC_VER < 1910)
	template<class Tr>
	struct extract_traits_select_on_container_copy_construction<Tr, typename std::enable_if<
		!!sizeof(Tr::select_on_container_copy_construction(std::declval<typename Tr::allocator_type const &>())),
		decltype(Tr::select_on_container_copy_construction(std::declval<typename Tr::allocator_type const &>()))
	>::type> : std::true_type
	{
		static typename Tr::allocator_type invoke(typename Tr::allocator_type const &ax)
		{
			return Tr::select_on_container_copy_construction(ax);
		}
	};

#endif

	template<class Ax, class = Ax>
	struct extract_allocator_select_on_container_copy_construction : std::false_type
	{
		static Ax invoke(Ax const &ax)
		{
			return ax;
		}
	};

#if !(defined(_MSC_VER) && _MSC_VER < 1910)
	template<class Ax>
	struct extract_allocator_select_on_container_copy_construction<Ax, typename std::enable_if<
		!!sizeof(Ax::select_on_container_copy_construction(std::declval<Ax const &>())),
		decltype(Ax::select_on_container_copy_construction(std::declval<Ax const &>()))
	>::type> : std::true_type
	{
		static Ax invoke(Ax const &ax)
		{
			return ax.select_on_container_copy_construction();
		}
	};

#endif

	template<class T>
	struct volatile_pointer_of : std::enable_if<true, T> { };

	template<class T>
	struct volatile_pointer_of<T *> : std::enable_if<true, T volatile *> { };

	template<class T>
	struct volatile_reference_of : std::enable_if<true, T> { };

	template<class T>
	struct volatile_reference_of<T &> : std::enable_if<true, T volatile &> { };

	template<class T, T, T>
	struct is_same_value : std::false_type { };

	template<class T, T V>
	struct is_same_value<T, V, V> : std::true_type { };

	// Define type traits to detect whether an arbitrary allocator::member is the same as std::allocator::member.
#define X_MEMBER_FUNCTION_DEFAULT_CHECKER(Ax, CheckerName, Name, Params)  \
	template<class Ax, class = void> struct CheckerName##_detail : std::false_type { };  \
	template<class Ax> struct CheckerName##_detail<Ax, typename std::enable_if<  \
		is_same_value<typename std::allocator_traits<Ax>::pointer(std::allocator<typename std::allocator_traits<Ax>::value_type>:: *) Params,  \
			&Ax::Name,  \
			&std::allocator<typename std::allocator_traits<Ax>::value_type>::Name  \
		>::value, void>::type> : std::true_type { };  \
	template<class Ax> struct CheckerName : CheckerName##_detail<Ax> { };  \
	template<class T> struct CheckerName<std::allocator<T> > : std::true_type { }
X_MEMBER_FUNCTION_DEFAULT_CHECKER(Ax, uses_default_allocate_nohint, allocate, (typename std::allocator_traits<Ax>::size_type));
X_MEMBER_FUNCTION_DEFAULT_CHECKER(Ax, uses_default_allocate_hint, allocate, (typename std::allocator_traits<Ax>::size_type, typename std::allocator_traits<Ax>::const_void_pointer));
X_MEMBER_FUNCTION_DEFAULT_CHECKER(Ax, uses_default_deallocate, deallocate, (typename std::allocator_traits<Ax>::pointer, typename std::allocator_traits<Ax>::size_type));
#if defined(_CPPLIB_VER) && _CPPLIB_VER >= 650
template<class Ax> using uses_default_value_construct = std::_Uses_default_construct<Ax, typename std::allocator_traits<Ax>::pointer>;
template<class Ax> using uses_default_copy_construct = std::_Uses_default_construct<Ax, typename std::allocator_traits<Ax>::pointer, typename std::allocator_traits<Ax>::value_type const &>;
template<class Ax> using uses_default_destroy = std::_Uses_default_destroy<Ax, typename std::allocator_traits<Ax>::pointer>;
#else
X_MEMBER_FUNCTION_DEFAULT_CHECKER(Ax, uses_default_value_construct, construct, (typename std::allocator_traits<Ax>::pointer));
X_MEMBER_FUNCTION_DEFAULT_CHECKER(Ax, uses_default_copy_construct, construct, (typename std::allocator_traits<Ax>::pointer, typename std::allocator_traits<Ax>::value_type const &));
X_MEMBER_FUNCTION_DEFAULT_CHECKER(Ax, uses_default_destroy, destroy, (typename std::allocator_traits<Ax>::pointer));
#endif
#undef  X_MEMBER_FUNCTION_DEFAULT_CHECKER
}

enum stationary_vector_growth
{
	// Optimizes for contiguity to allow arbitrarily large blocks, at the expense of making iterator traversal take O(log n) time.
	stationary_vector_growth_prefer_contiguity,
	// Allows the behavior to be customized at runtime (at a minor performance cost, typically < 20%). The default behavior in this case is unspecified.
	stationary_vector_growth_dynamic_preference,
	// Optimizes iterator traversal to O(1) at the expense of potentially excessive discontiguity, by forcing blocks to grow geometrically.
	stationary_vector_growth_prefer_fast_iterators,
};

template<std::nullptr_t = nullptr>
// Do not override this; this is for debugging the implementation. (Note: This may break debugger visualizers.)
struct stationary_vector_debugging : std::integral_constant<bool, !detail::optimized_build::value>
{
};

// Don't specialize the const/volatile versions; they're for internal use. Instead, specialize the raw version, and inherit from the const version to obtain the defaults.
template<class Ax, class Constraints = void>
struct allocator_optimizations : allocator_optimizations<Ax volatile, Constraints>
{
};

template<class Ax, class Constraints>
struct allocator_optimizations<Ax volatile, Constraints>
{
	enum
	{
		assume_allocator_is_unspecialized = false,
	};

	// If true is returned, the allocator is assumed to pass calls through to malloc() without any extra bookkeeping.
	// This enables the potential use of implementation-specific optimizations (such as _expand() in VC++) to expand the underlying buffer.
	// Note that this is only performed on a best-effort basis, and no such optimizations are guaranteed.
	static bool can_assume_allocator_passthrough_to_malloc(Ax const &) { return false; }
};

namespace detail
{
	template<class Ax>
	class extended_allocator_traits
	{
		typedef Ax allocator_type;
		typedef std::allocator_traits<allocator_type> traits_type;
	public:
		typedef typename traits_type::value_type value_type;
#define X(Member, Fallback) typedef typename extract_nested_##Member<traits_type, typename extract_nested_##Member<allocator_type, Fallback>::type>::type Member
		X(reference, value_type &);
		X(const_reference, value_type const &);
		X(volatile_reference, typename volatile_reference_of<reference>::type);
		X(const_volatile_reference, typename volatile_reference_of<const_reference>::type);
		X(volatile_pointer, typename volatile_pointer_of<typename traits_type::pointer>::type);
		X(const_volatile_pointer, typename volatile_pointer_of<typename traits_type::const_pointer>::type);
		X(is_always_equal, std::is_empty<allocator_type>);
#undef  X
		static value_type *address(allocator_type const &, reference r)
		{
			return std::addressof(static_cast<value_type &>(r));
		}

		static value_type const *address(allocator_type const &, const_reference r)
		{
			return std::addressof(static_cast<value_type const &>(r));
		}

		static value_type *ptr_address(allocator_type const &ax, typename traits_type::pointer p)
		{
			return !p ? NULL : address(ax, checked_iterator_traits<typename traits_type::pointer>::deref_unchecked(p));
		}

		static value_type const *ptr_address(allocator_type const &ax, typename traits_type::const_pointer p)
		{
			return !p ? NULL : address(ax, checked_iterator_traits<typename traits_type::const_pointer>::deref_unchecked(p));
		}

		static allocator_type select_on_container_copy_construction(allocator_type const &ax)
		{
			return std::conditional<
				extract_traits_select_on_container_copy_construction<traits_type>::value,
				extract_traits_select_on_container_copy_construction<traits_type>,
				extract_allocator_select_on_container_copy_construction<allocator_type>
			>::type::invoke(ax);
		}
	};

	template<class T1, class T2>
	struct propagate_cv : std::enable_if<true, T2> { };

	template<class T1, class T2>
	struct propagate_cv<T1 const, T2> : propagate_cv<T1, T2 const> { };

	template<class T1, class T2>
	struct propagate_cv<T1 volatile, T2> : propagate_cv<T1, T2 volatile> { };

	template<class T1, class T2>
	struct propagate_cv<T1 const volatile, T2> : propagate_cv<T1, T2 const volatile> { };

	template<class It, class Category, class = void>
	struct is_iterator : std::false_type { };

	template<class It, class Category>
	struct is_iterator<
		It,
		Category,
		typename std::enable_if<
			std::is_base_of<
				Category,
				typename std::enable_if<
					!std::is_integral<It>::value /* Workaround for _MSC_VER < 1900 */ && !std::is_void<typename std::remove_pointer<It>::type>::value,
					std::iterator_traits<It>
				>::type::iterator_category
			>::value,
		void>::type> : std::true_type { };

	template<class Ax>
	struct allocator_temporary : private Ax
	{
		typedef std::allocator_traits<Ax> traits_type;
		typedef typename traits_type::value_type value_type;
		typedef typename traits_type::pointer pointer;

		value_type *addr()
		{
			return reinterpret_cast<value_type *>(&this->buf);
		}

		value_type &get()
		{
			return *this->addr();
		}

		Ax &allocator()
		{
			return static_cast<Ax &>(*this);
		}

		~allocator_temporary()
		{
			traits_type::destroy(this->allocator(), this->addr());
		}

		template<class... Args>
		explicit allocator_temporary(Ax &ax, Args &&... args)
			noexcept(noexcept(traits_type::construct(std::declval<Ax &>(), std::declval<typename traits_type::value_type *>(), std::forward<Args>(args)...))) : Ax(ax)
		{
			traits_type::construct(this->allocator(), this->addr(), std::forward<Args>(args)...);
		}
	private:
		typename std::aligned_storage<sizeof(value_type), std::alignment_of<value_type>::value>::type buf;
		allocator_temporary(allocator_temporary const &);
		allocator_temporary &operator=(allocator_temporary const &);
	};

	template<class It>
	struct reverse_iterator : public std::reverse_iterator<It>
	{
		reverse_iterator() = default;
		reverse_iterator(std::reverse_iterator<It> const &p) : std::reverse_iterator<It>(p) { }
		explicit reverse_iterator(It const p) : std::reverse_iterator<It>(p) { }
		operator It() const
		{
			return this->std::reverse_iterator<It>::base() - 1;
		}
	};

	template<class Ax>
	class wrap_alloc : public Ax
	{
	public:
		typedef reverse_iterator<typename std::allocator_traits<Ax>::pointer> pointer;
		wrap_alloc() = default;
		wrap_alloc(Ax const &ax) : Ax(ax) { }
		template<class U>
		struct rebind { typedef wrap_alloc<U> other; };
	};

	template<class Pointer, bool = is_defined<stdext::checked_array_iterator<unsigned char *> >::value, class Constaints = void>
	struct convertible_array_iterator_base
	{
		typedef Pointer pointer;
		pointer arr;
		size_t size, index;
		convertible_array_iterator_base(pointer const &arr, size_t const size, size_t const index) : arr(arr), size(size), index(index) { }
		convertible_array_iterator_base() : arr(), size(), index() { }
		operator pointer() const
		{
			return arr + static_cast<typename std::iterator_traits<pointer>::difference_type>(index);
		}
		typedef pointer type;
	};

	template<class T>
	struct convertible_array_iterator_base<T *, true, typename std::conditional<
		stationary_vector_debugging<typename std::conditional<false, T, std::nullptr_t>::type(nullptr)>::value
		/* This breaks the API (&v[0] is no longer a pointer) so only enable it when debugging the container itself */
		, T, void>::type> : public convertible_array_iterator_base<T *, false>
	{
		typedef T *pointer;
		typedef convertible_array_iterator_base<pointer, false> base_type;
		convertible_array_iterator_base() = default;
		explicit convertible_array_iterator_base(pointer const &arr, size_t const size, size_t const index) : base_type(arr, size, index) { }
		operator stdext::checked_array_iterator<pointer>() const
		{
			stdext::checked_array_iterator<pointer> result(this->arr, this->size, this->index);
			return result;
		}

		operator stdext::unchecked_array_iterator<pointer>() const
		{
			stdext::unchecked_array_iterator<pointer> result(this->operator pointer());
			return result;
		}
		typedef stdext::
#if defined(_SECURE_SCL) && _SECURE_SCL
			checked_array_iterator
#else
			unchecked_array_iterator
#endif
			<pointer> type;
	};

	template<class Pointer>
	struct convertible_array_iterator : public convertible_array_iterator_base<Pointer>
	{
		typedef convertible_array_iterator this_type;
		typedef convertible_array_iterator_base<Pointer> base_type;
		convertible_array_iterator(typename base_type::pointer const &arr, size_t const size, size_t const index) : base_type(arr, size, index) { }
		convertible_array_iterator() = default;
	};

	template<class Iterator>
	struct iterator_partitioner
	{
		typedef typename std::enable_if<std::is_same<Iterator, typename std::remove_reference<Iterator>::type>::value, Iterator>::type iterator;
		typedef typename std::iterator_traits<iterator>::difference_type difference_type;
		iterator_partitioner() : _begin(), _end() { }
		explicit iterator_partitioner(iterator const &begin, iterator const &end) : _begin(begin), _end(end) { }
		iterator begin() const
		{
			return this->_begin;
		}

		iterator end() const
		{
			return this->_end;
		}

		bool empty() const
		{
			return this->_begin == this->_end;
		}

		iterator begin_front() const
		{
			return this->_begin;
		}

		void begin_front(iterator const &value)
		{
			this->_begin = value;
		}

		iterator end_front() const
		{
			return this->_end;
		}

		difference_type front_distance() const
		{
			return this->end_front() - this->begin_front();
		}

		void pop_front()
		{
			this->_begin = this->_end;
		}

		iterator begin_back() const
		{
			return this->_begin;
		}

		iterator end_back() const
		{
			return this->_end;
		}

		void end_back(iterator const &value)
		{
			this->_end = value;
		}

		difference_type back_distance() const
		{
			return this->end_back() - this->begin_back();
		}

		void pop_back()
		{
			this->_end = this->_begin;
		}
	protected:
		iterator _begin, _end;
	};

	template<class Iterator>
	struct iterator_partitioner<std::reverse_iterator<Iterator> > : protected iterator_partitioner<Iterator>
	{
		typedef iterator_partitioner<Iterator> base_type;
		typedef std::reverse_iterator<Iterator> base_iterator;
		typedef std::reverse_iterator<typename base_type::iterator> iterator;
		typedef typename std::iterator_traits<iterator>::difference_type difference_type;
		iterator_partitioner() = default;
		explicit iterator_partitioner(base_iterator const &begin, base_iterator const &end) : base_type(end.base(), begin.base()) { }
		base_iterator begin() const
		{
			return base_iterator(this->base_type::begin());
		}

		base_iterator end() const
		{
			return base_iterator(this->base_type::end());
		}

		bool empty() const
		{
			return this->base_type::empty();
		}

		iterator begin_front() const
		{
			return iterator(this->base_type::end_back());
		}

		void begin_front(iterator const &value)
		{
			this->base_type::end_back(value.base());
		}

		iterator end_front() const
		{
			return iterator(this->base_type::begin_back());
		}

		difference_type front_distance() const
		{
			return this->base_type::front_distance();
		}

		void pop_front()
		{
			return this->base_type::pop_back();
		}

		iterator begin_back() const
		{
			return iterator(this->base_type::end_front());
		}

		iterator end_back() const
		{
			return iterator(this->base_type::begin_front());
		}

		void end_back(iterator const &value)
		{
			this->base_type::begin_front(value.base());
		}

		difference_type back_distance() const
		{
			return this->base_type::back_distance();
		}

		void pop_back()
		{
			return this->base_type::pop_front();
		}
	};

	template<class Iterator>
	struct iterator_partitioner<std::move_iterator<Iterator> > : protected iterator_partitioner<Iterator>
	{
		typedef iterator_partitioner<Iterator> base_type;
		typedef std::move_iterator<Iterator> base_iterator;
		typedef std::move_iterator<typename base_type::iterator> iterator;
		typedef typename std::iterator_traits<iterator>::difference_type difference_type;
		iterator_partitioner() = default;
		explicit iterator_partitioner(base_iterator const &begin, base_iterator const &end) : base_type(begin.base(), end.base()) { }
		base_iterator begin() const
		{
			return base_iterator(this->base_type::begin());
		}

		base_iterator end() const
		{
			return base_iterator(this->base_type::end());
		}

		bool empty() const
		{
			return this->base_type::empty();
		}

		iterator begin_front() const
		{
			return iterator(this->base_type::begin_front());
		}

		void begin_front(iterator const &value)
		{
			this->base_type::begin_front(value.base());
		}

		iterator end_front() const
		{
			return iterator(this->base_type::end_front());
		}

		difference_type front_distance() const
		{
			return this->base_type::front_distance();
		}

		void pop_front()
		{
			return this->base_type::pop_front();
		}

		iterator begin_back() const
		{
			return iterator(this->base_type::begin_back());
		}

		iterator end_back() const
		{
			return iterator(this->base_type::end_back());
		}

		void end_back(iterator const &value)
		{
			this->base_type::end_back(value.base());
		}

		difference_type back_distance() const
		{
			return this->base_type::back_distance();
		}

		void pop_back()
		{
			return this->base_type::pop_back();
		}
	};

	template<class It>
	It destroy(It begin, It end)
	{
#if __cplusplus >= 201703L || defined(__cpp_lib_raw_memory_algorithms)
		std::destroy(begin, end);
		begin = end;
#else
		typedef typename std::iterator_traits<It>::value_type value_type;
		while (begin != end)
		{
			begin->~value_type();
			++begin;
		}
#endif
		return begin;
	}

	template<class FwdIt, class OutIt>
	OutIt uninitialized_destructive_move(FwdIt begin, FwdIt end, OutIt dest)
	{
		dest =
#if __cplusplus >= 201703L || defined(__cpp_lib_raw_memory_algorithms)
			std::uninitialized_move(begin, end, dest)
#else
			std::uninitialized_copy(std::move_iterator<FwdIt>(begin), std::move_iterator<FwdIt>(end), dest)
#endif
			;
		detail::destroy(begin, end);
		return dest;
	}

	template<class FwdIt, class OutIt>
	typename std::enable_if<
		std::is_nothrow_move_constructible<typename std::iterator_traits<OutIt>::value_type>::value &&
		std::is_nothrow_destructible<typename std::iterator_traits<OutIt>::value_type>::value
		, OutIt>::type uninitialized_destructive_move_if_noexcept(FwdIt begin, FwdIt end, OutIt dest)
	{
		// If we can move without throwing, then perform a destructive move construction.
		return uninitialized_destructive_move<FwdIt, OutIt>(std::forward<FwdIt>(begin), std::forward<FwdIt>(end), std::forward<OutIt>(dest));
	}

	template<class FwdIt, class OutIt>
	typename std::enable_if<
		std::is_nothrow_move_constructible<typename std::iterator_traits<FwdIt>::value_type>::value &&
		std::is_nothrow_destructible<typename std::iterator_traits<FwdIt>::value_type>::value
		, OutIt>::type uninitialized_destructive_move_if_noexcept_backout(FwdIt begin, FwdIt end, OutIt src)
	{
		// Backing out nothrow uninitialized move construction a destructive move construction back into the source.
		return uninitialized_destructive_move<FwdIt, OutIt>(std::forward<FwdIt>(begin), std::forward<FwdIt>(end), std::forward<OutIt>(src));
	}

	template<class FwdIt, class OutIt>
	typename std::enable_if<!(
		std::is_nothrow_move_constructible<typename std::iterator_traits<OutIt>::value_type>::value &&
		std::is_nothrow_destructible<typename std::iterator_traits<OutIt>::value_type>::value
		), OutIt>::type uninitialized_destructive_move_if_noexcept(FwdIt begin, FwdIt end, OutIt dest)
	{
		// If we can't move without throwing, then copy instead.
		return std::uninitialized_copy(begin, end, dest);
	}

	template<class FwdIt, class OutIt>
	typename std::enable_if<!(
		std::is_nothrow_move_constructible<typename std::iterator_traits<FwdIt>::value_type>::value &&
		std::is_nothrow_destructible<typename std::iterator_traits<FwdIt>::value_type>::value
		), OutIt>::type uninitialized_destructive_move_if_noexcept_backout(FwdIt begin, FwdIt end, OutIt src)
	{
		// Backing out uninitialized copy construction is mere destruction, as we already have a copy at the source.
		std::advance<OutIt>(src, std::distance(begin, end));
		detail::destroy(begin, end);
		return src;
	}

	template<class Ax, class It>
	struct initialization_guard
	{
		typedef initialization_guard this_type;
		typedef Ax allocator_type;
		typedef std::allocator_traits<Ax> traits_type;
		typedef It iterator;
		initialization_guard() : ax() { }
		explicit initialization_guard(allocator_type &ax, iterator const &begin) : i(begin), begin(begin), ax(std::addressof(ax)) { }
		~initialization_guard()
		{
			if (ax)
			{
				while (i != begin)
				{
					traits_type::destroy(*ax, detail::extended_allocator_traits<allocator_type>::address(*ax, *--i));
				}
			}
		}

		template<class... Args>
		void emplace_back(Args &&... args)
		{
			traits_type::construct(*ax, detail::extended_allocator_traits<allocator_type>::address(*ax, *i), std::forward<Args>(args)...);
			++i;
		}

		iterator release()
		{
			begin = i;
			return i;
		}
		iterator i;
	private:
		iterator begin;
		allocator_type *ax;
		initialization_guard(this_type const &);
		this_type &operator =(this_type const &);
	};

	class copy_operation
	{
	public:
		template<class InIt, class OutIt>
		typename std::enable_if<(
			is_iterator<InIt, std::input_iterator_tag>::value &&
			(is_iterator<OutIt, std::output_iterator_tag>::value || is_iterator<OutIt, std::bidirectional_iterator_tag>::value)
			), void>::type single(InIt &input, OutIt &output)
		{
			*output = *input;
			++output;
			++input;
		}

		template<class OutIt>
		typename std::enable_if<is_iterator<OutIt, std::bidirectional_iterator_tag>::value, void>::type cleanup_single(OutIt &output)
		{
			--output;
		}

		template<class InIt, class OutIt>
		typename std::enable_if<(
			is_iterator<InIt, std::input_iterator_tag>::value &&
			(is_iterator<OutIt, std::output_iterator_tag>::value || is_iterator<OutIt, std::bidirectional_iterator_tag>::value)
			), OutIt>::type multiple(InIt &&i, InIt const &j, OutIt &&out)
		{
			return std::copy(std::forward<InIt>(i), j, std::forward<OutIt>(out));
		}
	};

	class move_operation
	{
	public:
		template<class InIt, class OutIt>
		typename std::enable_if<(
			is_iterator<InIt, std::input_iterator_tag>::value &&
			(is_iterator<OutIt, std::output_iterator_tag>::value || is_iterator<OutIt, std::bidirectional_iterator_tag>::value)
			), void>::type single(InIt &input, OutIt &output)
		{
			*output = std::move(*input);
			++output;
			++input;
		}

		template<class OutIt>
		typename std::enable_if<is_iterator<OutIt, std::bidirectional_iterator_tag>::value, void>::type cleanup_single(OutIt &output)
		{ --output; }

		template<class InIt, class OutIt>
		typename std::enable_if<(
			is_iterator<InIt, std::input_iterator_tag>::value &&
			(is_iterator<OutIt, std::output_iterator_tag>::value || is_iterator<OutIt, std::bidirectional_iterator_tag>::value)
			), OutIt>::type multiple(InIt &&i, InIt const &j, OutIt &&out)
		{ return std::move(std::forward<InIt>(i), j, std::forward<OutIt>(out)); }
	};

	template<class Ax>
	class uninitialized_copy_operation : private Ax
	{
		typedef std::allocator_traits<Ax> traits_type;
		Ax &allocator()
		{
			return *this;
		}
	public:
		explicit uninitialized_copy_operation(Ax const &ax) : Ax(ax) { }
		template<class InIt, class OutIt>
		typename std::enable_if<(
			is_iterator<InIt, std::input_iterator_tag>::value &&
			is_iterator<OutIt, std::bidirectional_iterator_tag>::value
			), void>::type single(InIt &input, OutIt &output)
		{
			traits_type::construct(this->allocator(), detail::extended_allocator_traits<Ax>::address(this->allocator(), *output), *input);
			++output;
			++input;
		}

		template<class OutIt>
		typename std::enable_if<is_iterator<OutIt, std::bidirectional_iterator_tag>::value, void>::type cleanup_single(OutIt &output)
		{
			--output;
			traits_type::destroy(this->allocator(), detail::extended_allocator_traits<Ax>::address(this->allocator(), *output));
		}

		template<class InIt, class OutIt>
		typename std::enable_if<(
			(
				allocator_optimizations<Ax>::assume_allocator_is_unspecialized &&
				detail::uses_default_value_construct<Ax>::value && std::is_trivially_default_constructible<typename std::allocator_traits<Ax>::value_type>::value &&
				detail::uses_default_copy_construct<Ax>::value && std::is_trivially_copyable<typename std::allocator_traits<Ax>::value_type>::value &&
				detail::uses_default_destroy<Ax>::value && std::is_trivially_destructible<Ax>::value
			) &&
			is_iterator<InIt, std::bidirectional_iterator_tag>::value &&
			is_iterator<OutIt, std::bidirectional_iterator_tag>::value
			), OutIt>::type multiple(std::reverse_iterator<InIt> begin, std::reverse_iterator<InIt> end, OutIt out)
		{
			return std::reverse_copy(end.base(), begin.base(), out);
		}

		template<class InIt, class OutIt>
		typename std::enable_if<(
			is_iterator<InIt, std::input_iterator_tag>::value &&
			is_iterator<OutIt, std::bidirectional_iterator_tag>::value
			), OutIt>::type multiple(InIt begin, InIt end, OutIt out)
		{
#if defined(_CPPLIB_VER)
#if defined(_CPPLIB_VER) && _CPPLIB_VER <= 650 && !defined(_MSVC_STL_VERSION)
			typename std::conditional<is_defined<std::_Wrap_alloc<Ax> >::value, std::_Wrap_alloc<Ax>, Ax &>::type

#else
			Ax &
#endif
				wrap_ax(this->allocator());
			out = std::_Uninitialized_copy(std::forward<InIt>(begin), std::forward<InIt>(end), std::forward<OutIt>(out), wrap_ax);
#else
			// PERF: Optimize allocator-aware uninitialized_copy() for POD
			initialization_guard<Ax, OutIt> guard(this->allocator(), std::forward<OutIt>(out));
			for (; begin != end; ++begin)
			{
				guard.emplace_back(*begin);
			}
			out = guard.release();
#endif
			return out;
		}
	};

	template<class FwdIt>
	class safe_move_or_copy_construction
	{
		typedef safe_move_or_copy_construction this_type;
		safe_move_or_copy_construction(this_type const &other);
		this_type &operator =(this_type const &other);
		typedef FwdIt iterator;
		typedef typename std::iterator_traits<iterator>::value_type value_type;
		typedef typename std::iterator_traits<iterator>::difference_type difference_type;
		iterator src;
		iterator begin, end;
	public:
		~safe_move_or_copy_construction()
		{
			detail::uninitialized_destructive_move_if_noexcept_backout<iterator, iterator>(this->begin, this->end, this->src);
		}

		explicit safe_move_or_copy_construction(iterator const begin, iterator const end, iterator const dest) : src(begin), begin(dest), end(dest)
		{
			std::advance<iterator>(this->end, std::distance(dest, uninitialized_destructive_move_if_noexcept<iterator, iterator>(begin, end, dest)));
		}

		void swap(this_type &other)
		{
			using std::swap;
			swap(this->begin, other.begin);
			swap(this->end, other.end);
		}

		friend void swap(this_type &a, this_type &b)
		{
			return a.swap(b);
		}

		void release()
		{
			this->begin = this->end;
		}
	};

	template<class FwdIt1, class FwdIt2>
	typename std::enable_if<
		detail::is_iterator<FwdIt1, std::input_iterator_tag>::value &&
		detail::is_iterator<FwdIt2, std::input_iterator_tag>::value &&
		!(
			detail::is_iterator<FwdIt1, std::forward_iterator_tag>::value &&
			std::is_trivially_default_constructible<typename std::iterator_traits<FwdIt1>::value_type>::value &&
			std::is_trivially_destructible<typename std::iterator_traits<FwdIt1>::value_type>::value &&
			detail::is_iterator<FwdIt2, std::forward_iterator_tag>::value &&
			std::is_trivially_default_constructible<typename std::iterator_traits<FwdIt2>::value_type>::value &&
			std::is_trivially_destructible<typename std::iterator_traits<FwdIt2>::value_type>::value
		), void>::type uninitialized_swap_ranges(FwdIt1 begin1, FwdIt1 end1, FwdIt2 begin2, FwdIt2 end2)  // swaps two ranges, assuming sufficient uninitialized space exists beyond the shorter range
	{
		FwdIt1 mid1 = begin1;
		FwdIt2 mid2 = begin2;
		{
			typename std::iterator_traits<FwdIt1>::difference_type const n1 = std::distance(begin1, end1);
			typename std::iterator_traits<FwdIt2>::difference_type const n2 = std::distance(begin2, end2);
			n1 < n2
				? std::advance<FwdIt2>(mid2, n1)
				: std::advance<FwdIt2>(mid2, n2);
			n2 < n1
				? std::advance<FwdIt1>(mid1, n2)
				: std::advance<FwdIt1>(mid1, n1);
		}
		{
			safe_move_or_copy_construction<FwdIt1> op1(mid1, end1, mid2);
			{
				safe_move_or_copy_construction<FwdIt2> op2(mid2, end2, mid1);
				{
					bool success = std::swap_ranges(begin1, mid1, begin2) == mid2;
					(void)success;
					assert(success);
				}
				op2.release();
			}
			op1.release();
		}
	}

	template<bool B, class T> inline typename std::enable_if<B, void>::type swap_if(T &a, T &b)
	{
		using std::swap;
		swap(a, b);
	}

	template<bool B, class T> inline typename std::enable_if<!B, void>::type swap_if(T &, T &) { }

	template<class FwdIt1, class FwdIt2>
	typename std::enable_if<(
		detail::is_iterator<FwdIt1, std::forward_iterator_tag>::value &&
		std::is_trivially_default_constructible<typename std::iterator_traits<FwdIt1>::value_type>::value &&
		std::is_trivially_destructible<typename std::iterator_traits<FwdIt1>::value_type>::value &&
		detail::is_iterator<FwdIt2, std::forward_iterator_tag>::value &&
		std::is_trivially_default_constructible<typename std::iterator_traits<FwdIt2>::value_type>::value &&
		std::is_trivially_destructible<typename std::iterator_traits<FwdIt2>::value_type>::value
		), void>::type uninitialized_swap_ranges(FwdIt1 begin1, FwdIt1 end1, FwdIt2 begin2, FwdIt2 end2)
		// swaps two ranges, assuming sufficient uninitialized space exists beyond the shorter range
	{
		if (std::distance(begin2, end2) < std::distance(begin1, end1))
		{
			begin2 = std::swap_ranges(begin1, end1, begin2);
		}
		else
		{
			begin1 = std::swap_ranges(begin2, end2, begin1);
		}
	}

	template<class Ax, class OutIt>
	OutIt uninitialized_value_construct(OutIt begin, typename std::allocator_traits<Ax>::size_type n, Ax &ax)
	{
#if defined(_CPPLIB_VER) && _CPPLIB_VER == 650 && defined(_MSVC_STL_VERSION) && 141 <= _MSVC_STL_VERSION && _MSVC_STL_VERSION <= 142
		begin = std::_Uninitialized_value_construct_n(std::forward<OutIt>(begin), n, ax);
#else
		// PERF: Optimize allocator-aware uninitialized_value_construct() for POD
		initialization_guard<Ax, OutIt> guard(ax, std::forward<OutIt>(begin));
		while (n)
		{
			guard.emplace_back();
			--n;
		}
		begin = guard.release();
#endif
		return begin;
	}

	template<class Ax>
	class heap_ptr : public Ax
	{
		heap_ptr(heap_ptr const &);
		heap_ptr &operator =(heap_ptr const &);
		typedef std::allocator_traits<Ax> traits_type;
		typedef typename traits_type::pointer pointer;
		pointer p;
		typename traits_type::size_type n;
	public:
		~heap_ptr()
		{
			if (this->p != pointer())
			{
				traits_type::deallocate(static_cast<Ax &>(*this), this->p, this->n);
			}
		}

		explicit heap_ptr(Ax &ax, typename traits_type::size_type const count, pointer const hint /* only before C++20 */) : Ax(ax), p(), n(count)
		{
			if (count)
			{
				this->p = static_cast<pointer>(traits_type::allocate(static_cast<Ax &>(*this), count, hint));
			}
		}

		pointer get() const
		{
			return this->p;
		}

		pointer release()
		{
			pointer const ptr = this->p;
			this->p = pointer();
			return ptr;
		}
	};

#if defined(STATIONARY_VECTOR_ATOMICS) && (STATIONARY_VECTOR_ATOMICS)
	template<class T, std::memory_order DefaultMemoryOrder = std::memory_order_relaxed>
	class loose_atomic  // Relaxed memory ordering is probably wrong here!
	{
		typedef loose_atomic this_type;
		std::atomic<T> data;
		this_type &unvolatile() volatile
		{
			return const_cast<this_type &>(*this);
		}

		this_type const &unvolatile() const volatile
		{
			return const_cast<this_type const &>(*this);
		}

		T exchange(T value)
		{
			return this->data.exchange(value, DefaultMemoryOrder);
		}

		T load() const
		{
			return this->data.load(DefaultMemoryOrder);
		}

		T load() const volatile
		{
			return this->unvolatile().load();
		}
	public:
		loose_atomic() /* This might NOT initialize the internal value! */ = default;
		explicit loose_atomic(T const &value) : data(value) { }
		loose_atomic(this_type const &other) : data(other.operator T()) { }
		loose_atomic(this_type &&other) : data(other.exchange(T())) { }
		this_type &operator =(this_type const &other)
		{
			this->data.store(other.operator T(), DefaultMemoryOrder);
			return *this;
		}

		this_type &operator =(T const &value)
		{
			this->data.store(value, DefaultMemoryOrder);
			return *this;
		}

		this_type &operator =(this_type &&other)
		{
			this->data.store(other.exchange(T()), DefaultMemoryOrder);
			return *this;
		}

		this_type &operator--()
		{
			return this->operator-=(1);
		}

		this_type &operator++()
		{
			return this->operator+=(1);
		}

		this_type &operator-=(T value)
		{
			this->data.fetch_sub(value, DefaultMemoryOrder);
			return *this;
		}

		this_type &operator+=(T value)
		{
			this->data.fetch_add(value, DefaultMemoryOrder);
			return *this;
		}

		operator T() const
		{
			return this->load();
		}

		operator T() const volatile
		{
			return this->load();
		}

		T swap(this_type &other)
		{
			return this->exchange(other.exchange(this->load()));
		}

		T swap(this_type volatile &other) volatile
		{
			return this->exchange(other.exchange(this->load()));
		}

		friend T swap(this_type &a, this_type &b)
		{
			return a.swap(b);
		}

		friend T swap(this_type volatile &a, this_type volatile &b)
		{
			return a.swap(b);
		}
	};

#endif

	template<class T, class Atomic =
#if defined(STATIONARY_VECTOR_ATOMICS) && (STATIONARY_VECTOR_ATOMICS)
		loose_atomic<T>
#else
		T
#endif
	>
	struct atomic_traits_of  // Customization point for those who want atomic behavior for a type
	{
		typedef typename std::conditional<std::is_trivially_copyable<T>::value, Atomic, T>::type atomic_type;

		static T load(T volatile const *const p)
		{
			return /* HACK: Reading a non-atomic as an atomic */ static_cast<T>(*reinterpret_cast<atomic_type const *>(const_cast<T const *>(p)));
		}
	};

	template<class It, class Ax>
	It destroy_preferably_in_reverse(It const begin, It end, Ax &ax)
	{
		bool elide = false;
#if defined(_CPPLIB_VER) && _CPPLIB_VER >= 650 && defined(_XMEMORY_)  // Is std::_Destroy_range is available?
		// std::_Destroy_range is available, so let's use it. This is an important optimization as it will elide no-op destructions.
		// MSVC's STL implements this by using std::_Is_default_allocator<allocator> to verify the presence of allocator::_From_primary, as well as the absence of destroy().
		// Notice that this is is *NOT* something we can portably implement ourselves, because we have no way to detect if an allocator's destructor is a no-op.
		{
			typedef wrap_alloc<Ax> allocator_type;
			allocator_type rev_ax(ax);
#if defined(_CPPLIB_VER) && _CPPLIB_VER <= 650 && !defined(_MSVC_STL_VERSION)
			typename std::conditional<is_defined<std::_Wrap_alloc<allocator_type> >::value, std::_Wrap_alloc<allocator_type>, allocator_type &>::type

#else
			allocator_type &
#endif
				wrap_ax(rev_ax);
			std::_Destroy_range(static_cast<typename allocator_type::pointer>(end), static_cast<typename allocator_type::pointer>(begin), wrap_ax);
			end = begin;
			elide = true;
		}
#endif
		if (!elide)
		{
			while (end != begin)
			{
				std::allocator_traits<Ax>::destroy(ax, extended_allocator_traits<Ax>::address(ax, *--end));
			}
		}
		return end;
	}

	template<class Ax, class It>
	typename std::enable_if<(
		detail::is_iterator<typename std::decay<It>::type, std::bidirectional_iterator_tag>::value
		), void>::type reverse(It begin, It end, Ax &ax)
	{
		typedef typename std::allocator_traits<Ax>::difference_type difference_type;
		difference_type d = std::distance(begin, end);
		if (d >= 2)
		{
			--end;
			allocator_temporary<Ax> temp(ax, std::move(*end));
			*end = std::move(*begin);
			--d;
			*begin = std::move(temp.get());
			--d;
			++begin;
			if (d >= 2)
			{
				It mid1(begin);
				std::advance<It>(mid1, +(d / static_cast<difference_type>(2)));
				It mid2(end);
				std::advance<It>(mid2, -(d / static_cast<difference_type>(2)));
				typedef iterator_partitioner<It> Partitioner;
				typedef typename Partitioner::iterator pointer;
				Partitioner lparts(begin, mid1), rparts(mid2, end);
				while (!lparts.empty() || !rparts.empty())
				{
					assert(lparts.empty() == rparts.empty());
					difference_type dmin;
					{
						difference_type const d1 = lparts.front_distance(), d2 = rparts.back_distance();
						dmin = d2 < d1 ? d2 : d1;
					}
					pointer i1 = lparts.begin_front();
					pointer j2 = rparts.end_back();
					{
						pointer j1 = i1;
						pointer i2 = j2;
						std::advance<pointer>(j1, static_cast<typename std::iterator_traits<pointer>::difference_type>(+dmin));
						std::advance<pointer>(i2, static_cast<typename std::iterator_traits<pointer>::difference_type>(-dmin));
						rparts.end_back(i2);
						lparts.begin_front(j1);
						if (i2 == rparts.begin_back())
						{
							rparts.pop_back();
						}

						if (j1 == lparts.end_front())
						{
							lparts.pop_front();
						}
					}

					while (dmin > 0)
					{
						--j2;
						temp.get() = std::move(*j2);
						*j2 = std::move(*i1);
						*i1 = std::move(temp.get());
						++i1;
						--dmin;
					}
				}
			}
		}
	}

	template<class Ax, class It>
	typename std::enable_if<(
		detail::is_iterator<typename std::decay<It>::type, std::bidirectional_iterator_tag>::value
		), void>::type rotate(Ax &ax, It begin, It mid, It end)
	{
		// Can't call std::rotate as it lacks an allocator for construction
		detail::reverse(begin, mid, ax);
		detail::reverse(mid, end, ax);
		detail::reverse(begin, end, ax);
	}

	template<class InIt, class OutIt, class BinOp>
	typename std::enable_if<
		detail::is_iterator<InIt, std::input_iterator_tag>::value &&
		(detail::is_iterator<OutIt, std::output_iterator_tag>::value || detail::is_iterator<OutIt, std::bidirectional_iterator_tag>::value) && !(
		detail::is_iterator<InIt, std::forward_iterator_tag>::value &&
		detail::is_iterator<OutIt, std::forward_iterator_tag>::value
		), std::pair<InIt, OutIt> >::type operate_limited(InIt input_begin, InIt input_end, OutIt output_begin, OutIt output_end, BinOp binop)
	{
		typedef std::pair<InIt, OutIt> result_type;
		struct OperationGuard
		{
			result_type result;
			OutIt output_begin;
			BinOp binop;
			~OperationGuard()
			{
				while (this->result.second != this->output_begin)
				{
					binop.cleanup_single(this->result.second);
				}
			}
			OperationGuard(InIt &&input_begin, OutIt &&output_begin, BinOp &&binop)
				: result(std::forward<InIt>(input_begin), std::forward<OutIt>(output_begin)), output_begin(result.second), binop(std::forward<BinOp>(binop)) { }
		} result(std::forward<InIt>(input_begin), std::forward<OutIt>(output_begin), std::forward<BinOp>(binop));
		for (; result.result.first != input_end && result.result.second != output_end; )
		{
			result.binop.single(result.result.first, result.result.second);
		}
		result.output_begin = result.result.second;  // Commit the operation (disable rollback)
		return static_cast<result_type &&>(result.result);
	}

	template<class InIt, class OutIt, class BinOp>
	typename std::enable_if<(
		detail::is_iterator<InIt, std::forward_iterator_tag>::value &&
		detail::is_iterator<OutIt, std::forward_iterator_tag>::value
		), std::pair<InIt, OutIt> >::type operate_limited(InIt input_begin, InIt input_end, OutIt output_begin, OutIt output_end, BinOp binop)
	{
		{
			typename std::iterator_traits<InIt>::difference_type const d1 = std::distance<InIt>(input_begin, input_end);
			typename std::iterator_traits<OutIt>::difference_type const d2 = std::distance<OutIt>(output_begin, output_end);
			if (d2 < d1)
			{
				input_end = input_begin;
				std::advance<InIt>(input_end, static_cast<typename std::iterator_traits<InIt>::difference_type>(d2));
			}

			if (d1 < d2)
			{
				output_end = output_begin;
				std::advance<OutIt>(output_end, static_cast<typename std::iterator_traits<OutIt>::difference_type>(d1));
			}
		}
		std::pair<InIt, OutIt> result(std::forward<InIt>(input_end), std::forward<OutIt>(output_begin));
		result.second = binop.multiple(std::forward<InIt>(input_begin), result.first, std::forward<OutIt>(result.second));
		return result;
	}

	template<class InIt, class OutIt, class BinOp>
	typename std::enable_if<(
		detail::is_iterator<InIt, std::input_iterator_tag>::value &&
		(detail::is_iterator<OutIt, std::output_iterator_tag>::value || detail::is_iterator<OutIt, std::bidirectional_iterator_tag>::value)
		), void>::type partitioned_operate_limited(iterator_partitioner<InIt> &input_parts, iterator_partitioner<OutIt> &output_parts, BinOp binop)
	{
		while (!input_parts.empty() && !output_parts.empty())
		{
			std::pair<typename iterator_partitioner<InIt>::iterator, typename iterator_partitioner<OutIt>::iterator> limits = detail::operate_limited(
				input_parts.begin_front(), input_parts.end_front(),
				output_parts.begin_front(), output_parts.end_front(),
				binop);
			if (limits.first == input_parts.end_front())
			{
				input_parts.pop_front();
			}
			else
			{
				input_parts.begin_front(limits.first);
			}

			if (limits.second == output_parts.end_front())
			{
				output_parts.pop_front();
			}
			else
			{
				output_parts.begin_front(limits.second);
			}
		}
	}
}

template<class Pointer, bool =
	detail::is_vc_array_iterator<typename detail::convertible_array_iterator<Pointer>::type>::value
#if defined(_MSC_VER) && _MSC_VER < 1900  // Visual C++ 2013 doesn't support move semantics properly, causing ambiguities in operator= and overload resolution failures in std::_Wrap_alloc
	&& false
#endif
>
struct stationary_vector_pointer
{
	typedef stationary_vector_pointer this_type;
	typedef Pointer type;
	static type create(Pointer const &arr, size_t const size, size_t const index)
	{
		(void)size;
		return arr + static_cast<typename std::iterator_traits<Pointer>::difference_type>(index);
	}
};

template<class Pointer>
struct stationary_vector_pointer<Pointer, true> : detail::convertible_array_iterator<Pointer>::type
{
	typedef stationary_vector_pointer this_type;
	typedef this_type type;
	typedef detail::convertible_array_iterator<Pointer> create_base;
	typedef typename create_base::type base_type;

	template<unsigned char I>
	size_t member() const
	{
		return (*reinterpret_cast<size_t const (*)[2]>(reinterpret_cast<Pointer const *>(&static_cast<base_type const &>(*this)) + 1))[I];
	}

	size_t size() const
	{
		return this->member<0>();
	}

	size_t index() const
	{
		return this->member<1>();
	}

	Pointer base() const
	{
		return this->base_type::base();
	}

	static type create(Pointer const &arr, size_t const size, size_t const index)
	{
		type result(arr, size, index);
		return result;
	}

	typedef typename std::iterator_traits<Pointer>::difference_type difference_type;

	stationary_vector_pointer() : base_type() { }

	stationary_vector_pointer(Pointer const &arr, size_t const size, size_t const index)
		: base_type(create_base(arr - static_cast<difference_type>(index), size, index).operator base_type())
	{
	}

	template<class OtherPointer>
	stationary_vector_pointer(stationary_vector_pointer<OtherPointer> const &other, size_t = sizeof(unsigned char[!(sizeof(Pointer) - sizeof(OtherPointer))]))
		: base_type(create_base(Pointer(other.base() - static_cast<difference_type>(other.index())), other.size(), other.index())) { }
	operator Pointer() const
	{
		return this->base();
	}

	// Normally, checked iterators don't allow comparing across different arrays, but we allow that for equality comparisons
	template<class OtherPointer> bool operator==(stationary_vector_pointer<OtherPointer, true> const &other) const
	{
		return this->base() == other.base();
	}

	template<class OtherPointer> bool operator!=(stationary_vector_pointer<OtherPointer, true> const &other) const
	{
		return this->base() != other.base();
	}

	bool operator==(this_type const &other) const
	{
		return this->base() == other.base();
	}

	bool operator!=(this_type const &other) const
	{
		return !this->operator==(other);
	}

	this_type operator+ (difference_type d) const
	{
		this_type result(*this);
		result += d;
		return result;
	}

	this_type operator- (difference_type d) const
	{
		this_type result(*this);
		result -= d;
		return result;
	}

	this_type &operator-=(difference_type const &delta)
	{
		static_cast<base_type &>(*this) -= delta;
		return *this;
	}

	this_type &operator+=(difference_type const &delta)
	{
		static_cast<base_type &>(*this) += delta;
		return *this;
	}

	this_type operator--()
	{
		--static_cast<base_type &>(*this);
		return *this;
	}

	this_type operator++()
	{
		++static_cast<base_type &>(*this);
		return *this;
	}

	this_type &operator =(Pointer const &other)
	{
		return *this += other - *this;
	}

	difference_type operator- (this_type const &other) const
	{
		return static_cast<base_type const &>(*this) - static_cast<base_type const &>(other);
	}

	template<class OtherPointer>
	difference_type operator- (stationary_vector_pointer<OtherPointer> const &other) const
	{
		return this->base() - other.base();
	}
};

template<class Pointer, stationary_vector_growth GrowthMode = stationary_vector_growth_dynamic_preference, class Diff = typename std::iterator_traits<Pointer>::difference_type, class Size = size_t>
struct stationary_vector_payload;

template<class Pointer, stationary_vector_growth GrowthMode = stationary_vector_growth_dynamic_preference, class Diff = typename std::iterator_traits<Pointer>::difference_type, class Size = size_t>
struct stationary_vector_bucket;

template<class Pointer, stationary_vector_growth GrowthMode>
struct stationary_vector_bucket<Pointer, GrowthMode, void, void>
{
	typedef Pointer pointer;
	pointer _begin;
};

template<class Size>
struct stationary_vector_bucket<void, stationary_vector_growth_dynamic_preference, void, Size>
{
	typedef Size size_type;
	union
	{
		size_type _ibegin_and_markers;
#if defined(_MSC_VER)  // These are to aid the debugger visualizer; they're not used in the code
#pragma warning(push)
#pragma warning(disable: 4201)
		struct
		{
			size_type _markers : 1;
			size_type _ibegin : sizeof(size_type) * CHAR_BIT - 1;
		};
#pragma warning(pop)
#endif
	};
};

template<stationary_vector_growth GrowthMode, class Size>
struct stationary_vector_bucket<void, GrowthMode, void, Size>
{
	typedef Size size_type;
	union
	{
		size_type _ibegin_and_markers;
#if defined(_MSC_VER)  // These are to aid the debugger visualizer; they're not used in the code
#pragma warning(push)
#pragma warning(disable: 4201)
		size_type _ibegin;
#pragma warning(pop)
#endif
	};
};

template<class Pointer, stationary_vector_growth GrowthMode, class Diff, class Size>
struct stationary_vector_bucket : public stationary_vector_bucket<Pointer, GrowthMode, void, void>, public stationary_vector_bucket<void, GrowthMode, void, Size>
{
	typedef Pointer pointer;
	typedef Diff difference_type;
	typedef Size size_type;
	typedef stationary_vector_bucket this_type;

	struct ibegin_less
	{
		bool operator()(this_type const &a, size_type const b) const
		{
			return a.ibegin() < b;
		}

		bool operator()(this_type const volatile &a, size_type const b) const
		{
			return a.ibegin() < b;
		}
	};

	pointer items_begin() const
	{
		return this->_begin;
	}

	pointer items_begin() const volatile
	{
		return detail::atomic_traits_of<pointer>::load(&this->_begin);
	}

	pointer items_end() const
	{
		pointer result = this->items_begin();
		result += static_cast<difference_type>(this->bucket_size());
		return result;
	}

	pointer items_end() const volatile
	{
		pointer result = this->items_begin();
		result += static_cast<difference_type>(this->bucket_size());
		return result;
	}

	size_type ibegin() const
	{
		return this->_calc_ibegin_from_field(this->_ibegin_and_markers);
	}

	size_type ibegin() const volatile
	{
		return this->_calc_ibegin_from_field(detail::atomic_traits_of<size_type>::load(&this->_ibegin_and_markers));
	}

	size_type iend() const /* This requires information from the next (adjacent) instance in memory if the pointer is valid! */
	{
		pointer const p = this->items_begin();
		size_type const result = p ? (this + 1)->ibegin() : this->ibegin();
		assert(~result >= result);
		assert(result >= this->ibegin());
		return result;
	}

	size_type iend() const volatile
	{
		return (this + !!this->items_begin())->ibegin();
	}

	bool pow2_total_marker() const
	{
		stationary_vector_growth r = this_type::static_pow2_mode();
		return r == stationary_vector_growth_prefer_fast_iterators || (r == stationary_vector_growth_dynamic_preference && !!(this->_ibegin_and_markers & this->pow2_total_marker_mask()));
	}

	bool pow2_total_marker() volatile const
	{
		return const_cast<this_type const *>(this)->pow2_total_marker() /* TODO: How correct is this? Does it matter for the end() iterator? */;
	}

	void pow2_total_marker(bool value)
	{
		stationary_vector_growth r = this_type::static_pow2_mode();
		if (r != stationary_vector_growth_dynamic_preference)
		{
			value = false;
		}
		size_type const mask = this->pow2_total_marker_mask();
		this->_ibegin_and_markers = value ? (this->_ibegin_and_markers | mask) : (this->_ibegin_and_markers & ~mask);
	}

	size_type bucket_size() const
	{
		return this->iend() - this->ibegin();
	}

	size_type bucket_size() const volatile
	{
		return this->iend() - this->ibegin();
	}

	static size_type max_size()
	{
		return this_type::_calc_ibegin_from_field(~size_type());
	}

	static stationary_vector_growth static_pow2_mode()
	{
		return GrowthMode;
	}
private:  // Don't let outsiders create instances of a single bucket, since adjacent instances depend on each other in memory and this is bug-prone.
	stationary_vector_bucket() = default;

	static size_type _calc_ibegin_from_field(size_type field)
	{
		return this_type::static_pow2_mode() == stationary_vector_growth_dynamic_preference ? (field >> this_type::ibegin_shift()) : field;
	}

	static size_type pow2_total_marker_mask()
	{
		return this_type::static_pow2_mode() == stationary_vector_growth_dynamic_preference ? static_cast<size_type>(1) : size_type();
	}

	static unsigned char ibegin_shift()
	{
		return 1;
	}

	void items_begin(pointer const p)
	{
		this->_begin = p;
	};

	void ibegin(size_type const value)
	{
		unsigned char const shift = this_type::static_pow2_mode() == stationary_vector_growth_dynamic_preference ? this_type::ibegin_shift() : size_type();
		this->_ibegin_and_markers = (this->_ibegin_and_markers & (static_cast<size_type>(1) << shift)) | (value << shift);
	}

	void ibegin_add(size_type const value)
	{
		this->ibegin(this->ibegin() + value);
	}

	static this_type create(pointer const b, size_type const ib, bool const pow2_total_marker)
	{
		stationary_vector_bucket result = stationary_vector_bucket();
		result._ibegin_and_markers = size_type();  // Visual C++ 2013 initialization workaround
		result._begin = b;
		result.ibegin(ib);
		result.pow2_total_marker(pow2_total_marker);
		return result;
	}

	friend struct stationary_vector_payload<Pointer, GrowthMode, Diff, Size>;
};

template<class Pointer, stationary_vector_growth GrowthMode, class Diff, class Size>
struct stationary_vector_payload
{
	typedef stationary_vector_payload this_type;
	typedef typename std::remove_cv<Pointer>::type pointer;
	typedef Diff difference_type;
	typedef Size size_type;
	typedef stationary_vector_bucket<pointer, GrowthMode, Diff, Size> bucket_type;
#if defined(_MSC_VER) && defined(_DEBUG)
#pragma warning(push)
#if _MSC_VER >= 1900
#pragma warning(disable: 4582)  // constructor is not implicitly called
#endif
#endif
	~stationary_vector_payload()
	{
		size_t i = this->bucket_count();
		i += i + 1 < static_bucket_capacity;
		while (i > 0)
		{
			--i;
			this->destroy_bucket(i);
		}
		bool debug_build = detail::debug_build::value;
		if (debug_build)
		{
			assert(!this->members.constructed_buckets());
		}
	}

	explicit stationary_vector_payload() : members()
	{
		// Ensure we always have at least 1 valid bucket.
		// This is necessary to be able to detect past-the-end pointers without involving anything except a reference to the bucket (which we already have).
		unsigned char ensure_buckets_are_trivially_destructible[std::is_trivially_destructible<bucket_type>::value];
		(void)ensure_buckets_are_trivially_destructible;
		bool debug_build = detail::debug_build::value;
		if (debug_build)
		{
			std::uninitialized_fill_n(
				reinterpret_cast<unsigned char *>(&*this->_buckets.begin()),
				sizeof(*this->_buckets.begin()) * this->_buckets.size(),
				static_cast<unsigned char>(0xCD));
		}
		struct super_type
		{
			bucket_type bucket;
		} temp = { } /* ensure value-initialization no matter what -- this is a workaround for Visual C++ 2013 */;
		this->construct_bucket(0, temp.bucket);
	}
#if defined(_MSC_VER) && defined(_DEBUG)
#pragma warning(pop)
#endif
	enum { static_bucket_capacity = (sizeof(size_t) >= sizeof(unsigned long long) ? sizeof(unsigned int) + sizeof(unsigned short) : sizeof(size_t)) * CHAR_BIT };

	typedef bucket_type buckets_type[static_bucket_capacity];
	struct member_nbuckets_type
	{
		detail::atomic_traits_of<size_t>::atomic_type nbuckets;
		member_nbuckets_type() : nbuckets(0) { }
	};
	struct fake_debug_members_type : member_nbuckets_type
	{
		size_t constructed_buckets() const { return 0; }
		void constructed_buckets(size_t) { }
	};
	struct debug_members_type : member_nbuckets_type
	{
		size_t _constructed_buckets /* bit-vector */;
		debug_members_type() : _constructed_buckets(0) { }
		size_t constructed_buckets() const { return this->_constructed_buckets; }
		void constructed_buckets(size_t value) { this->_constructed_buckets = value; }
	};
	struct members_except_bucket_count_type : std::conditional<detail::debug_build::value, debug_members_type, fake_debug_members_type>::type
	{
	};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4623)  // default constructor could not be generated because a base class default constructor is inaccessible or deleted
#endif
	struct members_type
	{
		// We ultimately decided NOT use the allocator to construct and destroy these buckets.
		// This is to avoid the payload introducing a dependency on the allocator.
		typedef typename std::conditional<
			std::is_trivially_destructible<buckets_type>::value ||
			!detail::is_defined<std::aligned_storage<sizeof(buckets_type), std::alignment_of<buckets_type>::value> >::value,
			std::enable_if<true, buckets_type>,
			std::aligned_storage<sizeof(buckets_type), std::alignment_of<buckets_type>::value>
		>::type::type buckets_type_inner;
		buckets_type_inner _uninitialized_buckets;
		buckets_type &get()
		{
			return reinterpret_cast<buckets_type &>(this->_uninitialized_buckets);
		}

		buckets_type const &get() const
		{
			return reinterpret_cast<buckets_type const &>(this->_uninitialized_buckets);
		}

		buckets_type volatile &get() volatile
		{
			return reinterpret_cast<buckets_type volatile &>(this->_uninitialized_buckets);
		}

		buckets_type const volatile &get() const volatile
		{
			return reinterpret_cast<buckets_type const volatile &>(this->_uninitialized_buckets);
		}

		bucket_type &operator[](size_t const i)
		{
			return this->get()[i];
		}

		bucket_type const &operator[](size_t const i) const
		{
			return this->get()[i];
		}

		bucket_type volatile &operator[](size_t const i) volatile
		{
			return this->get()[i];
		}

		bucket_type const volatile &operator[](size_t const i) const volatile
		{
			return this->get()[i];
		}

		bucket_type *operator +(ptrdiff_t const i)
		{
			return this->get() + i;
		}

		bucket_type const *operator +(ptrdiff_t const i) const
		{
			return this->get() + i;
		}

		typedef typename detail::convertible_array_iterator<bucket_type *> bucket_pointer;
		size_t size() const
		{
			return sizeof(this->get()) / sizeof(*this->get());
		}

		typename bucket_pointer::type begin()
		{
			return bucket_pointer(this->get(), this->size(), 0);
		}

		typename bucket_pointer::type end()
		{
			return bucket_pointer(this->get(), this->size(), this->size());
		}
	};

#ifdef _MSC_VER
#pragma warning(pop)
#endif
	members_except_bucket_count_type members;
	members_type _buckets;
	size_t bucket_count() const
	{
		return this->members.nbuckets;
	}

	size_t bucket_count() const volatile
	{
		return this->members.nbuckets;
	}

	size_t constructed_bucket_count() const
	{
		return this->bucket_count() + 1;
	}

	members_type &buckets()
	{
		return this->_buckets;
	}

	members_type const &buckets() const
	{
		return this->_buckets;
	}

	members_type volatile &buckets() volatile
	{
		return this->_buckets;
	}

	members_type const volatile &buckets() const volatile
	{
		return this->_buckets;
	}

	bucket_type *bucket_address(size_t const i)
	{
		return &this->buckets()[i];
	} /* use this when the bucket might NOT have been constructed */
	bucket_type const *bucket_address(size_t const i) const
	{
		return &this->buckets()[i];
	}

	bucket_type volatile *bucket_address(size_t const i) volatile
	{
		return &this->buckets()[i];
	}

	bucket_type const volatile *bucket_address(size_t const i) const volatile
	{
		return &this->buckets()[i];
	}

	pointer bucket_begin(size_t const ibucket) const
	{
		return this->bucket(ibucket).items_begin();
	}

	pointer bucket_begin(size_t const ibucket) const volatile
	{
		return this->bucket(ibucket).items_begin();
	}

	pointer bucket_end(size_t const ibucket) const
	{
		return this->bucket(ibucket).items_end();
	}

	template<class BucketIndex>
	pointer unsafe_index(size_type const i, BucketIndex *const bucket_index = NULL) const
	{
		size_t ibucket = this->bucket_index(i);
		pointer result(this->bucket_begin(ibucket));
		result += static_cast<difference_type>(i - this->bucket_ibegin(ibucket));
		if (bucket_index)
		{
			*bucket_index = ibucket;
		}
		return result;
	}

	pointer unsafe_index(size_type const i) const
	{
		return this->unsafe_index(i, static_cast<size_t *>(NULL));
	}

	template<class BucketIndex>
	pointer unsafe_index(size_type const i, BucketIndex *const bucket_index) const volatile
	{
		size_t ibucket = this->bucket_index(i);
		pointer result(this->bucket_begin(ibucket));
		result += static_cast<difference_type>(i - this->bucket_ibegin(ibucket));
		if (bucket_index)
		{
			*bucket_index = ibucket;
		}
		return result;
	}

	pointer unsafe_index(size_type const i) const volatile
	{
		return this->unsafe_index(i, static_cast<size_t *>(NULL));
	}

	size_type bucket_ibegin(size_t const i) const
	{
		return this->bucket(i).ibegin();
	}

	size_type bucket_ibegin(size_t const i) const volatile
	{
		return this->bucket(i).ibegin();
	}

	size_type bucket_iend(size_t const i) const
	{
		return this->bucket(i).iend();
	}

	size_type bucket_iend(size_t const i) const volatile
	{
		return this->bucket(i).iend();
	}

	size_t bucket_index(size_t const i) const
	{
		return this_type::bucket_index<bucket_type const>(*this, i);
	}

	size_t bucket_index(size_t const i) const volatile
	{
		return this_type::bucket_index<bucket_type const volatile>(*this, i);
	}

	pointer alloc_begin(size_t const ibucket) const
	{
		return this->bucket_begin(ibucket);
	}

	pointer alloc_end(size_t const ibucket) const
	{
		return this->bucket_end(ibucket);
	}

	size_type alloc_size(size_t const ibucket) const
	{
		assert(ibucket < this->bucket_count());
		return this->bucket_size(ibucket);
	}

	size_type bucket_size(size_t const ibucket) const
	{
		return this->bucket(ibucket).bucket_size();
	}

	void expand_bucket_size(size_t const ibucket, size_type const new_size)
	{
		size_type const old_size = this->bucket_size(ibucket);
		assert(new_size >= old_size);
		bucket_type *const bucket = this->bucket_address(ibucket);
		assert(bucket->bucket_size() > 0);
		assert(!(bucket + 1)->items_begin());
		(bucket + 1)->ibegin_add(new_size - old_size);
	}

	void set_bucket(size_t const ibucket, pointer const bucket_begin, pointer const bucket_end, size_type const bucket_iend)
	{
		(void)bucket_end;
		assert(bucket_begin != bucket_end);
		bucket_type *const bucket = this->bucket_address(ibucket);
		size_t bucket_count = this->bucket_count();
		assert(ibucket <= bucket_count && "attempting to set a bucket before preceding ones have been constructed");
		assert(ibucket < bucket_count || !this->bucket_end(ibucket));
		if (bucket_count <= ibucket && ibucket + 1 < static_bucket_capacity)
		{
			// Construct the bucket AFTER the one we want to set, to ensure the end iterator can function correctly.
			this->construct_bucket(ibucket + 1, bucket_type::create(pointer(), bucket_iend, bucket->pow2_total_marker()));
		}
		assert(!bucket->items_begin() && bucket->ibegin() == bucket_iend - static_cast<size_type>(bucket_end - bucket_begin));
		bucket->items_begin(bucket_begin) /* only set the field that's being actually changing (the others should be untouched, for multithreading) */;
		if (ibucket + 1 >= bucket_count)
		{
			++this->members.nbuckets;
		}
	}

	void splice_buckets(this_type &&other)
	{
		size_t ibucket = this->bucket_count();
		size_type iend = this->bucket_iend(ibucket);
		for (size_t i = 0; i < other.bucket_count(); i++)
		{
			this->set_bucket(ibucket++, other.bucket_begin(i), other.bucket_end(i), iend + other.bucket_iend(i));
		}

		for (size_t i = other.bucket_count(); i != 0; --i)
		{
			other.pop_bucket();
		}
	}

	void pop_bucket()
	{
		size_t n = this->bucket_count();
		assert(n > 0);
		this->bucket(n - 1)._begin = pointer();
		if (n < static_bucket_capacity)
		{
			this->destroy_bucket(n);
		}
		--this->members.nbuckets;
	}

	void swap(this_type &other)
	{
		members_type &this_buckets = this->buckets(), &other_buckets = other.buckets();
		detail::uninitialized_swap_ranges(
			this_buckets.begin(), this_buckets.begin() + static_cast<ptrdiff_t>(this->constructed_bucket_count()),
			other_buckets.begin(), other_buckets.begin() + static_cast<ptrdiff_t>(other.constructed_bucket_count()));
		using std::swap;
		swap(this->members, other.members);
	}

	friend void swap(this_type &a, this_type &b)
	{
		return a.swap(b);
	}
	struct traits_type
	{
		enum
		{
			initial_bucket_size = static_cast<size_type>(1) << detail::log2<(
				stationary_vector_debugging<typename std::conditional<false, Pointer, std::nullptr_t>::type(nullptr)>::value ? static_cast<size_type>(1) :
				(sizeof(this_type) + sizeof(typename std::iterator_traits<pointer>::value_type) - 1) /
				sizeof(typename std::iterator_traits<pointer>::value_type) /* Performance optimization to allocate at least as much as the size of the vector */
				) * 2 - 1>::value
		};
	};
private:
	bucket_type &bucket(size_t const ibucket)
	{
		return *this->bucket_address(ibucket);
	}

	bucket_type const &bucket(size_t const ibucket) const
	{
		return *this->bucket_address(ibucket);
	}

	bucket_type volatile &bucket(size_t const ibucket) volatile
	{
		return *this->bucket_address(ibucket);
	}

	bucket_type const volatile &bucket(size_t const ibucket) const volatile
	{
		return *this->bucket_address(ibucket);
	}

	template<class Bucket, class Me>
	static size_t bucket_index_slow(Me &me, size_t const i)
	{
		size_t const nbuckets = me.bucket_count();
		size_t j;
		for (j = nbuckets; j; --j)
		{
			if (me.bucket_ibegin(j) <= i)
			{
				break;
			}

			if (i <= (static_cast<size_t>(1) << (nbuckets - j)))
			{
				if (!~i) /* Dummy condition (never executed) just to discourage inlining of this function and encourage inlining its caller */
				{
					return bucket_index_slow<Bucket, Me>(me, i ^ nbuckets);
				}
				Bucket *bucket0 = me.bucket_address(0), *b = bucket0;
				b = std::lower_bound(bucket0, me.bucket_address(nbuckets), i, typename bucket_type::ibegin_less());
				if (b->ibegin() > i)
				{
					--b;
				}
				j = static_cast<size_t>(b - bucket0);
				break;
			}
		}
		return j;
	}

	template<class Bucket, class Me>
	static size_t bucket_index(Me &me, size_t const i)
	{
		size_t j;
		if (me.bucket_address(0)->pow2_total_marker())
		{
			enum { initial_bucket_size = this_type::traits_type::initial_bucket_size, initial_bucket_size_log2 = detail::log2<initial_bucket_size>::value };

			assert(initial_bucket_size == (1U << initial_bucket_size_log2));
			unsigned int kp1;
			detail::bsr<size_t>(&kp1, ((i >> initial_bucket_size_log2) << 1) | 1);
			size_t const nbuckets = me.bucket_count();
			j = kp1 < nbuckets ? kp1 : nbuckets;
		}
		else
		{
			j = this_type::bucket_index_slow<Bucket, Me>(me, i);
		}
		return j;
	}

	void destroy_bucket(size_t const ibucket)
	{
		bool debug_build = detail::debug_build::value;
		if (debug_build)
		{
			assert((this->members.constructed_buckets() & (static_cast<size_t>(1) << ibucket)));
		}
		bucket_type *const bucket = this->bucket_address(ibucket);
		bucket->~bucket_type();
		if (debug_build)
		{
			this->members.constructed_buckets(this->members.constructed_buckets() & ~(static_cast<size_t>(1) << ibucket));
			std::uninitialized_fill_n(reinterpret_cast<unsigned char *>(bucket), sizeof(*bucket), static_cast<unsigned char>(0xCD));
		}
	}

	void construct_bucket(size_t const ibucket, bucket_type const &value)
	{
		bool debug_build = detail::debug_build::value;
		if (debug_build)
		{
			assert(!(this->members.constructed_buckets() & (static_cast<size_t>(1) << ibucket)));
		}
		::new(static_cast<void *>(this->bucket_address(ibucket))) bucket_type(value);
		if (debug_build)
		{
			this->members.constructed_buckets(this->members.constructed_buckets() | (static_cast<size_t>(1) << ibucket));
		}
	}
	stationary_vector_payload(this_type const &other);
	stationary_vector_payload(this_type &&other);
	this_type &operator =(this_type other);
};

template<class T, stationary_vector_growth GrowthMode = stationary_vector_growth_dynamic_preference, class Pointer = T *, class Reference = T &, class Bucket = stationary_vector_bucket<T *> >
struct stationary_vector_iterator
#if defined(_UTILITY_) && !defined(_CPPLIB_VER)
: std::iterator<
	std::random_access_iterator_tag,
	T,
	typename Bucket::difference_type,
	Pointer,
	Reference
>
#endif
{
	typedef stationary_vector_iterator this_type;
	typedef std::random_access_iterator_tag iterator_category;
	typedef T value_type;
	typedef Pointer pointer;
	typedef Reference reference;
	typedef typename detail::propagate_cv<T, Bucket>::type bucket_type;
	typedef typename bucket_type::difference_type difference_type;
	typedef typename bucket_type::size_type size_type;
	typedef bucket_type *bucket_pointer;
	stationary_vector_iterator() = default;
	explicit stationary_vector_iterator(bucket_pointer const b, pointer const p) : b(b), p(p) { }
	typedef typename stationary_vector_payload<pointer, GrowthMode, difference_type, size_type>::traits_type traits_type;
#if 1
	// No, we shouldn't have to define move & copy operations.
	// But MSVC seems to optimize MUCH better when we do (44% faster in one of my benchmarks), and Clang seems to generate slightly better code too (18%).
	// Why? I don't know. For MSVC, it might be related to SSE2, because handwritten operators inhibit the default usage of movdqu/movdqa (SSE2) on x64.
	// But it might be something else. I don't think that explains the issue for Clang either.
	stationary_vector_iterator(this_type const &other) noexcept(std::is_nothrow_copy_constructible<pointer>::value) : b(other.b), p(other.p) { }
	stationary_vector_iterator(this_type &&other) noexcept(std::is_nothrow_move_constructible<pointer>::value) : b(std::move(other.b)), p(std::move(other.p)) { }
	this_type &operator =(this_type const &other) noexcept(std::is_nothrow_copy_assignable<pointer>::value)
	{
		this->p = other.p;
		this->b = other.b;
		return *this;
	}
	this_type &operator =(this_type &&other) noexcept(std::is_nothrow_move_assignable<pointer>::value)
	{
		this->p = std::move(other.p);
		this->b = std::move(other.b);
		return *this;
	}
#endif
	template<class T2, stationary_vector_growth GM2, class P2, class R2>
	stationary_vector_iterator(stationary_vector_iterator<T2, GM2, P2, R2, Bucket> const &other, typename std::enable_if<
		std::is_convertible<T2 *, T *>::value &&
		std::is_convertible<P2, Pointer>::value &&
		std::is_convertible<R2, Reference>::value
		, std::nullptr_t>::type = nullptr) : this_type(other.b, other.p) { }
	void revalidate_after_insertion()
	{
		if (!this->p)
		{
			this->p = this->b->items_begin();
		}
		else if (this->p == this->b->items_end() && this->p != this->b->items_begin())
		{
			this->p = (++this->b)->items_begin();
		}
	}

	// TODO: Our const iterators point to mutable values! Fix this!
	pointer operator->() const
	{
		return this->p;
	}

	reference operator*() const
	{
		return *this->p;
	}

	reference operator[](difference_type const value) const
	{
		this_type i(*this);
		i += value;
		return *i;
	}

	template<class T2, stationary_vector_growth GM2, class P2, class R2> bool operator==(stationary_vector_iterator<T2, GM2, P2, R2, Bucket> const &other) const
	{
		return this->p == other.p;
	}

	template<class T2, stationary_vector_growth GM2, class P2, class R2> bool operator!=(stationary_vector_iterator<T2, GM2, P2, R2, Bucket> const &other) const
	{
		return !this->operator==(other);
	}

	template<class T2, stationary_vector_growth GM2, class P2, class R2> bool operator< (stationary_vector_iterator<T2, GM2, P2, R2, Bucket> const &other) const
	{
		return other.b && (!this->b || this->operator- (other) < difference_type());
	}

	template<class T2, stationary_vector_growth GM2, class P2, class R2> bool operator> (stationary_vector_iterator<T2, GM2, P2, R2, Bucket> const &other) const
	{
		return  other.operator< (*this);
	}

	template<class T2, stationary_vector_growth GM2, class P2, class R2> bool operator<=(stationary_vector_iterator<T2, GM2, P2, R2, Bucket> const &other) const
	{
		return !this->operator> (other);
	}

	template<class T2, stationary_vector_growth GM2, class P2, class R2> bool operator>=(stationary_vector_iterator<T2, GM2, P2, R2, Bucket> const &other) const
	{
		return !this->operator< (other);
	}

	this_type &operator+=(difference_type const &value)
	{
		bool assume_begin = bucket_type::static_pow2_mode() == stationary_vector_growth_prefer_fast_iterators;
		assume_begin &= this->b->ibegin() == 0;
		assume_begin &= this->p == this->b->items_begin();
		this->p = assume_begin
			? bucket_advance(const_cast<bucket_type const *&>(this->b), this->p, value, std::true_type())
			: bucket_advance(const_cast<bucket_type const *&>(this->b), this->p, value, std::false_type());
		return *this;
	}

	this_type &operator-=(difference_type const value)
	{
		return this->operator+=(-value);
	}

	this_type &operator++()
	{
		++this->p;
		if (this->p == this->b->items_end())
		{
			++this->b;
			this->p = this->b->items_begin();
		}
		return *this;
	}

	this_type &operator--()
	{
		if (this->p == this->b->items_begin())
		{
			--this->b;
			this->p = this->b->items_end();
		}
		--this->p;
		return *this;
	}

	this_type  operator++(int)
	{
		this_type self(*this);
		this->operator++();
		return self;
	}

	this_type  operator--(int)
	{
		this_type self(*this);
		this->operator--();
		return self;
	}

	this_type  operator+ (difference_type const value) const
	{
		this_type result(*this);
		result += value;
		return result;
	}

	this_type  operator- (difference_type const value) const
	{
		this_type result(*this);
		result -= value;
		return result;
	}

	friend this_type operator+ (difference_type const value, this_type const &me)
	{
		return me + value;
	}

	template<class T2, stationary_vector_growth GM2, class P2, class R2>
	difference_type operator- (stationary_vector_iterator<T2, GM2, P2, R2, Bucket> const other) const
	{
		assert(!!this->b == !!other.b);
		return
			(this->b ? static_cast<difference_type>(this->b->ibegin() + static_cast<size_type>(this->p - static_cast<pointer const &>(this->b->items_begin()))) : difference_type()) -
			(other.b ? static_cast<difference_type>(other.b->ibegin() + static_cast<size_type>(other.p - static_cast<pointer const &>(other.b->items_begin()))) : difference_type());
	}
	bucket_pointer b;
	pointer p;

	// We optimize the case of begin() + offset since it's quite common and somewhat cheaper to do.
	static pointer bucket_advance_pow2(bucket_type const *&bucket, pointer p, difference_type const &value, bool const assume_begin)
	{
		enum
		{
			initial_bucket_size = traits_type::initial_bucket_size, initial_bucket_size_log2 = detail::log2<initial_bucket_size>::value,
			initial_bucket_size_is_power_of_2 = initial_bucket_size == (1U << initial_bucket_size_log2),
		};

		bucket_type const *b = bucket;
		size_type bucket_ibegin = b->ibegin();
		assert(!(initial_bucket_size_is_power_of_2 ? bucket_ibegin & ((1U << initial_bucket_size_log2) - 1) : (bucket_ibegin % initial_bucket_size)));
		size_type i = assume_begin ? size_type() : bucket_ibegin + static_cast<size_type>(p - b->items_begin());
		size_type j = static_cast<size_type>(static_cast<difference_type>(i) + value);
		size_type arg1 = initial_bucket_size_is_power_of_2 ? i >> initial_bucket_size_log2 : (i / initial_bucket_size);
		size_type arg2 = initial_bucket_size_is_power_of_2 ? j >> initial_bucket_size_log2 : (j / initial_bucket_size);
		arg1 <<= 1;
		arg2 <<= 1;
		arg1 |= 1;
		arg2 |= 1;
		unsigned int ichunk_log2p1, jchunk_log2p1;
		if (assume_begin) { ichunk_log2p1 = 0; }
		else { detail::bsr(&ichunk_log2p1, arg1); }
		detail::bsr(&jchunk_log2p1, arg2);
		if (jchunk_log2p1 != ichunk_log2p1)
		{
			detail::fast_advance(b, static_cast<ptrdiff_t>(jchunk_log2p1) - static_cast<ptrdiff_t>(ichunk_log2p1));
			p = b->items_begin();
			i = b->ibegin();
		}
		detail::fast_advance(p, static_cast<difference_type>(j) - static_cast<difference_type>(i));
		assert(!(b &&p != b->items_begin() && p == b->items_end()) && "pointer must never point to the end of an array, but to the beginning of the next array");
		bucket = b;
		return p;
	}

	static pointer bucket_advance_nonpow2(bucket_type const *&bucket, pointer p, difference_type value)
	{
		bucket_type const *b = bucket;
		difference_type const zero = difference_type();
		if (value > zero)
		{
			while (value)
			{
				difference_type const remaining = static_cast<difference_type>(b->items_end() - p);
				bool const falls_short = value < remaining;
				difference_type const delta = falls_short ? value : remaining;
				p += delta;
				value -= delta;
				if (!falls_short)
				{
					p = (++b)->items_begin();
				}
			}
		}
		else if (value < zero)
		{
			while (value)
			{
				difference_type const remaining = static_cast<difference_type>(b->items_begin() - p);
				bool const falls_short = value > remaining;
				difference_type const delta = falls_short ? value : remaining;
				p += delta;
				value -= delta;
				if (!falls_short && value < zero)
				{
					p = (--b)->items_end();
				}
			}
		}
		assert(!(b &&p != b->items_begin() && p == b->items_end()) && "pointer must never point to the end of an array, but to the beginning of the next array");
		bucket = b;
		return p;
	}

	static pointer do_bucket_advance(bucket_type const *&b, pointer const &p, difference_type const &value, bool assume_begin)
	{
		return b->pow2_total_marker()
			? bucket_advance_pow2(b, p, value, assume_begin)
			: bucket_advance_nonpow2(b, p, value);
	}

protected:
	static pointer bucket_advance(bucket_type const *&b, pointer const &p, difference_type const &value, std::false_type assume_begin)
	{
		return do_bucket_advance(b, p, value, static_cast<bool>(assume_begin));
	}

	static pointer bucket_advance(bucket_type const *&b, pointer const &p, difference_type const &value, std::true_type assume_begin)
	{
		return do_bucket_advance(b, p, value, static_cast<bool>(assume_begin));
	}
};

namespace detail
{
	template<class T, stationary_vector_growth GrowthMode, class Pointer, class Reference, class Bucket>
	struct iterator_partitioner<stationary_vector_iterator<T, GrowthMode, Pointer, Reference, Bucket> >
	{
		typedef stationary_vector_iterator<T, GrowthMode, Pointer, Reference, Bucket> base_iterator;
		typedef typename base_iterator::pointer iterator;
		typedef typename std::iterator_traits<iterator>::difference_type difference_type;
		base_iterator _begin, _end;
		iterator_partitioner() = default;
		explicit iterator_partitioner(base_iterator const &begin, base_iterator const &end) : _begin(begin), _end(end) { }
		base_iterator begin() const
		{
			return this->_begin;
		}

		base_iterator end() const
		{
			return this->_end;
		}

		bool empty() const
		{
			return this->_begin == this->_end;
		}

		iterator begin_front() const
		{
			return this->_begin.p;
		}

		void begin_front(iterator const &value)
		{
			this->_begin.p = value;
		}

		iterator end_front() const
		{
			return this->_begin.b == this->_end.b ? this->_end.p : static_cast<iterator const &>(this->_begin.b->items_end());
		}

		difference_type front_distance() const
		{
			return this->end_front() - this->begin_front();
		}

		void pop_front()
		{
			this->_begin.p = this->_begin.b == this->_end.b ? this->_end.p : static_cast<iterator const &>((++this->_begin.b)->items_begin());
		}

		iterator begin_back() const
		{
			iterator result = this->_begin.p;
			if (this->_begin.b != this->_end.b)
			{
				if (!this->needs_decrement())
				{
					result = this->_end.b->items_begin();
				}
				else if (this->_begin.b != (this->_end.b - 1))
				{
					result = (this->_end.b - 1)->items_begin();
				}
			}
			return result;
		}

		iterator end_back() const
		{
			return this->_begin.b == this->_end.b || !this->needs_decrement() ? this->_end.p : (this->_end.b - 1)->items_end();
		}

		void end_back(iterator const &value)
		{
			if (value != this->_end.p)
			{
				this->_end.b -= this->needs_decrement();
				this->_end.p = value;
			}
		}

		difference_type back_distance() const
		{
			return this->end_back() - this->begin_back();
		}

		void pop_back()
		{
			if (this->_begin.b == this->_end.b)
			{
				this->_end.p = this->_begin.p;
			}
			else
			{
				if (this->needs_decrement())
				{
					--this->_end.b;
					this->_end.p = iterator();
				}
				this->_end.p = this->_begin.b == this->_end.b ? this->_begin.p : this->_end.b->items_begin();
				assert(this->_end.b->items_begin() <= this->_end.p && this->_end.p <= this->_end.b->items_end());
			}
		}
	private:
		bool needs_decrement() const
		{
			return !this->_end.b->items_begin() || this->_end.b->items_begin() == this->_end.p;
		}
	};

	template<class Ax>
	struct payload_with_nonswapping_allocator : Ax
	{
		typedef payload_with_nonswapping_allocator this_type;
		typedef Ax allocator_type;
		payload_with_nonswapping_allocator() = default;
		explicit payload_with_nonswapping_allocator(allocator_type const &ax) : Ax(ax) { }
		explicit payload_with_nonswapping_allocator(allocator_type &&ax) : Ax(std::move(ax)) { }
		allocator_type &allocator()
		{
			return *this;
		}

		allocator_type const &allocator() const
		{
			return *this;
		}
		friend void swap(this_type &, this_type &) { }
	};

	template<class T, class Base = void>
	struct payload_with_inner : std::conditional<std::is_void<Base>::value, std::enable_if<true, void>, Base>::type
	{
		typedef payload_with_inner this_type;
		typedef typename std::conditional<std::is_void<Base>::value, std::enable_if<true, void>, Base>::type base_type;
		typedef T inner_type;
		inner_type inner;

		template<class... Args> explicit payload_with_inner(Args... args) : base_type(std::forward<Args>(args)...), inner() { }

		friend void swap(this_type &a, this_type &b)
		{
			using std::swap;
			swap(static_cast<Base &>(a), static_cast<Base &>(b));
			swap(a.inner, b.inner);
		}
	};

	template<class T, class Base = void>
	struct payload_with_size : std::conditional<std::is_void<Base>::value, std::enable_if<true, void>, Base>::type
	{
		typedef payload_with_size this_type;
		typedef typename std::conditional<std::is_void<Base>::value, std::enable_if<true, void>, Base>::type base_type;
		typedef T size_type;
		typename detail::atomic_traits_of<size_type>::atomic_type _size;

		template<class... Args>
		explicit payload_with_size(Args... args) : base_type(std::forward<Args>(args)...), _size(size_type()) { }

		friend void swap(this_type &a, this_type &b)
		{
			using std::swap;
			swap(static_cast<Base &>(a), static_cast<Base &>(b));
			swap(a._size, b._size);
		}

		size_type size() const
		{
			return this->_size;
		}

		size_type size() const volatile
		{
			return this->_size;
		}
	};

	template<class T, class Base = void>
	struct payload_without_end_pointer : std::conditional<std::is_void<Base>::value, std::enable_if<true, void>, Base>::type
	{
		typedef payload_without_end_pointer this_type;
		typedef typename std::conditional<std::is_void<Base>::value, std::enable_if<true, void>, Base>::type base_type;
		typedef T pointer;
		typedef size_t used_buckets_type;

		template<class... Args>
		explicit payload_without_end_pointer(Args... args) : base_type(std::forward<Args>(args)...) { }

		friend void swap(this_type &a, this_type &b)
		{
			using std::swap;
			swap(static_cast<Base &>(a), static_cast<Base &>(b));
		}

		used_buckets_type *used_buckets_address()
		{
			return NULL;
		}

		size_t used_buckets() const
		{
			return ~size_t();
		}
		void used_buckets(size_t const &) { }
		pointer end_ptr() const
		{
			return pointer();
		}
		void end_ptr(pointer const &) { }
	};

	template<class T, class Base = void>
	struct payload_with_end_pointer : payload_without_end_pointer<T, Base>
	{
		typedef payload_with_end_pointer this_type;
		typedef payload_without_end_pointer<T, Base> base_type;
		typedef typename base_type::pointer pointer;
		typedef typename detail::atomic_traits_of<size_t>::atomic_type used_buckets_type;
		used_buckets_type _used_buckets;
		pointer _endp;

		template<class... Args>
		explicit payload_with_end_pointer(Args... args) : base_type(std::forward<Args>(args)...), _used_buckets(size_t()), _endp() { }

		friend void swap(this_type &a, this_type &b)
		{
			using std::swap;
			swap(static_cast<Base &>(a), static_cast<Base &>(b));
			swap(a._used_buckets, b._used_buckets);
			swap(a._endp, b._endp);
		}

		used_buckets_type *used_buckets_address()
		{
			return &this->_used_buckets;
		}

		size_t used_buckets() const
		{
			return this->_used_buckets;
		}

		void used_buckets(size_t value)
		{
			this->_used_buckets = value;
		}

		pointer end_ptr() const
		{
			return this->_endp;
		}

		void end_ptr(pointer const &value)
		{
			this->_endp = value;
		}
	};

	template<class...>
	struct reversed_chain_inheritance;

	template<class... T>
	struct reversed_chain_inheritance<void, T...> : reversed_chain_inheritance<T...> { };

	template<>
	struct reversed_chain_inheritance<> : std::enable_if<true, reversed_chain_inheritance<> > { };

	template<class T>
	struct reversed_chain_inheritance<T> : std::enable_if<true, T> { };

	template<template<class>
	class Tmpl0, class... T>
	struct reversed_chain_inheritance<Tmpl0<void>, T...> : std::enable_if<true, Tmpl0<typename reversed_chain_inheritance<T...>::type>> { };

	template<template<class, class>
	class Tmpl0, class T0, class... T>
	struct reversed_chain_inheritance<Tmpl0<T0, void>, T...> : std::enable_if<true, Tmpl0<T0, typename reversed_chain_inheritance<T...>::type>> { };

}

template<class T, class Ax = std::allocator<T>, stationary_vector_growth GrowthMode = stationary_vector_growth_prefer_fast_iterators>
class stationary_vector
{
	// Some of the complicated logic here is to allow for the invalidation of the 'end' iterator when inserting elements (and removing elements, wherever we choose to auto-deallocate).
	typedef stationary_vector this_type;
	enum { is_nothrow_swappable_except_allocator = true };

	typedef typename std::conditional<std::is_same<typename std::remove_cv<Ax>::type, void>::value, std::allocator<T>, Ax>::type proposed_allocator_type;
	typedef allocator_optimizations<Ax> stationary_vector_optimizations_;
public:
	typedef typename std::conditional<
		!std::is_same<T, typename proposed_allocator_type::value_type>::value,
		typename std::allocator_traits<proposed_allocator_type>::template rebind_alloc<T>,
		Ax
	>::type allocator_type;
	typedef std::allocator_traits<allocator_type> allocator_traits;
	typedef T value_type;
	enum
	{
		use_cref_iterators = !stationary_vector_debugging<typename std::conditional<false, Ax, std::nullptr_t>::type(nullptr)>::value,
		fast_end_iterator = true,
	};
	typedef std::integral_constant<stationary_vector_growth, GrowthMode> allocation_growth_mode;
	typedef typename detail::extended_allocator_traits<allocator_type>::reference reference;
	typedef typename detail::extended_allocator_traits<allocator_type>::const_reference const_reference;
	typedef typename detail::extended_allocator_traits<allocator_type>::volatile_reference volatile_reference;
	typedef typename detail::extended_allocator_traits<allocator_type>::const_volatile_reference const_volatile_reference;
	typedef stationary_vector_pointer<typename allocator_traits::pointer> pointer_wrapper;
	typedef stationary_vector_pointer<typename allocator_traits::const_pointer> const_pointer_wrapper;
	typedef stationary_vector_pointer<typename detail::extended_allocator_traits<allocator_type>::volatile_pointer> volatile_pointer_wrapper;
	typedef stationary_vector_pointer<typename detail::extended_allocator_traits<allocator_type>::const_volatile_pointer> const_volatile_pointer_wrapper;
	typedef typename pointer_wrapper::type pointer;
	typedef typename const_pointer_wrapper::type const_pointer;
	typedef typename volatile_pointer_wrapper::type volatile_pointer;
	typedef typename const_volatile_pointer_wrapper::type const_volatile_pointer;
	typedef typename allocator_traits::difference_type difference_type;
	typedef typename allocator_traits::size_type size_type;
	typedef stationary_vector_payload<pointer, allocation_growth_mode::value, difference_type, size_type> payload_type;
	typedef typename payload_type::bucket_type bucket_type;
	typedef stationary_vector_iterator<value_type, allocation_growth_mode::value, pointer, reference, bucket_type const> iterator;
	typedef stationary_vector_iterator<value_type const, allocation_growth_mode::value, const_pointer, const_reference, bucket_type const> const_iterator;
	typedef stationary_vector_iterator<value_type volatile, allocation_growth_mode::value, volatile_pointer, volatile_reference, bucket_type const> multithreading_iterator;
	typedef stationary_vector_iterator<value_type const volatile, allocation_growth_mode::value, const_volatile_pointer, const_volatile_reference, bucket_type const> const_multithreading_iterator;
	typedef detail::iterator_partitioner<iterator> partitioner_type;
	typedef detail::iterator_partitioner<const_iterator> const_partitioner_type;

	typedef typename std::conditional<use_cref_iterators, iterator const &, iterator>::type iterator_or_cref;
	typedef typename std::conditional<use_cref_iterators, const_iterator const &, const_iterator>::type const_iterator_or_cref;
	typedef typename std::conditional<use_cref_iterators, multithreading_iterator const &, multithreading_iterator>::type multithreading_iterator_or_cref;
	typedef typename std::conditional<use_cref_iterators, const_multithreading_iterator const &, const_multithreading_iterator>::type const_multithreading_iterator_or_cref;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	~stationary_vector()
	{
		this->reset();
		assert(this->capacity() == 0);
	}

	stationary_vector(this_type const &other) : stationary_vector(
		static_cast<void const *>(NULL),
		static_cast<void const *>(NULL),
		static_cast<this_type const &>(other),
		static_cast<allocator_type const &>(allocator_traits::select_on_container_copy_construction(other.allocator()))
	)
	{
	}

	stationary_vector(this_type const &other, allocator_type const &ax) : stationary_vector(static_cast<void const *>(NULL), static_cast<void const *>(NULL), other, ax) { }

	stationary_vector(this_type &&other) noexcept(true) : stationary_vector(static_cast<void const *>(NULL), static_cast<void const *>(NULL), std::move(other.allocator()))
	{
		// We can't call the move-with-allocator overload due to the noexcept specification on this one being stronger
		this->do_init(std::move(other), std::integral_constant<int, 2>());
	}

	stationary_vector(this_type &&other, allocator_type const &ax) noexcept(detail::extended_allocator_traits<allocator_type>::is_always_equal::value) : stationary_vector(ax)
	{
		this->do_init(std::move(other), std::integral_constant<int, detail::extended_allocator_traits<allocator_type>::is_always_equal::value ? 2 : 1>());
	}

	stationary_vector() noexcept(std::is_nothrow_default_constructible<allocator_type>::value) : stationary_vector(allocator_type()) { }

	explicit stationary_vector(allocator_type const &ax) noexcept(true) : stationary_vector(static_cast<void const *>(NULL), static_cast<void const *>(NULL), ax) { }

	stationary_vector(std::initializer_list<value_type> items, allocator_type const &ax = allocator_type()) : stationary_vector(items.begin(), items.end(), ax) { }

	template<class It>
	explicit stationary_vector(
		It begin,
		typename std::enable_if<!std::is_same<typename std::decay<It>::type, this_type>::value, size_type>::type n,
		typename std::enable_if<detail::is_iterator<typename std::decay<It>::type, std::forward_iterator_tag>::value, allocator_type>::type const &ax = allocator_type()
	) : stationary_vector(ax)
	{
		It end(begin);
		std::advance<It>(end, static_cast<typename std::iterator_traits<It>::difference_type>(n));
		this->initialize<It>(std::forward<It>(begin), std::forward<It>(end));
	}

	template<class It>
	explicit stationary_vector(It begin, It end, typename std::enable_if<
		detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value,
		allocator_type>::type const &ax = allocator_type()) : stationary_vector(ax)
	{
		this->initialize<It>(std::forward<It>(begin), std::forward<It>(end));
	}

	explicit stationary_vector(size_type const count, T const &value, allocator_type const &ax = allocator_type()) : stationary_vector(ax)
	{
		this->resize(count, value);
	}

	explicit stationary_vector(size_type const count, allocator_type const &ax = allocator_type()) : stationary_vector(ax)
	{
		this->resize(count);
	}

	template<class It>
	typename std::enable_if<(
		detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value
		), void>::type append(It begin, It end)
	{
		bool const expand_requires_pow2 = this->pow2_growth();
		for (;;)
		{
			begin = this->append_up_to_reallocation<typename std::decay<It>::type>(std::forward<It>(begin), end);
			if (!expand_requires_pow2 || begin == end)
			{
				break;
			}
			assert(this->size() == this->capacity());
			this->reserve(this->size() + 1);
			assert(this->size() != this->capacity());
		}

		if (expand_requires_pow2)
		{
			assert(begin == end);
		}
		else if (begin != end)
		{
			assert(this->size() == this->capacity());
			bool is_forward_iterator = detail::is_iterator<typename std::decay<It>::type, std::forward_iterator_tag>::value;
			typename std::iterator_traits<It>::difference_type const d = std::distance(begin, is_forward_iterator ? end : begin);
			this_type temp(this->allocator());
			temp.reserve(temp.size() + (d ? static_cast<size_type>(d) : static_cast<size_type>(1)));
			assert(temp.payload().bucket_count() <= 1);
			assert(temp.capacity() - temp.size() >= static_cast<size_type>(d));
			temp.initialize<It>(begin, end);
			this->_splice_buckets(std::move(temp.payload()));
			this->outer.add_size(temp.outer.size());
			temp.outer.set_size(0);
			assert(temp.payload().bucket_count() == 0);
			begin = end;
		}
	}

	template<class It>
	typename std::enable_if<(
		detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value
		), void>::type append(It begin, It end, size_type reps)
	{
		if (reps && begin != end)
		{
			size_type const m = this->size();
			append_guard revert(this, m);
			this->append<It>(std::forward<It>(begin), std::forward<It>(end));
			--reps;
			this->_finish_append(this->size() - m, reps);
			revert.release();
		}
	}

	this_type &append(size_type reps, value_type const &value)
	{
		this->append<value_type const *>(std::addressof(value) + 0, std::addressof(value) + 1, reps);
		return *this;
	}

	template<class It>
	typename std::enable_if<(
		detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value
		), It>::type append_up_to_reallocation(It const &begin, It const &end)
	{
		iterator i = this->end();
		typedef detail::iterator_partitioner<iterator> Partitioner;
		detail::iterator_partitioner<It> input_parts(begin, end);
		struct GuardedOutputParts : Partitioner
		{
			allocator_type *ax;
			typename Partitioner::base_iterator base;
			explicit GuardedOutputParts(allocator_type &ax, typename Partitioner::base_iterator const &begin, typename Partitioner::base_iterator const &end)
				: Partitioner(begin, end), ax(std::addressof(ax)), base(begin) { }
			~GuardedOutputParts()
			{
				Partitioner reverse_partitioner(this->base, this->begin());
				while (!reverse_partitioner.empty())
				{
					reverse_partitioner.end_back(detail::destroy_preferably_in_reverse(reverse_partitioner.begin_back(), reverse_partitioner.end_back(), *this->ax));
				}
			}
		} output_parts(this->allocator(), i, this->uninitialized_end());
		detail::partitioned_operate_limited(input_parts, static_cast<Partitioner &>(output_parts), detail::uninitialized_copy_operation<allocator_type>(this->allocator()));
		this->outer.add_size(static_cast<size_type>(output_parts.begin() - i));
		output_parts.base = output_parts.begin();
		return std::move(input_parts).begin();
	}

	template<class It>
	typename std::enable_if<(
		detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value
		), void>::type initialize(It begin, It end)
	{
		assert(this->empty() && "initialize() may fail on non-empty due to aliasing issues; use assign() for potentially non-empty containers.");
		for (bool first_run = true; begin != end; first_run = false)
		{
			if (!first_run)
			{
				bool is_forward_iterator = detail::is_iterator<typename std::decay<It>::type, std::forward_iterator_tag>::value;
				typename std::iterator_traits<It>::difference_type d =
					is_forward_iterator ? std::distance(begin, end) : typename std::iterator_traits<It>::difference_type();
				this->reserve(this->size() + (d ? static_cast<size_type>(d) : static_cast<size_type>(1)));
			}
			begin = this->append_up_to_reallocation<typename std::decay<It>::type>(std::forward<It>(begin), end);
			assert(begin == end || this->size() == this->capacity());
		}
	}

	template<class It>
	typename std::enable_if<(
		detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value
		), void>::type assign(It begin, It end)
	{
		detail::iterator_partitioner<It> input_parts(begin, end);
		detail::iterator_partitioner<iterator> output_parts(this->begin(), this->end());
		detail::partitioned_operate_limited(input_parts, output_parts, detail::copy_operation());
		this->pop_back_n(static_cast<size_type>(this->end() - output_parts.begin()));
		this->append<It>(std::move(input_parts).begin(), std::forward<It>(end));
	}

	void assign(std::initializer_list<value_type> value)
	{
		return this->assign<value_type const *>(value.begin(), value.end());
	}

	void assign(size_type n, value_type const &value)
	{
		size_type const m = this->size();
		iterator mid = this->begin();
		mid += static_cast<difference_type>(n < m ? n : m);
		std::fill(this->begin(), mid, value) /* PEF: std::fill() */;
		this->append<value_type const *>(std::addressof(value) + 0, std::addressof(value) + 1, n - (n < m ? n : m));
		this->pop_back_n(this->size() - n);
	}

	reference at(size_type const i)
	{
		this->check_index(i);
		return this->operator[](i);
	}

	const_reference at(size_type const i) const
	{
		this->check_index(i);
		return this->operator[](i);
	}

	reference back() noexcept(true)
	{
		return this->operator[](this->size() - 1);
	}

	const_reference back() const noexcept(true)
	{
		return this->operator[](this->size() - 1);
	}

	iterator begin()
	{
		payload_type &payload = this->payload();
		iterator result(payload.bucket_address(size_t()), payload.bucket_begin(size_t()));
		return result;
	}

	const_iterator begin() const
	{
		payload_type const &payload = this->payload();
		const_iterator result(payload.bucket_address(size_t()), payload.bucket_begin(size_t()));
		return result;
	}

	multithreading_iterator begin() volatile
	{
		payload_type volatile &payload = this->payload();
		multithreading_iterator result(const_cast<bucket_type *>(payload.bucket_address(size_t())), payload.bucket_begin(size_t()));
		return result;
	}

	const_multithreading_iterator begin() const volatile
	{
		payload_type const volatile &payload = this->payload();
		const_multithreading_iterator result(const_cast<bucket_type const *>(payload.bucket_address(size_t())), payload.bucket_begin(size_t()));
		return result;
	}

	pointer begin_ptr()
	{
		return this->begin().p;
	}

	const_pointer begin_ptr() const
	{
		return this->begin().p;
	}

	size_type capacity() const
	{
		payload_type const &payload = this->payload();
		return payload.bucket_ibegin(payload.bucket_count());
	}

	const_iterator cbegin() const
	{
		return this->begin();
	}

	const_iterator cend() const
	{
		return this->end();
	}

	void clear() noexcept(true)
	{
		this->pop_back_n(this->size());
	}

	const_reverse_iterator crbegin() const
	{
		return this->rbegin();
	}

	const_reverse_iterator crend() const
	{
		return this->rend();
	}

#if !defined(STATIONARY_VECTOR_NO_DEPRECATE)
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(deprecated)
	[[deprecated("data() is deprecated as elements are not contiguous")]]
#endif
#endif
#endif
	value_type *data()
	{
		return detail::extended_allocator_traits<allocator_type>::ptr_address(this->allocator(), this->begin_ptr());
	}

#if !defined(STATIONARY_VECTOR_NO_DEPRECATE)
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(deprecated)
	[[deprecated("data() is deprecated as elements are not contiguous")]]
#endif
#endif
#endif
	value_type const *data() const
	{
		return detail::extended_allocator_traits<allocator_type>::ptr_address(this->allocator(), this->begin_ptr());
	}

	template<class... Args>
	iterator emplace(const_iterator_or_cref pos, Args &&... args)
	{
		return this->emplace<Args...>(this->mutable_iterator(pos), std::forward<Args &&>(args)...);
	}

	template<class... Args>
	iterator emplace(iterator pos, Args &&... args)
	{
		if (this->is_at_end(pos))
		{
			this->emplace_back<Args...>(std::forward<Args>(args)...);
			pos = this->end();
			--pos;
		}
		else  // We have at least 1 element, so we can move-construct it into the last slot
		{
			detail::allocator_temporary<allocator_type> temp_alloc(this->allocator(), std::forward<Args>(args)...);
			pos = this->_finish_insert(pos, std::move(temp_alloc.get()));
		}
		return pos;
	}

	template<class... Args>
	reference emplace_back(Args &&... args)
	{
		payload_type const &payload = this->payload();
		size_type const size = this->size(), new_size = size + 1;
		if (this->capacity() == size)
		{
			this->reserve(new_size);
		}
		size_type const used_buckets = static_cast<size_type>(this->outer.used_buckets());
		allocator_type &ax = this->allocator();
		pointer p = this->end_ptr();
		allocator_traits::construct(ax, detail::extended_allocator_traits<allocator_type>::ptr_address(ax, p), std::forward<Args>(args)...);
		assert(this->outer.size() == size);
		pointer pnext = p;
		++pnext;
		bool fast_end_iterator = !!this_type::fast_end_iterator;
		if (fast_end_iterator)
		{
			bucket_type const *b = payload.bucket_address(used_buckets);
			if (pnext == b->items_end())
			{
				++b;
				pnext = b->items_begin();
			}
			this->outer.update_all(new_size, static_cast<size_t>(b - payload.bucket_address(0)), pnext);
			assert(this->end() == iterator(b, pnext));
		}
		else
		{
			this->outer.set_size(new_size);
		}
		return *p;
	}

	void emplace_back_n(size_type const count)
		// NOTE: This function has no variadic counterpart as it is optimized for cases where an allocator-aware version of std::uninitialized_value_construct can be called.
	{
		size_type const initial_size = this->size();
		size_type m = initial_size, n = count;
		append_guard revert(this, m);
		if (this->capacity() < m + n)
		{
			this->reserve(m + n);
		}
		iterator i = this->end();
		while (n)
		{
			pointer p_end = i.b->items_end();
			size_type delta = static_cast<size_type>(p_end - i.p);
			if (p_end - i.p > static_cast<difference_type>(n))
			{
				p_end = i.p;
				p_end += static_cast<difference_type>(n);
				delta = n;
			}
			assert(delta == static_cast<size_type>(p_end - i.p));
			detail::uninitialized_value_construct(i.p, delta, this->allocator());
			m += delta;
			n -= delta;
			if (!n)
			{
				i.p = p_end;
				break;
			}
			++i.b;
			i.p = i.b->items_begin();
		}
		assert(m == initial_size + count);
		if (i.p == i.b->items_end())
		{
			++i.b;
			i.p = i.b->items_begin();
		}
		this->outer.update_all(m, static_cast<size_t>(i.b - this->payload().bucket_address(0)), i.p);
		assert(static_cast<size_type>(this->end() - this->begin()) == initial_size + count);
		revert.release();
	}

	iterator uninitialized_end()
	{
		const_iterator j = static_cast<this_type const *>(this)->uninitialized_end();
		return this->mutable_iterator(j);
	}

	const_iterator uninitialized_end() const
	{
		payload_type const &payload = this->payload();
		size_type const capacity = this->capacity();
		(void)capacity;
		const_iterator result(payload.bucket_address(payload.bucket_count()), const_pointer());
		assert(this->index_of(result) == capacity && "uninitialized_end() iterator positioning implemented incorrectly");
		assert(result == this->nth(capacity));
		assert(static_cast<size_type>(result - this->end()) == capacity - this->size());
		return result;
	}

	bool empty() const noexcept(true)
	{
		return !this->size();
	}

	bool empty() const volatile noexcept(true)
	{
		return !this->size();
	}

	iterator end()
	{
		const_iterator j = static_cast<this_type const *>(this)->end();
		return this->mutable_iterator(j);
	}

	const_iterator end() const
	{
		bool fast_end_iterator = !!this_type::fast_end_iterator;
		if (fast_end_iterator)
		{
			bucket_type const *b;
			const_pointer p = this->end_ptr(&b);
			const_iterator result(b, p);
			assert(this->index_of(result) == this->size() && "end() iterator positioning implemented incorrectly");
			assert(result == this->nth(this->size()));
			return result;
		}
		else
		{
			return this->nth(this->size());
		}
	}

	pointer end_ptr(bucket_type **b = NULL)
	{
		bool fast_end_iterator = !!this_type::fast_end_iterator;
		if (fast_end_iterator)
		{
			return this->mutable_pointer(static_cast<this_type const *>(this)->end_ptr(const_cast<bucket_type const **>(b)));
		}
		else
		{
			return this->end().p;
		}
	}

	const_pointer end_ptr(bucket_type const **b = NULL) const
	{
		bool fast_end_iterator = !!this_type::fast_end_iterator;
		if (fast_end_iterator)
		{
			struct Helper  // The slow implementation here ensures the cached implementation is updated properly
			{
				static pointer slow_version(this_type const &me, bucket_type const **b = NULL)
				{
					payload_type const &payload = me.payload();
					size_type const i = me.size();
					size_t bi = me.outer.used_buckets();
					pointer p(payload.bucket_begin(bi));
					p += static_cast<difference_type>(i - payload.bucket_ibegin(bi));
					if (b)
					{
						*b = payload.bucket_address(bi);
					}
					return p;
				}
			};

			pointer p = this->outer.end_ptr();
			if (b)
			{
				*b = this->payload().bucket_address(this->outer.used_buckets());
			}
			assert(p == Helper::slow_version(*this, b));
			return p;
		}
		else
		{
			return this->end().p;
		}
	}

	iterator erase(const_iterator_or_cref begin, const_iterator_or_cref end)
	{
		return this->erase(this->mutable_iterator(begin), this->mutable_iterator(end));
	}

	iterator erase(const_iterator_or_cref pos)
	{
		return this->erase(this->mutable_iterator(pos));
	}

	iterator erase(iterator_or_cref pos)
	{
		iterator end(pos);
		++end;
		return this->erase(pos, end);
	}

	iterator erase(iterator begin, iterator end)
	{
		if (difference_type const d = end - begin)
		{
			bool const is_end = this->is_at_end(end);
			iterator my_end = this->end(), my_new_end = my_end;
			my_new_end -= d;
			detail::iterator_partitioner<iterator> input_parts(end, my_end), output_parts(begin, my_new_end);
			detail::partitioned_operate_limited(input_parts, output_parts, detail::move_operation());
			this->pop_back_n(static_cast<size_type>(d));
			if (is_end)
			{
				begin = this->end();
			}
			end = begin;
		}
		return begin;
	}

	bool pow2_growth() const
	{
		payload_type const &payload = this->payload();
		size_t const nbuckets = payload.bucket_count();
		bucket_type const *const bucket = payload.bucket_address(nbuckets);
		return bucket->pow2_total_marker();
	}

	void pow2_growth(bool const value) /* Only valid when the capacity is zero! */
	{
		payload_type &payload = this->payload();
		size_t const nbuckets = payload.bucket_count();
		assert(nbuckets == 0);
		bucket_type *const bucket = payload.bucket_address(nbuckets);
		bucket->pow2_total_marker(value);
	}

	reference front() noexcept(true)
	{
		return *this->begin_ptr();
	}

	const_reference front() const noexcept(true)
	{
		return *this->begin_ptr();
	}

	allocator_type get_allocator() const
	{
		return this->allocator();
	}

	size_type index_of(const_iterator_or_cref pos) const
	{
		return static_cast<size_type>(pos - this->begin());
	}

	size_type index_of(const_multithreading_iterator_or_cref pos) const volatile
	{
		return static_cast<size_type>(pos - this->begin());
	}

	bool is_at_begin(const_iterator_or_cref i) const
	{
		return i.p == this->begin_ptr();
	}

	bool is_at_end(const_iterator_or_cref i) const
	{
		return i.p == this->end_ptr();
	}

	template<class It>
	typename std::enable_if<detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value, iterator>::type insert(iterator pos, It begin, It end)
	{
		this->do_insert<It, std::false_type>(pos, std::forward<It>(begin), std::forward<It>(end), std::false_type());
		return pos;
	}

	template<class It>
	typename std::enable_if<detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value, iterator>::type insert(iterator pos, It begin, It end, size_type reps)
	{
		this->do_insert<It, size_type>(pos, std::forward<It>(begin), std::forward<It>(end), reps);
		return pos;
	}

	template<class It>
	typename std::enable_if<detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value, iterator>::type insert(const_iterator_or_cref pos, It begin, It end)
	{
		return this->insert<It>(this->mutable_iterator(pos), std::forward<It>(begin), std::forward<It>(end));
	}

	template<class It>
	typename std::enable_if<detail::is_iterator<typename std::decay<It>::type, std::input_iterator_tag>::value, iterator>::type insert(const_iterator_or_cref pos, It begin, It end, size_type const reps)
	{
		return this->insert<It>(this->mutable_iterator(pos), std::forward<It>(begin), std::forward<It>(end), reps);
	}

	iterator insert(const_iterator_or_cref pos, const_iterator_or_cref begin, const_iterator_or_cref end)
	{
		return this->insert<const_iterator>(pos, begin, end);
	}

	iterator insert(const_iterator_or_cref pos, const_iterator_or_cref begin, const_iterator_or_cref end, size_type const reps)
	{
		return this->insert<const_iterator>(pos, begin, end, reps);
	}

	iterator insert(const_iterator_or_cref pos, value_type const &value)
	{
		return this->emplace<value_type const &>(pos, value);
	}

	iterator insert(const_iterator_or_cref pos, size_type const reps, value_type const &value)
	{
		return this->insert<value_type const *>(pos, std::addressof(value) + 0, std::addressof(value) + 1, reps);
	}

	iterator insert(const_iterator_or_cref pos, value_type &&value)
	{
		return this->emplace<value_type &&>(pos, std::move(value));
	}

	iterator insert(const_iterator_or_cref pos, std::initializer_list<value_type> ilist)
	{
		return this->insert<value_type const *>(pos, ilist.begin(), ilist.end());
	}

	size_type max_size() const
	{
		return (std::min)(
			static_cast<size_type>(allocator_traits::max_size(this->allocator())),
			static_cast<size_type>((std::min)(static_cast<size_type>((std::numeric_limits<difference_type>::max)()), static_cast<size_type>(bucket_type::max_size()))));
	}

	iterator nth(size_type const i)
	{
		const_iterator j = static_cast<this_type const *>(this)->nth(i);
		return this->mutable_iterator(j);
	}

	const_iterator nth(size_type const i) const
	{
		/* COPY of below (volatile) -- keep in sync! */
		payload_type const &payload = this->payload();
		if (bucket_type::static_pow2_mode() != stationary_vector_growth_prefer_fast_iterators)
		{
			bucket_type const *const bucket = payload.bucket_address(0);
			if (i < bucket->iend())  // Fast path -- optimization for 1 bucket
			{
				const_iterator result0(bucket, bucket->items_begin() + static_cast<ptrdiff_t>(i));
				return result0;
			}
		}
		size_t b;
		pointer const p = payload.unsafe_index(i, &b);
		const_iterator result(payload.bucket_address(b), p);
		assert(this->index_of(result) == i && "iterator positioning implemented incorrectly");
		return result;
	}

	const_multithreading_iterator nth(size_type const i) const volatile
	{
		/* COPY of above (non-volatile) -- keep in sync! */
		payload_type const volatile &payload = this->payload();
		if (bucket_type::static_pow2_mode() != stationary_vector_growth_prefer_fast_iterators)
		{
			bucket_type const volatile *const bucket = payload.bucket_address(0);
			if (i < bucket->iend())  // Fast path -- optimization for 1 bucket
			{
				const_multithreading_iterator result0(bucket, bucket->items_begin() + static_cast<ptrdiff_t>(i));
				return result0;
			}
		}
		size_t b;
		pointer const p = payload.unsafe_index(i, &b);
		const_multithreading_iterator result(payload.bucket_address(b), p);
		/* assert(this->index_of(result) == i && "iterator positioning implemented incorrectly"); */
		return result;
	}

	pointer nth_ptr(size_type const i)
	{
		return this->payload().unsafe_index(i);
	}

	const_pointer nth_ptr(size_type const i) const
	{
		return this->payload().unsafe_index(i);
	}

	volatile_pointer nth_ptr(size_type const i) volatile
	{
		return this->payload().unsafe_index(i);
	}

	const_volatile_pointer nth_ptr(size_type const i) const volatile
	{
		return this->payload().unsafe_index(i);
	}
	friend
#ifdef __cpp_lib_three_way_comparison
		auto operator<=>
#else
		bool operator<
#endif
		(this_type const &a, this_type const &b)
	{
		// PERF: Overload std::lexicographical_compare so that this works for sub-ranges too.
		typedef detail::iterator_partitioner<const_iterator> Partitioner;
		Partitioner a_parts(a.begin(), a.end());
		Partitioner b_parts(b.begin(), b.end());
		for (;;)
		{
			difference_type const a_part_size = a_parts.front_distance();
			difference_type const b_part_size = b_parts.front_distance();
			difference_type const part_size = b_part_size < a_part_size ? b_part_size : a_part_size;
			if (!part_size)
			{
				assert(a_parts.empty() || b_parts.empty());
#ifdef __cpp_lib_three_way_comparison
				auto
#else
				bool
#endif
					r = a_part_size
#ifdef __cpp_lib_three_way_comparison
					<=>
#else
					<
#endif
					b_part_size;
#ifdef __cpp_lib_three_way_comparison
				assert(r == std::lexicographical_compare_three_way(a.begin(), a.end(), b.begin(), b.end()));
#else
				assert(r == std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()));
#endif
				return r;
			}
			typename Partitioner::iterator i1 = a_parts.begin_front(), j1 = i1;
			j1 += part_size;
			typename Partitioner::iterator i2 = b_parts.begin_front(), j2 = i2;
			j2 += part_size;
#if __cpp_lib_three_way_comparison
			auto result = std::lexicographical_compare_three_way(i1, j1, i2, j2);
			if (result != 0 || !part_size)
			{
				assert(result == std::lexicographical_compare_three_way(a.begin(), a.end(), b.begin(), b.end()));
				return result;
			}
#else
			bool result = std::lexicographical_compare(i1, j1, i2, j2);
			if (result || std::lexicographical_compare(i2, j2, i1, j1) || !part_size)
			{
				assert(result == std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()));
				return result;
			}
#endif
			a_parts.begin_front(j1);
			b_parts.begin_front(j2);
			if (part_size == a_part_size)
			{
				a_parts.pop_front();
			}

			if (part_size == b_part_size)
			{
				b_parts.pop_front();
			}
		}
	}
#ifndef __cpp_lib_three_way_comparison
	// We make these friend functions so that forced template instantiation doesn't instantiate them. (Otherwise they might not compile..)
	friend bool operator> (this_type const &a, this_type const &b)
	{
		return (b < a);
	}

	friend bool operator<=(this_type const &a, this_type const &b)
	{
		return !(a > b);
	}

	friend bool operator>=(this_type const &a, this_type const &b)
	{
		return !(a < b);
	}

	friend bool operator!=(this_type const &a, this_type const &b)
	{
		return !(a == b);
	}
#endif
	friend bool operator==(this_type const &a, this_type const &b)
	{
		// PERF: Overload std::equal() (both overloads) so that this works for sub-ranges too.
		typedef detail::iterator_partitioner<const_iterator> Partitioner;
		bool result = a.size() == b.size();
		if (result)
		{
			Partitioner a_parts(a.begin(), a.end());
			Partitioner b_parts(b.begin(), b.end());
			for (;;)
			{
				difference_type const a_part_size = static_cast<difference_type>(a_parts.front_distance());
				difference_type const b_part_size = static_cast<difference_type>(b_parts.front_distance());
				difference_type const part_size = b_part_size < a_part_size ? b_part_size : a_part_size;
				if (!part_size)
				{
					assert(a_parts.empty() || b_parts.empty());
					result &= a_part_size == b_part_size;
					break;
				}
				typename Partitioner::iterator i1 = a_parts.begin_front(), j1 = i1;
				j1 += part_size;
				typename Partitioner::iterator i2 = b_parts.begin_front(), j2 = i2;
				j2 += part_size;
				result &= std::equal(i1, j1, i2);
				if (!result)
				{
					break;
				}
				a_parts.begin_front(j1);
				b_parts.begin_front(j2);
				if (part_size == a_part_size)
				{
					a_parts.pop_front();
				}

				if (part_size == b_part_size)
				{
					b_parts.pop_front();
				}
			}
		}
		assert(result == (a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin())));
		return result;
	}

	this_type &operator =(std::initializer_list<value_type> items)
	{
		this->assign(items.begin(), items.end());
		return *this;
	}

	this_type &operator =(this_type const &other)
	{
		if (this != &other)
		{
			bool propagate_allocator = allocator_traits::propagate_on_container_copy_assignment::value;
			if (propagate_allocator && this->allocator() != other.allocator())
			{
				this->reset();  // Clear before copying, to reduce memory usage
				this_type(other).swap_except_allocator(*this);
			}
			else
			{
				this->assign<const_iterator>(other.begin(), other.end());
			}

			if (propagate_allocator)
			{
				allocator_type temp(other.allocator());
				detail::swap_if<allocator_traits::propagate_on_container_copy_assignment::value, allocator_type>(temp, this->allocator());
			}
		}
		return *this;
	}

	this_type &operator =(this_type &&other) noexcept(allocator_traits::propagate_on_container_move_assignment::value || detail::extended_allocator_traits<allocator_type>::is_always_equal::value)
	{
		if (this != &other)
		{
			enum { Arg = detail::extended_allocator_traits<allocator_type>::is_always_equal::value ? 2 : (allocator_traits::propagate_on_container_move_assignment::value ? 1 : 0) };
			this->do_assign(std::move(other), typename std::integral_constant<int, Arg>::type());
		}
		return *this;
	}

	reference operator[](size_type const i) noexcept(true)
	{
		assert(i < this->size());
		return *this->nth_ptr(i);
	}

	const_reference operator[](size_type const i) const noexcept(true)
	{
		assert(i < this->size());
		return *this->nth_ptr(i);
	}

	volatile_reference operator[](size_type const i) volatile noexcept(true)
	{
		return *this->nth_ptr(i);
	}

	const_volatile_reference operator[](size_type const i) const volatile noexcept(true)
	{
		return *this->nth_ptr(i);
	}

	void pop_back()
	{
		allocator_type &ax = this->allocator();
		iterator i = this->end();
		--i;
		bool const consider_shrinking = i.p == i.b->items_begin();
		allocator_traits::destroy(ax, detail::extended_allocator_traits<allocator_type>::address(ax, *i));
		this->outer.subtract_size(1);
		if (consider_shrinking)
		{
			this->shrink_to_fit(true);
		}
	}

	void pop_back_n(size_type n)
	{
		assert(n <= this->size());
		allocator_type &ax = this->allocator();
		size_type const new_size = this->size() - n;
		for (iterator i = this->nth(new_size), end = this->end(); i != end; )
		{
			--end;
			++end.p;
			pointer p_begin = i.b == end.b ? i.p : end.b->items_begin();
			detail::destroy_preferably_in_reverse(p_begin, end.p, ax);
			end.p = p_begin;
		}
		this->outer.subtract_size(n);
		if (!this->pow2_growth())
		{
			this->shrink_to_fit(true);
		}
	}

	reference push_back(value_type const &value)
	{
		return this->emplace_back(value);
	}

	reference push_back(value_type &&value)
	{
		return this->emplace_back(std::move(value));
	}

	reverse_iterator rbegin()
	{
		return reverse_iterator(this->end());
	}

	const_reverse_iterator rbegin() const
	{
		return const_reverse_iterator(this->end());
	}

	reverse_iterator rend()
	{
		return reverse_iterator(this->begin());
	}

	const_reverse_iterator rend() const
	{
		return const_reverse_iterator(this->begin());
	}

	typedef typename payload_type::traits_type traits_type;

	void reserve(size_type const n)
	{
		if (n > this->max_size())
		{
			detail::throw_length_error("length too long");
		}

		while (n > this->capacity())
		{
			size_type desired_bucket_size = size_type();
			if (this->pow2_growth())
			{
				payload_type &payload = this->payload();
				if (size_type const m = static_cast<size_type>(payload.bucket_count()))
				{
					size_type const preceding_bucket_size = payload.bucket_size(m - 1);
					desired_bucket_size = preceding_bucket_size + (m > 1 ? preceding_bucket_size : size_type());
				}
				else
				{
					desired_bucket_size = traits_type::initial_bucket_size;
				}
			}
			else
			{
				payload_type const &payload = this->payload();
				size_type const m = static_cast<size_type>(payload.bucket_index(n));
				if (m >= payload.bucket_count() && n)
				{
					size_type
						preceding_bucket_size = m >= 1 ? payload.bucket_size(m - 1) : size_type(),
						desired_extra_size = n - payload.bucket_ibegin(m);
					size_type const min_size = !m ? traits_type::initial_bucket_size : size_type();
					if (desired_extra_size < min_size)
					{
						desired_extra_size = min_size;
					}
					desired_bucket_size = (m > 1 ? preceding_bucket_size : 0) + (desired_extra_size < preceding_bucket_size ? preceding_bucket_size : desired_extra_size);
				}
			}
#if (__cplusplus >= 202002L || defined(__cpp_if_constexpr)) && defined(_MSC_VER) && defined(_CPPLIB_VER)
			if constexpr (std::is_pointer<typename std::allocator_traits<Ax>::pointer>::value &&
				detail::uses_default_allocate_nohint<Ax>::value &&
				detail::uses_default_allocate_hint<Ax>::value &&
				detail::uses_default_deallocate<Ax>::value &&
				!(std::alignment_of<typename std::allocator_traits<Ax>::value_type>::value & (std::alignment_of<typename std::allocator_traits<Ax>::value_type>::value - 1)) &&
				(std::alignment_of<typename std::allocator_traits<Ax>::value_type>::value <= std::alignment_of<long double>::value))
			{
				if (allocator_optimizations<allocator_type>::can_assume_allocator_passthrough_to_malloc(this->allocator()) && desired_bucket_size)
				{
					payload_type &payload = this->payload();
					size_type const m = payload.bucket_count();
					pointer const prev_alloc_begin = payload.alloc_begin(m - !!m), prev_alloc_end = payload.alloc_end(m - !!m);
					if (static_cast<void *>(prev_alloc_begin))
					{
						size_t const
							old_known_byte_size = static_cast<size_t>(
								reinterpret_cast<unsigned char const volatile *>(prev_alloc_end) -
								reinterpret_cast<unsigned char const volatile *>(prev_alloc_begin)),
							new_known_byte_size = old_known_byte_size + desired_bucket_size * sizeof(value_type);
						if (new_known_byte_size < 1 << 12 /* std::_Big_allocation_threshold == 4096 in VC++ */)
						{
							assert(!(new_known_byte_size % sizeof(value_type)));
							if (_expand(static_cast<void *>(prev_alloc_begin), new_known_byte_size) == static_cast<void *>(prev_alloc_begin))
							{
								this->_expand_bucket_size(m - !!m, new_known_byte_size / sizeof(value_type));
								desired_bucket_size = size_type();
							}
						}
					}
				}
			}
#endif
			if (desired_bucket_size)
			{
				this->_push_bucket(desired_bucket_size);
			}
		}

		/* To reduce the need for multiplication and division, we should ensure power-of-2 growth implies power-of-2 capacity. */
		assert(!this->pow2_growth() || !(this->capacity() & (this->capacity() - 1)));
	}

	void resize(size_type const n)
	{
		size_type m = this->size();
		if (m < n)
		{
			this->emplace_back_n(n - m);
		}
		else
		{
			this->pop_back_n(m - n);
		}
	}

	void resize(size_type const n, value_type const &value)
	{
		size_type const m = this->size();
		if (m < n)
		{
			this->append(n - m, value);
		}
		else
		{
			this->pop_back_n(m - n);
		}
	}

	void shrink_to_fit()
	{
		return this->shrink_to_fit(false);
	}

	void shrink_to_fit(bool const advisory, size_t preferred_buckets = 0)
		/* Does NOT make elements contiguous! If you want that, move-construct + swap a fresh container with power-of-2 sizing mode disabled. */
	{
		payload_type &payload = this->payload();
		size_t ibucket;
		pointer const p = payload.unsafe_index(this->size(), &ibucket);
		size_t min_buckets = ibucket + (p != payload.bucket_begin(ibucket)) + advisory;
		if (min_buckets < preferred_buckets)
		{
			min_buckets = preferred_buckets;
		}

		for (allocator_type &ax = this->allocator(); payload.bucket_count() > min_buckets; )
		{
			size_t const nb = payload.bucket_count() - 1;
			allocator_traits::deallocate(ax, payload.alloc_begin(nb), payload.alloc_size(nb));
			this->_pop_bucket();
		}
	}

	size_type size() const noexcept(true)
	{
		return this->outer.size();
	}

	size_type size() const volatile noexcept(true)
	{
		return this->outer.size();
	}
	void swap(this_type &other)
		noexcept(allocator_traits::propagate_on_container_swap::value || noexcept(is_nothrow_swappable_except_allocator) || detail::extended_allocator_traits<allocator_type>::is_always_equal::value)
	{
		// Note: When propagate_on_container_swap is false, C++ requires allocators to be equal; otherwise, swapping is undefined behavior.
		return this->swap(other, std::integral_constant<bool, allocator_traits::propagate_on_container_swap::value>());
	}

	void swap(this_type &other, std::integral_constant<bool, true>) noexcept(true)
	{
		using std::swap;
		swap(this->outer.allocator(), other.outer.allocator());
		return this->swap(other, std::integral_constant<bool, false>());
	}

	void swap(this_type &other, std::integral_constant<bool, false>) noexcept(true)
	{
		this->swap_except_allocator(other);
	}

	friend void swap(this_type &a, this_type &b) noexcept(noexcept(std::declval<this_type &>().swap(std::declval<this_type &>())))
	{
		return a.swap(b);
	}

	bool __invariants() const  // Needed for libc++ tests
	{
		return true /* TODO: Add invariants */;
	}

protected:
#if defined(__cpp_lib_is_layout_compatible)
	typedef unsigned char ensure_layout_compatibility[std::is_layout_compatible<const_iterator, iterator>::value];
#endif
	static pointer address(const_iterator_or_cref i)
	{
		/* HACK: There's no legal way to un-const arbitrary pointers, but the alternative seems to be code duplication everywhere... */
		return *reinterpret_cast<pointer const *>(reinterpret_cast<uintptr_t>(std::addressof(static_cast<const_pointer const &>(i.p))));
	}

	static pointer mutable_pointer(const_pointer i)
	{
		/* HACK: There's no legal way to un-const arbitrary pointers, but the alternative seems to be code duplication everywhere... */
		return *reinterpret_cast<pointer const *>(reinterpret_cast<uintptr_t>(std::addressof(i)));
	}

	static iterator &mutable_iterator(const_iterator &i)
	{
		return const_cast<iterator &>(this_type::mutable_iterator(static_cast<const_iterator const &>(i)));
	}

	static iterator const &mutable_iterator(const_iterator const &i)
	{
		/* HACK: There's no legal way to un-const arbitrary pointers, but the alternative seems to be code duplication everywhere... */
		return *reinterpret_cast<iterator const *>(reinterpret_cast<uintptr_t>(std::addressof(i)));
	}

	void check_index(size_type const i) const
	{
		if (i >= this->size())
		{
			(void)std::vector<size_t>().at(i);
		}
	}
private:
	void _finish_append(size_type const d, size_type reps)
	{
		if (reps)
		{
			size_type extra_size = d;
			if (reps != 1)
			{
				extra_size *= reps;
			}
			this->reserve(this->size() + extra_size);
			bool first_run = false;
			iterator j = this->end();
			iterator i = j;
			i -= static_cast<difference_type>(d);
			while (reps)
			{
				assert(i != j);
				this->append_up_to_reallocation(i, j);
				if (first_run) /* on the first run, validate the iterator, since it'll have been invalidated */
				{
					j = i;
					i += static_cast<difference_type>(d);
					first_run = false;
				}
				--reps;
			}
		}
	}

	iterator _finish_insert(iterator pos, value_type &&unaliased_to_insert)
	{
		assert(!this->is_at_end(pos) && "This method is only intended for emplace() NOT at the end of the container. That implies the container was non-empty before insertion.");
		this->push_back(std::move(this->back()));
		// Now we have 2 elements
		// 'pos' is still guaranteed to be valid here, since it wasn't the end pointer.
		typedef detail::iterator_partitioner<iterator> Partitioner;
		Partitioner parts(pos, this->end() - 1);
		typename Partitioner::iterator t = parts.end_back();
		--t;
		parts.end_back(t);
		if (t == parts.begin_back())
		{
			parts.pop_back();
		}

		for (; !parts.empty(); parts.pop_back())
		{
			typename Partitioner::iterator begin_back = parts.begin_back(), end_back = parts.end_back(), r = end_back;
			*t = std::move(*--r);
			std::move_backward(begin_back, r, end_back);
			t = begin_back;
		}
		*pos = std::move(unaliased_to_insert);
		return pos;
	}

	void _expand_bucket_size(size_t const ibucket, size_type const new_size)
	{
		this->payload().expand_bucket_size(ibucket, new_size);
		this->outer.update_end_ptr();
	}

	void _pop_bucket()
	{
		this->payload().pop_bucket();
		this->outer.update_end_ptr();
	}

	void _push_bucket(size_type const desired_bucket_size)
	{
		payload_type &payload = this->payload();
		size_type const m = static_cast<size_type>(payload.bucket_count());
		assert(m == payload.bucket_index(this->size() + desired_bucket_size));
		allocator_type &ax = this->allocator();
		detail::heap_ptr<allocator_type> ptr(ax, desired_bucket_size, payload.alloc_end(m - !!m));
		pointer begin(pointer_wrapper::create(ptr.get(), desired_bucket_size, size_t()));
		pointer end(begin);
		end += static_cast<difference_type>(desired_bucket_size);
		payload.set_bucket(m, begin, end, payload.bucket_iend(m - !!m) + desired_bucket_size);
		assert(payload.bucket_size(m) == payload.bucket_iend(m) - payload.bucket_ibegin(m));
		ptr.release();
		this->outer.update_end_ptr();
	}

	void _splice_buckets(payload_type &&other)
	{
		this->payload().splice_buckets(std::move(other));
		this->outer.update_end_ptr();
	}

	void reset()
	{
		this->clear();
		this->shrink_to_fit(false);
	}

	void swap_except_allocator(this_type &other) noexcept(is_nothrow_swappable_except_allocator)
	{
		this->outer.swap_except_allocator(other.outer);
	}

	template<class It>
	void append(It &&begin, It &&end, std::false_type const &)
	{
		return this->append(begin, end);
	}

	void do_init(this_type &&other, std::integral_constant<int, 2>) noexcept(true)
	{
		this->swap_except_allocator(other);
	}

	void do_init(this_type &&other, std::integral_constant<int, 1>)
	{
		if (this->allocator() == other.allocator())
		{
			this->do_init(std::move(other), std::integral_constant<int, 2>());
		}
		else
		{
			typedef std::move_iterator<iterator> MoveIt;
			this->initialize<MoveIt>(MoveIt(other.begin()), MoveIt(other.end()));
		}
	}

	template<class It, class Reps>
	typename std::enable_if<std::is_same<Reps, size_type>::value || std::is_same<Reps, std::false_type>::value, void>::type do_insert(iterator &pos, It begin, It end, Reps reps)
	{
		if (begin != end)
		{
			size_type const m = this->size();
			bool const is_end = this->is_at_end(pos);
			this->append<typename std::conditional<std::is_same<Reps, std::false_type>::value, It const &, It>::type>(std::forward<It>(begin), std::forward<It>(end), std::forward<Reps>(reps));
			assert(((void)(end = It()), (void)(end = It()), true)) /* Make explicit that the iterators might invalidated at this point, because they might've pointed into this container. */;
			if (is_end)
			{
				pos.p = this->nth(m).p;
				assert(pos == this->nth(m));
			}
			else
			{
				if (this->size() - m == 1)
				{
					detail::allocator_temporary<allocator_type> temp_alloc(this->allocator(), std::move(this->back()));
					this->pop_back();
					pos = this->_finish_insert(pos, std::move(temp_alloc.get()));
				}
				else
				{
					// Only rotate when it's difficult to avoid, since it's inefficient.
					iterator mid = this->begin();
					mid += static_cast<difference_type>(m);
					detail::rotate(this->allocator(), pos, mid, this->end());
				}
			}
		}
	}

	void do_assign(this_type &&other, std::integral_constant<int, 2> const) noexcept(true)  // is_always_equal
	{
		assert(this != &other && "if (this != &other) must have already been checked");
		this->swap(other, std::integral_constant<bool, allocator_traits::propagate_on_container_move_assignment::value>());
		other.reset();
	}

	void do_assign(this_type &&other, std::integral_constant<int, 1> const) noexcept(std::is_nothrow_move_constructible<this_type>::value)
		// propagate_on_container_move_assignment
	{
		assert(this != &other && "if (this != &other) must have already been checked");
		other.swap(*this, std::true_type());
		other.reset();
		other.allocator() = this->allocator();
	}

	void do_assign(this_type &&other, std::integral_constant<int, 0> const)  // allocators stay where they are
	{
		assert(this != &other && "if (this != &other) must have already been checked");
		if (this->allocator() == other.allocator())
		{
			this->swap(other);
			other.reset();
		}
		else
		{
			typedef std::move_iterator<iterator> MoveIt;
			this->assign<MoveIt>(MoveIt(other.begin()), MoveIt(other.end()));
		}
	}

	typedef typename detail::reversed_chain_inheritance<
		detail::payload_with_size<size_type>,
		typename std::conditional<
			!!this_type::fast_end_iterator,
			detail::payload_with_end_pointer<pointer>,
			detail::payload_without_end_pointer<pointer>
		>::type,
		detail::payload_with_inner<payload_type>,
		detail::payload_with_nonswapping_allocator<allocator_type>
	>::type outer_payload_base_type;

	struct outer_payload_type : public outer_payload_base_type
	{
		typedef outer_payload_base_type base_type;
		typedef typename detail::atomic_traits_of<size_t>::atomic_type atomic_size_t;
		typedef typename detail::atomic_traits_of<size_type>::atomic_type atomic_size_type;

		outer_payload_type() = default;

		explicit outer_payload_type(allocator_type const &ax) : base_type(ax) { }

		void add_size(size_type d)
		{
			this->set_size(this->size() + d);
		}

		void subtract_size(size_type d)
		{
			this->set_size(this->size() - d);
		}

		void update_all(size_type const new_size, size_t const new_used_buckets, pointer new_endp)
		{
			(void)new_endp;
			(void)new_used_buckets;
			this->end_ptr(new_endp);
			this->used_buckets(new_used_buckets);
			this->_size = new_size;
		}

		void update_end_ptr()
		{
			if (typename outer_payload_base_type::used_buckets_type *const p = this->used_buckets_address())
			{
				this->end_ptr(this->inner.unsafe_index(this->size(), p));
			}
		}

		void set_size(size_type const value)
		{
			this->_size = value;
			this->update_end_ptr();
		}

		void swap_except_allocator(outer_payload_type &other)
		{
			using std::swap;
			swap(static_cast<base_type &>(*this), static_cast<base_type &>(other));
		}
	};

	class append_guard
	{
		append_guard(append_guard const &);
		append_guard &operator =(append_guard const &);
		this_type *me;
		size_type m;
		size_t old_nbuckets;
	public:
		explicit append_guard(this_type *me, size_type m) : me(me), m(m), old_nbuckets(me->payload().bucket_count()) { }
		~append_guard()
		{
			if (me)
			{
				me->pop_back_n(me->size() - m);
				me->shrink_to_fit(false, this->old_nbuckets);
				assert(me->payload().bucket_count() >= this->old_nbuckets);
			}
		}

		void release()
		{
			this->me = NULL;
		}
	};

	outer_payload_type outer;
	payload_type &payload()
	{
		return this->outer.inner;
	}

	payload_type const &payload() const
	{
		return this->outer.inner;
	}

	payload_type volatile &payload() volatile
	{
		return this->outer.inner;
	}

	payload_type const volatile &payload() const volatile
	{
		return this->outer.inner;
	}

	allocator_type &allocator()
	{
		return this->outer.allocator();
	}

	allocator_type const &allocator() const
	{
		return this->outer.allocator();
	}

	stationary_vector(void const *, void const *, this_type const &other, allocator_type const &ax) : stationary_vector(ax)
	{
		this->initialize<const_iterator>(other.begin(), other.end());
	}

	explicit stationary_vector(void const *, void const *, allocator_type const &ax) noexcept(true)
		: outer(ax)
	{
		if (bucket_type::static_pow2_mode() == stationary_vector_growth_dynamic_preference)
		{
			this->pow2_growth(true);
		}
	}
};

#if __cplusplus >= 202002L || defined(__cpp_deduction_guides)
template<class Ax> stationary_vector(Ax const &)->stationary_vector<typename std::allocator_traits<Ax>::value_type, Ax>;
template<class It> stationary_vector(It, It)->stationary_vector<typename std::iterator_traits<It>::value_type>;
template<class It, class Ax> stationary_vector(It, It, Ax const &)->stationary_vector<typename std::iterator_traits<It>::value_type, Ax>;
template<class T> stationary_vector(size_t, T)->stationary_vector<T>;
template<class T, class Ax> stationary_vector(std::initializer_list<T>, Ax)->stationary_vector<T, Ax>;
template<class Ax> stationary_vector(typename std::allocator_traits<Ax>::size_type, typename std::allocator_traits<Ax>::value_type, Ax)->stationary_vector<typename std::allocator_traits<Ax>::value_type, Ax>;
template<class Ax> stationary_vector(typename std::allocator_traits<Ax>::value_type, typename std::iterator_traits<Ax>::value_type, Ax const &)->stationary_vector<typename std::allocator_traits<Ax>::value_type, Ax>;
#endif

template<class T, class Ax, class U>
typename stationary_vector<T, Ax>::size_type erase(stationary_vector<T, Ax> &c, U const &value)
{
	// PERF: Optimize erase()
	typename stationary_vector<T, Ax>::iterator it = std::remove(c.begin(), c.end(), value);
	typename stationary_vector<T, Ax>::difference_type r = std::distance(it, c.end());
	c.erase(it, c.end());
	return static_cast<typename stationary_vector<T, Ax>::size_type>(r);
}

template<class T, class Ax, class Pred>
typename stationary_vector<T, Ax>::size_type erase_if(stationary_vector<T, Ax> &c, Pred pred)
{
	// PERF: Optimize erase_if()
	typename stationary_vector<T, Ax>::iterator it = std::remove_if(c.begin(), c.end(), std::forward<Pred>(pred));
	typename stationary_vector<T, Ax>::difference_type r = std::distance(it, c.end());
	c.erase(it, c.end());
	return static_cast<typename stationary_vector<T, Ax>::size_type>(r);
}

#if defined(STATIONARY_VECTOR_END_NAMESPACE)
STATIONARY_VECTOR_END_NAMESPACE
#else
}
}
#endif

#endif

