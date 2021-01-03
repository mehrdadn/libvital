#pragma once 

#ifndef STATIONARY_VECTOR_HPP
#define STATIONARY_VECTOR_HPP

#include <limits.h>  // CHAR_BIT

#ifndef assert
#include <cassert>
#endif

#if !defined(STATIONARY_VECTOR_ATOMICS) && (defined(STATIONARY_VECTOR_DEBUG) && ((2 * STATIONARY_VECTOR_DEBUG + 1) - 1))
#define STATIONARY_VECTOR_ATOMICS 1  // Untested
#endif

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
#include <memory>
#include <new>  // std::bad_alloc
#include <vector>

#if defined(_MSC_VER) && defined(_CPPLIB_VER)
#include <malloc.h>
#endif

#if defined(_CPPLIB_VER) && _CPPLIB_VER <= 650 && !defined(_MSVC_STL_VERSION)
namespace std { template <class> struct _Wrap_alloc; }
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
#ifdef _NOEXCEPT_OP
#define noexcept _NOEXCEPT_OP
#else
#define noexcept(X)
#endif
#endif

namespace stdext
{
	template <class> class checked_array_iterator;
	template <class> class unchecked_array_iterator;
}

#if defined(STATIONARY_VECTOR_BEGIN_NAMESPACE)
STATIONARY_VECTOR_BEGIN_NAMESPACE
#else
namespace vit
{
namespace container
{
#endif
namespace detail
{
#define X_MEMBER_TYPE_DETECTOR(Member) \
	template<class T, class F, class = void> struct extract_nested_##Member { typedef F type; };  \
	template<class T, class F> struct extract_nested_##Member<T, F, typename std::conditional<false, typename T::Member, void>::type> { typedef typename T::Member type; }
	X_MEMBER_TYPE_DETECTOR(reference);
	X_MEMBER_TYPE_DETECTOR(const_reference);
	X_MEMBER_TYPE_DETECTOR(volatile_reference);
	X_MEMBER_TYPE_DETECTOR(const_volatile_reference);
	X_MEMBER_TYPE_DETECTOR(pointer);
	X_MEMBER_TYPE_DETECTOR(const_pointer);
	X_MEMBER_TYPE_DETECTOR(volatile_pointer);
	X_MEMBER_TYPE_DETECTOR(const_volatile_pointer);
	X_MEMBER_TYPE_DETECTOR(void_pointer);
	X_MEMBER_TYPE_DETECTOR(const_void_pointer);
	X_MEMBER_TYPE_DETECTOR(is_always_equal);
	X_MEMBER_TYPE_DETECTOR(propagate_on_container_copy_assignment);
	X_MEMBER_TYPE_DETECTOR(propagate_on_container_move_assignment);
	X_MEMBER_TYPE_DETECTOR(propagate_on_container_swap);
#undef  X_MEMBER_TYPE_DETECTOR

	template <class, class = void> struct is_defined : std::false_type { };
	template <class T> struct is_defined<T, typename std::enable_if<std::is_object<T>::value && !std::is_pointer<T>::value && !!sizeof(T)>::type> : std::true_type { };

	template<class T> struct is_vc_array_iterator : std::false_type { };
	template<class T> struct is_vc_array_iterator<stdext::checked_array_iterator<T> > : std::true_type { };
	template<class T> struct is_vc_array_iterator<stdext::unchecked_array_iterator<T> > : std::true_type { };

	template<class It> struct checked_iterator_traits  { typedef typename std::iterator_traits<It>::reference reference; static reference deref_unchecked(It const &i) { return *i; } };
	template<class It> struct checked_iterator_traits<stdext::checked_array_iterator<It> > { typedef typename std::iterator_traits<It>::reference reference; static reference deref_unchecked(It const &i) { It r = i.base(); r += i.index(); return *r; } };
	template<class It> struct checked_iterator_traits<stdext::unchecked_array_iterator<It> > { typedef typename std::iterator_traits<It>::reference reference; static reference deref_unchecked(It const &i) { return *i.base(); } };

	template<class Tr, class Ax = typename Tr::allocator_type> struct extract_traits_select_on_container_copy_construction : std::false_type { static Ax invoke(Ax const &ax) { return ax; } };
#if !(defined(_MSC_VER) && _MSC_VER < 1910)
	template<class Tr> struct extract_traits_select_on_container_copy_construction<Tr, typename std::enable_if<
		!!sizeof(Tr::select_on_container_copy_construction(std::declval<typename Tr::allocator_type const &>())),
		decltype(Tr::select_on_container_copy_construction(std::declval<typename Tr::allocator_type const &>()))
	>::type> : std::true_type { static typename Tr::allocator_type invoke(typename Tr::allocator_type const &ax) { return Tr::select_on_container_copy_construction(ax); } };
#endif

	template<class Ax, class = Ax> struct extract_allocator_select_on_container_copy_construction : std::false_type { static Ax invoke(Ax const &ax) { return ax; } };
#if !(defined(_MSC_VER) && _MSC_VER < 1910)
	template<class Ax> struct extract_allocator_select_on_container_copy_construction<Ax, typename std::enable_if<
		!!sizeof(Ax::select_on_container_copy_construction(std::declval<Ax const &>())),
		decltype(Ax::select_on_container_copy_construction(std::declval<Ax const &>()))
	>::type> : std::true_type { static Ax invoke(Ax const &ax) { return ax.select_on_container_copy_construction(); } };
#endif

	template<class T> struct volatile_pointer_of { typedef T type; };
	template<class T> struct volatile_pointer_of<T *> { typedef T volatile *type; };

	template<class T> struct volatile_reference_of { typedef T type; };
	template<class T> struct volatile_reference_of<T &> { typedef T volatile &type; };

	template<class T, T, T> struct is_same_value : std::false_type { };
	template<class T, T V> struct is_same_value<T, V, V> : std::true_type { };

	template<class Ax, class = void> struct uses_default_allocate_nohint : std::false_type { };
	template<class Ax> struct uses_default_allocate_nohint<Ax, typename std::enable_if<
		is_same_value<typename std::allocator_traits<Ax>::pointer(std::allocator<typename std::allocator_traits<Ax>::value_type>:: *)(typename std::allocator_traits<Ax>::size_type), &Ax::allocate, &std::allocator<typename std::allocator_traits<Ax>::value_type>::allocate>::value,
		void>::type> : std::true_type { };

	template<class Ax, class = void> struct uses_default_allocate_hint : std::false_type { };
	template<class Ax> struct uses_default_allocate_hint<Ax, typename std::enable_if<
		is_same_value<typename std::allocator_traits<Ax>::pointer(std::allocator<typename std::allocator_traits<Ax>::value_type>:: *)(typename std::allocator_traits<Ax>::size_type, typename std::allocator_traits<Ax>::const_void_pointer), &Ax::allocate, &std::allocator<typename std::allocator_traits<Ax>::value_type>::allocate>::value,
		void>::type> : std::true_type { };

	template<class Ax, class = void> struct uses_default_deallocate : std::false_type { };
	template<class Ax> struct uses_default_deallocate<Ax, typename std::enable_if<
		is_same_value<void(std::allocator<typename std::allocator_traits<Ax>::value_type>:: *)(typename std::allocator_traits<Ax>::pointer, typename std::allocator_traits<Ax>::size_type), &Ax::deallocate, &std::allocator<typename std::allocator_traits<Ax>::value_type>::deallocate>::value,
		void>::type> : std::true_type { };

	template<class Ax>
	struct allocator_traits : public std::allocator_traits<Ax>
	{
		typedef Ax allocator_type;
		typedef std::allocator_traits<allocator_type> traits_type;
		typedef typename traits_type::value_type value_type;
#define X(Member, Fallback) typedef typename extract_nested_##Member<traits_type, typename extract_nested_##Member<allocator_type, Fallback>::type>::type Member
		X(reference, value_type &);
		X(const_reference, value_type const &);
		X(volatile_reference, typename volatile_reference_of<reference>::type);
		X(const_volatile_reference, typename volatile_reference_of<const_reference>::type);
		X(pointer, value_type *);
		X(const_pointer, value_type const *);
		X(volatile_pointer, typename volatile_pointer_of<pointer>::type);
		X(const_volatile_pointer, typename volatile_pointer_of<const_pointer>::type);
		X(void_pointer, void *);
		X(const_void_pointer, void const *);
		X(is_always_equal, std::is_empty<allocator_type>);
		X(propagate_on_container_copy_assignment, std::false_type);
		X(propagate_on_container_move_assignment, std::false_type);
		X(propagate_on_container_swap, std::false_type);
#undef  X
		static pointer null_pointer() { struct { pointer p; } s = {}; return s.p; }
		static value_type *address(allocator_type const &, reference r) { return std::addressof(static_cast<value_type &>(r)); }
		static value_type const *address(allocator_type const &, const_reference r) { return std::addressof(static_cast<value_type const &>(r)); }
		static value_type *ptr_address(allocator_type const &ax, pointer p) { return !p ? NULL : allocator_traits::address(ax, checked_iterator_traits<pointer>::deref_unchecked(p)); }
		static value_type const *ptr_address(allocator_type const &ax, const_pointer p) { return !p ? NULL : allocator_traits::address(ax, checked_iterator_traits<const_pointer>::deref_unchecked(p)); }
		static allocator_type select_on_container_copy_construction(allocator_type const &ax)
		{
			return std::conditional<
				extract_traits_select_on_container_copy_construction<traits_type>::value,
				extract_traits_select_on_container_copy_construction<traits_type>,
				extract_allocator_select_on_container_copy_construction<allocator_type>
			>::type::invoke(ax);
		}
	};

	template<class Ax>
	struct is_allocator_same_as_malloc;

	template<class Ax>
	struct is_allocator_same_as_malloc :
#if defined(STATIONARY_VECTOR_OPTIMIZE_STD_ALLOCATOR_AS_MALLOC) && STATIONARY_VECTOR_OPTIMIZE_STD_ALLOCATOR_AS_MALLOC  // This is normally a broken assumption, so don't do it by default
		std::conditional<
			std::is_pointer<typename std::allocator_traits<Ax>::pointer>::value &&
			uses_default_allocate_nohint<Ax>::value &&
			uses_default_allocate_hint<Ax>::value &&
			uses_default_deallocate<Ax>::value &&
			!(std::alignment_of<typename std::allocator_traits<Ax>::value_type>::value & (std::alignment_of<typename std::allocator_traits<Ax>::value_type>::value - 1)) &&
			(std::alignment_of<typename std::allocator_traits<Ax>::value_type>::value <= std::alignment_of<long double>::value),
			std::true_type,
			std::false_type
		>::type
#else
		std::false_type
#endif
	{
	};

	template<class T1, class T2> struct propagate_cv { typedef T2 type; };
	template<class T1, class T2> struct propagate_cv<T1 const, T2> : propagate_cv<T1, T2 const> { };
	template<class T1, class T2> struct propagate_cv<T1 volatile, T2> : propagate_cv<T1, T2 volatile> { };
	template<class T1, class T2> struct propagate_cv<T1 const volatile, T2> : propagate_cv<T1, T2 const volatile> { };

	template<class It, class Category, class = void> struct is_iterator : std::false_type { };
	template<class It, class Category> struct is_iterator<It, Category, typename std::enable_if<std::is_base_of<Category, typename std::enable_if<!std::is_integral<It>::value /* Workaround for _MSC_VER < 1900 */, std::iterator_traits<It> >::type::iterator_category>::value, void>::type> : std::true_type { };

	template <class Ax>
	struct allocator_temporary : private Ax
	{
		typedef allocator_traits<Ax> traits_type;
		typedef typename traits_type::value_type value_type;
		typedef typename traits_type::pointer pointer;

		value_type *addr() { return reinterpret_cast<value_type *>(&this->buf); }
		value_type &get() { return *this->addr(); }
		Ax &allocator() { return static_cast<Ax &>(*this); }
		~allocator_temporary() { traits_type::destroy(this->allocator(), this->addr()); }
		template <class... Args>
		explicit allocator_temporary(Ax &ax, Args &&... args)
			noexcept(noexcept(traits_type::construct(std::declval<Ax &>(), std::declval<typename traits_type::value_type *>(), std::forward<Args>(args)...)))
			: Ax(ax) { traits_type::construct(this->allocator(), this->addr(), std::forward<Args>(args)...); }
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
		operator It() const { return this->std::reverse_iterator<It>::base() - 1; }
	};

	template<class Ax>
	class reverse_allocator : public Ax
	{
	public:
		typedef reverse_iterator<typename allocator_traits<Ax>::pointer> pointer;
		reverse_allocator() = default;
		reverse_allocator(Ax const &ax) : Ax(ax) { }
		template<class U> struct rebind { typedef reverse_allocator<U> other; };
	};

	template<class Pointer, bool = is_defined<stdext::checked_array_iterator<unsigned char *> >::value>
	struct convertible_array_iterator_base
	{
		typedef Pointer pointer;
		pointer arr; size_t size, index;
		convertible_array_iterator_base(pointer const &arr, size_t const size, size_t const index) : arr(arr), size(size), index(index) { }
		convertible_array_iterator_base() : arr(), size(), index() { }
		operator pointer() const { return arr + static_cast<typename std::iterator_traits<pointer>::difference_type>(index); }
		typedef pointer type;
	};
	
#if defined(_CPPLIB_VER) && defined(_ITERATOR_) && _CPPLIB_VER >= 505 && !defined(NDEBUG) && defined(_DEBUG) && defined(_SECURE_SCL) && 0 || (defined(STATIONARY_VECTOR_DEBUG) && ((2 * STATIONARY_VECTOR_DEBUG + 1) - 1)) /* This breaks the API (&v[0] is no longer a pointer) so only enable it when debugging the container itself */
	template<class T>
	struct convertible_array_iterator_base<T *, true> : public convertible_array_iterator_base<T *, false>
	{
		typedef T *pointer;
		typedef convertible_array_iterator_base<pointer, false> base_type;
		convertible_array_iterator_base() = default;
		explicit convertible_array_iterator_base(pointer const &arr, size_t const size, size_t const index) : base_type(arr, size, index) { }
		operator stdext::checked_array_iterator<pointer>() const { stdext::checked_array_iterator<pointer> result(this->arr, this->size, this->index); return result; }
		operator stdext::unchecked_array_iterator<pointer>() const { stdext::unchecked_array_iterator<pointer> result(this->operator pointer()); return result; }
		typedef stdext::
#if defined(_SECURE_SCL) && _SECURE_SCL
			checked_array_iterator
#else
			unchecked_array_iterator
#endif
			<pointer> type;
	};
#endif

	template<class T>
	struct assignment_guard
	{
		typedef assignment_guard this_type;
		typedef T value_type;
		~assignment_guard()
		{
			if (this->p)
			{
				using std::swap;
				swap(*this->p, this->old);
			}
		}
		explicit assignment_guard(value_type *const p, value_type val) : p(p)
		{
			using std::swap;
			swap(this->old, val);
			swap(*this->p, this->old);
		}
		void release() { this->p = NULL; }
	private:
		assignment_guard(this_type const &);
		this_type &operator =(this_type const &);
		value_type old, *p;
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
		typedef Iterator iterator;
		typedef typename std::iterator_traits<iterator>::difference_type difference_type;
		iterator_partitioner() : _begin(), _end() { }
		explicit iterator_partitioner(iterator const &begin, iterator const &end) : _begin(begin), _end(end) { }
		iterator begin() const { return this->_begin; }
		iterator end() const { return this->_end; }
		bool empty() const { return this->_begin == this->_end; }
		iterator begin_front() const { return this->_begin; }
		void begin_front(iterator const &value) { this->_begin = value; }
		iterator end_front() const { return this->_end; }
		difference_type front_distance() const { return this->end_front() - this->begin_front(); }
		void pop_front() { this->_begin = this->_end; }
		iterator begin_back() const { return this->_begin; }
		iterator end_back() const { return this->_end; }
		void end_back(iterator const &value) const { this->_end = value; }
		difference_type back_distance() const { return this->end_back() - this->begin_back(); }
		void pop_back() { this->_end = this->_begin; }
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
		explicit iterator_partitioner(base_iterator const &begin, base_iterator const &end) : base_type(begin.base(), end.base()) { }
		base_iterator begin() const { return base_iterator(this->base_type::begin()); }
		base_iterator end() const { return base_iterator(this->base_type::end()); }
		bool empty() const { return this->base_type::empty(); }
		iterator begin_front() const { return iterator(this->base_type::end_back()); }
		void begin_front(iterator const &value) { this->base_type::end_back(value); }
		iterator end_front() const { return iterator(this->base_type::begin_back()); }
		difference_type front_distance() const { return this->base_type::front_distance(); }
		void pop_front() { return this->base_type::pop_back(); }
		iterator begin_back() const { return iterator(this->base_type::end_front()); }
		iterator end_back() const { return iterator(this->base_type::begin_front()); }
		void end_back(iterator const &value) { this->base_type::begin_front(value); }
		difference_type back_distance() const { return this->base_type::back_distance(); }
		void pop_back() { return this->base_type::pop_front(); }
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
		base_iterator begin() const { return base_iterator(this->base_type::begin()); }
		base_iterator end() const { return base_iterator(this->base_type::end()); }
		bool empty() const { return this->base_type::empty(); }
		iterator begin_front() const { return iterator(this->base_type::begin_front()); }
		void begin_front(iterator const &value) { this->base_type::begin_front(value.base()); }
		iterator end_front() const { return iterator(this->base_type::end_front()); }
		difference_type front_distance() const { return this->base_type::front_distance(); }
		void pop_front() { return this->base_type::pop_front(); }
		iterator begin_back() const { return iterator(this->base_type::begin_back()); }
		iterator end_back() const { return iterator(this->base_type::end_back()); }
		void end_back(iterator const &value) { this->base_type::end_back(value.base()); }
		difference_type back_distance() const { return this->base_type::back_distance(); }
		void pop_back() { return this->base_type::pop_back(); }
	};

	template <class It>
	It destroy(It begin, It end)
	{
#if __cplusplus >= 201703L || defined(__cpp_lib_raw_memory_algorithms)
		std::destroy(begin, end);
		begin = end;
#else
		typedef typename std::iterator_traits<It>::value_type value_type;
		while (begin != end) { begin->~value_type(); ++begin; }
#endif
		return begin;
	}

	template <class FwdIt, class OutIt>
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

	template <class FwdIt, class OutIt>
	typename std::enable_if<
		std::is_nothrow_move_constructible<typename std::iterator_traits<OutIt>::value_type>::value &&
		std::is_nothrow_destructible<typename std::iterator_traits<OutIt>::value_type>::value
		, OutIt>::type uninitialized_destructive_move_if_noexcept(FwdIt begin, FwdIt end, OutIt dest)
	{
		// If we can move without throwing, then perform a destructive move construction.
		return uninitialized_destructive_move<FwdIt, OutIt>(std::forward<FwdIt>(begin), std::forward<FwdIt>(end), std::forward<OutIt>(dest));
	}

	template <class FwdIt, class OutIt>
	typename std::enable_if<
		std::is_nothrow_move_constructible<typename std::iterator_traits<FwdIt>::value_type>::value &&
		std::is_nothrow_destructible<typename std::iterator_traits<FwdIt>::value_type>::value
		, OutIt>::type uninitialized_destructive_move_if_noexcept_backout(FwdIt begin, FwdIt end, OutIt src)
	{
		// Backing out nothrow uninitialized move construction a destructive move construction back into the source.
		return uninitialized_destructive_move<FwdIt, OutIt>(std::forward<FwdIt>(begin), std::forward<FwdIt>(end), std::forward<OutIt>(src));
	}

	template <class FwdIt, class OutIt>
	typename std::enable_if<!(
		std::is_nothrow_move_constructible<typename std::iterator_traits<OutIt>::value_type>::value &&
		std::is_nothrow_destructible<typename std::iterator_traits<OutIt>::value_type>::value
		), OutIt>::type uninitialized_destructive_move_if_noexcept(FwdIt begin, FwdIt end, OutIt dest)
	{
		// If we can't move without throwing, then copy instead.
		return std::uninitialized_copy(begin, end, dest);
	}

	template <class FwdIt, class OutIt>
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

	template<class Type, Type &Value> struct static_ { typedef static_ this_type; typedef Type type; static type const &value; static type const *address() { return &Value; } };
	template<class Type, Type &Value> typename static_<Type, Value>::type const &static_<Type, Value>::value = Value;

	template<class R, class... Args, R(&F)(Args...)>
	struct static_<R(Args...), F>
	{
		typedef static_ this_type;
		typedef R type(Args...);
		typedef R result_type;
		static type &value;
		R operator()(Args... args) const { return F(std::forward<Args>(args)...); }
		static type *address() { return &F; }
	};
	template<class R, class... Args, R(&F)(Args...)>
	typename static_<R(Args...), F>::type &static_<R(Args...), F>::value = F;

	template<class Ax, class It>
	struct initialization_guard
	{
		typedef initialization_guard this_type;
		typedef Ax allocator_type;
		typedef allocator_traits<Ax> traits_type;
		typedef It iterator;
		initialization_guard() : ax() { }
		explicit initialization_guard(allocator_type &ax, iterator const &begin) : i(begin), begin(begin), ax(std::addressof(ax)) { }
		~initialization_guard() { if (ax) { while (i != begin) { traits_type::destroy(*ax, traits_type::address(*ax, *--i)); } } }
		template<class... Args>
		void emplace_back(Args &&... args) { traits_type::construct(*ax, traits_type::address(*ax, *i), std::forward<Args>(args)...); ++i; }
		iterator release() { begin = i; return i; }
		iterator i;
	private:
		iterator begin;
		allocator_type *ax;
		initialization_guard(this_type const &);
		this_type &operator =(this_type const &);
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
			uninitialized_destructive_move_if_noexcept_backout<iterator, iterator>(this->begin, this->end, this->src);
		}
		explicit safe_move_or_copy_construction(iterator const begin, iterator const end, iterator const dest) : src(begin), begin(dest), end(dest)
		{
			std::advance<iterator>(this->end, std::distance(dest, uninitialized_destructive_move_if_noexcept<iterator, iterator>(begin, end, dest)));
		}
		void swap(this_type &other) { using std::swap; swap(this->begin, other.begin); swap(this->end, other.end); }
		friend void swap(this_type &a, this_type &b) { return a.swap(b); }
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

	template<class T> inline void swap_if(T &a, T &b, std::integral_constant<bool, true>) { using std::swap; swap(a, b); }
	template<class T> inline void swap_if(T &, T &, std::integral_constant<bool, false>) { }

	template<class FwdIt1, class FwdIt2>
	typename std::enable_if<(
		detail::is_iterator<FwdIt1, std::forward_iterator_tag>::value &&
		std::is_trivially_default_constructible<typename std::iterator_traits<FwdIt1>::value_type>::value &&
		std::is_trivially_destructible<typename std::iterator_traits<FwdIt1>::value_type>::value &&
		detail::is_iterator<FwdIt2, std::forward_iterator_tag>::value &&
		std::is_trivially_default_constructible<typename std::iterator_traits<FwdIt2>::value_type>::value &&
		std::is_trivially_destructible<typename std::iterator_traits<FwdIt2>::value_type>::value
		), void>::type uninitialized_swap_ranges(FwdIt1 begin1, FwdIt1 end1, FwdIt2 begin2, FwdIt2 end2)  // swaps two ranges, assuming sufficient uninitialized space exists beyond the shorter range
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
	OutIt uninitialized_value_construct(OutIt const &begin, OutIt end, Ax &ax)
	{
		initialization_guard<Ax, OutIt> guard(ax, begin);
		while (guard.i != end) { guard.emplace_back(); }
		return guard.release();
	}

	template<class Ax>
	class heap_ptr : public Ax
	{
		typedef heap_ptr this_type;
		heap_ptr(this_type const &);
		this_type &operator =(this_type const &);
		typedef allocator_traits<Ax> traits_type;
		typedef typename traits_type::pointer pointer;
		typedef typename traits_type::size_type size_type;
		pointer p;
		size_type n;
	public:
		~heap_ptr()
		{
			if (this->p != pointer()) { traits_type::deallocate(static_cast<Ax &>(*this), this->p, this->n); }
		}
		explicit heap_ptr(Ax &ax, size_type const count) : Ax(ax), p(), n(count)
		{
			if (count) { this->p = static_cast<pointer>(traits_type::allocate(static_cast<Ax &>(*this), count)); }
		}
		explicit heap_ptr(Ax &ax, size_type const count, pointer const hint /* only before C++20 */) : Ax(ax), p(), n(count)
		{
			if (count) { this->p = static_cast<pointer>(traits_type::allocate(static_cast<Ax &>(*this), count, hint)); }
		}
		explicit heap_ptr(Ax &ax, pointer const ptr, size_type const count) : Ax(ax), p(ptr), n(count) { }
		pointer get() const { return this->p; }
		pointer release() { pointer const ptr = this->p; this->p = pointer(); return ptr; }
	};

#if defined(STATIONARY_VECTOR_ATOMICS) && (STATIONARY_VECTOR_ATOMICS)
	template<class T, std::memory_order DefaultMemoryOrder = std::memory_order_relaxed>
	class loose_atomic  // Relaxed memory ordering is probably wrong here!
	{
		typedef loose_atomic this_type;
		std::atomic<T> data;
		this_type &unvolatile() volatile { return const_cast<this_type &>(*this); }
		this_type const &unvolatile() const volatile { return const_cast<this_type const &>(*this); }
		T exchange(T value) { return this->data.exchange(value, DefaultMemoryOrder); }
		T load() const { return this->data.load(DefaultMemoryOrder); }
		T load() const volatile { return this->unvolatile().load(); }
	public:
		loose_atomic() /* This might NOT initialize the internal value! */ = default;
		explicit loose_atomic(T const &value) : data(value) { }
		loose_atomic(this_type const &other) : data(other.operator T()) { }
		loose_atomic(this_type &&other) : data(other.exchange(T())) { }
		this_type &operator =(this_type const &other) { this->data.store(other.operator T(), DefaultMemoryOrder); return *this; }
		this_type &operator =(this_type &&other) { this->data.store(other.exchange(T()), DefaultMemoryOrder); return *this; }
		this_type &operator--() { return this->operator-=(1); }
		this_type &operator++() { return this->operator+=(1); }
		this_type &operator-=(T value) { this->data.fetch_sub(value, DefaultMemoryOrder); return *this; }
		this_type &operator+=(T value) { this->data.fetch_add(value, DefaultMemoryOrder); return *this; }
		operator T() const { return this->load(); }
		operator T() const volatile { return this->load(); }
		T swap(this_type &other) { return this->exchange(other.exchange(this->load())); }
		T swap(this_type volatile &other) volatile { return this->exchange(other.exchange(this->load())); }
		friend T swap(this_type &a, this_type &b) { return a.swap(b); }
		friend T swap(this_type volatile &a, this_type volatile &b) { return a.swap(b); }
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

	template<class Ax>
	typename allocator_traits<Ax>::pointer destroy_preferably_in_reverse(typename allocator_traits<Ax>::pointer const begin, typename allocator_traits<Ax>::pointer end, Ax &ax)
	{
#if defined(_CPPLIB_VER) && _CPPLIB_VER >= 650 && defined(_XMEMORY_)
		__if_exists(std::_Destroy_range)
		{
			typedef reverse_allocator<Ax> allocator_type;
			allocator_type rev_ax(ax);
#if defined(_CPPLIB_VER) && _CPPLIB_VER <= 650 && !defined(_MSVC_STL_VERSION)
			typename std::conditional<is_defined<std::_Wrap_alloc<allocator_type> >::value, std::_Wrap_alloc<allocator_type>, allocator_type &>::type
			
#else
			allocator_type &
#endif
				wrap_ax(rev_ax);
			std::_Destroy_range(static_cast<typename allocator_type::pointer>(end), static_cast<typename allocator_type::pointer>(begin), wrap_ax);
			end = begin;
		}
#endif
		while (end != begin)
		{
			allocator_traits<Ax>::destroy(ax, allocator_traits<Ax>::address(ax, *--end));
		}
		return end;
	}

	template<class Ax, class It>
	typename std::enable_if<(
		detail::is_iterator<It, std::bidirectional_iterator_tag>::value
		), void>::type reverse(It begin, It end, Ax &ax)
	{
		typedef allocator_traits<Ax> traits_type;
		typedef typename traits_type::difference_type difference_type;
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
				It mid1(begin); std::advance<It>(mid1, +(d / static_cast<difference_type>(2)));
				It mid2(end); std::advance<It>(mid2, -(d / static_cast<difference_type>(2)));
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
						if (i2 == rparts.begin_back()) { rparts.pop_back(); }
						if (j1 == lparts.end_front()) { lparts.pop_front(); }
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
		detail::is_iterator<It, std::bidirectional_iterator_tag>::value
		), void>::type rotate(Ax &ax, It begin, It mid, It end)
	{
		// std::rotate(begin, mid, end);  // can't call this because the type might should not (and possibly cannot) be constructed without the allocator
		detail::reverse(begin, mid, ax);
		detail::reverse(mid, end, ax);
		detail::reverse(begin, end, ax);
	}

	template<class InIt, class OutIt, class Diff>
	typename std::enable_if<(
		detail::is_iterator<InIt, std::input_iterator_tag>::value &&
		!detail::is_iterator<InIt, std::forward_iterator_tag>::value &&
		detail::is_iterator<OutIt, std::output_iterator_tag>::value &&
		!detail::is_iterator<OutIt, std::forward_iterator_tag>::value
		), std::pair<InIt, OutIt> >::type copy_limited(InIt input_begin, InIt input_end, OutIt output_begin, OutIt output_end, Diff nmax = ~Diff())
	{
		std::pair<InIt, OutIt> result(std::forward<InIt>(input_begin), std::forward<OutIt>(output_begin));
		for (; nmax && result.first != input_end && result.second != output_end; (void)++result.first, (void)++result.second, --nmax)
		{
			*result.second = *result.first;
		}
		return result;
	}

	template<class InIt, class OutIt, class Diff>
	typename std::enable_if<(
		detail::is_iterator<InIt, std::forward_iterator_tag>::value &&
		detail::is_iterator<OutIt, std::output_iterator_tag>::value &&
		!detail::is_iterator<OutIt, std::forward_iterator_tag>::value
		), std::pair<InIt, OutIt> >::type copy_limited(InIt input_begin, InIt input_end, OutIt output_begin, OutIt output_end, Diff nmax = ~Diff())
	{
		{
			typename std::iterator_traits<InIt>::difference_type const d1 = std::distance<InIt>(input_begin, input_end);
			if (nmax == ~Diff() || d1 < nmax) { nmax = static_cast<Diff>(d1); }
			assert(nmax != ~Diff());
			if (nmax < d1) { input_end = input_begin; std::advance<InIt>(input_end, static_cast<typename std::iterator_traits<InIt>::difference_type>(nmax)); }
		}
		std::pair<InIt, OutIt> result(std::forward<InIt>(input_begin), std::forward<OutIt>(output_begin));
		for (; nmax && result.second != output_end; (void)++result.first, (void)++result.second, --nmax)
		{
			*result.second = *result.first;
		}
		return result;
	}

	template<class InIt, class OutIt, class Diff>
	typename std::enable_if<(
		detail::is_iterator<InIt, std::input_iterator_tag>::value &&
		!detail::is_iterator<InIt, std::forward_iterator_tag>::value &&
		detail::is_iterator<OutIt, std::forward_iterator_tag>::value
		), std::pair<InIt, OutIt> >::type copy_limited(InIt input_begin, InIt input_end, OutIt output_begin, OutIt output_end, Diff nmax = ~Diff())
	{
		{
			typename std::iterator_traits<OutIt>::difference_type const d2 = std::distance<OutIt>(output_begin, output_end);
			if (nmax == ~Diff() || d2 < nmax) { nmax = static_cast<Diff>(d2); }
			assert(nmax != ~Diff());
			if (nmax < d2) { output_end = output_begin; std::advance<OutIt>(output_end, static_cast<typename std::iterator_traits<OutIt>::difference_type>(nmax)); }
		}
		std::pair<InIt, OutIt> result(std::forward<InIt>(input_begin), std::forward<OutIt>(output_begin));
		for (; nmax && result.first != input_end; (void)++result.first, (void)++result.second, --nmax)
		{
			*result.second = *result.first;
		}
		return result;
	}

	template<class InIt, class OutIt, class Diff>
	typename std::enable_if<(
		detail::is_iterator<InIt, std::forward_iterator_tag>::value &&
		detail::is_iterator<OutIt, std::forward_iterator_tag>::value
		), std::pair<InIt, OutIt> >::type copy_limited(InIt input_begin, InIt input_end, OutIt output_begin, OutIt output_end, Diff nmax = ~Diff())
	{
		{
			typename std::iterator_traits<InIt>::difference_type const d1 = std::distance<InIt>(input_begin, input_end);
			typename std::iterator_traits<OutIt>::difference_type const d2 = std::distance<OutIt>(output_begin, output_end);
			if (nmax == ~Diff() || d1 < nmax) { nmax = static_cast<Diff>(d1); }
			if (nmax == ~Diff() || d2 < nmax) { nmax = static_cast<Diff>(d2); }
			assert(nmax != ~Diff());
			if (nmax < d1) { input_end = input_begin; std::advance<InIt>(input_end, static_cast<typename std::iterator_traits<InIt>::difference_type>(nmax)); }
			if (nmax < d2) { output_end = output_begin; std::advance<OutIt>(output_end, static_cast<typename std::iterator_traits<OutIt>::difference_type>(nmax)); }
		}
		std::pair<InIt, OutIt> result(std::forward<InIt>(input_end), std::forward<OutIt>(output_begin));
		result.second = std::copy(std::forward<InIt>(input_begin), result.first, std::forward<OutIt>(result.second));
		return result;
	}

	template<class InIt, class OutIt>
	typename std::enable_if<(
		detail::is_iterator<InIt, std::input_iterator_tag>::value
		), std::pair<InIt, OutIt> >::type partitioned_copy_limited(InIt input_begin, InIt input_end, OutIt output_begin, OutIt output_end)
	{
		iterator_partitioner<InIt> input_parts(input_begin, input_end);
		iterator_partitioner<OutIt> output_parts(output_begin, output_end);
		while (!input_parts.empty() && !output_parts.empty())
		{
			std::pair<typename iterator_partitioner<InIt>::iterator, typename iterator_partitioner<OutIt>::iterator> limits = detail::copy_limited(
				input_parts.begin_front(), input_parts.end_front(),
				output_parts.begin_front(), output_parts.end_front(),
				~typename std::iterator_traits<OutIt>::difference_type());
			if (limits.first == input_parts.end_front()) { input_parts.pop_front(); } else { input_parts.begin_front(limits.first); }
			if (limits.second == output_parts.end_front()) { output_parts.pop_front(); } else { output_parts.begin_front(limits.second); }
		}
		return std::pair<InIt, OutIt>(input_parts.begin(), output_parts.begin());
	}

	template<class Ax, class InIt, class OutIt>
	typename std::enable_if<(
		detail::is_iterator<InIt, std::input_iterator_tag>::value
		), OutIt>::type uninitialized_copy(InIt begin, InIt end, OutIt const &out, Ax &ax)
	{
		initialization_guard<Ax, OutIt> guard(ax, out);
		for (; begin != end; ++begin)
		{
			guard.emplace_back(*begin);
		}
		return guard.release();
	}

	template<class Ax, class InIt, class OutIt>
	typename std::enable_if<(
		detail::is_iterator<InIt, std::forward_iterator_tag>::value
		), std::pair<InIt, OutIt> >::type uninitialized_copy_up_to_n(InIt begin, InIt end, typename std::iterator_traits<InIt>::difference_type nmax, OutIt out, Ax &ax)
	{
		if (nmax < std::distance<InIt>(begin, end)) { end = begin; std::advance<InIt>(end, nmax); }
		return std::make_pair(end, detail::uninitialized_copy(std::forward<InIt>(begin), std::forward<InIt>(end), std::forward<OutIt>(out), ax));
	}

	template<class Ax, class InIt, class OutIt>
	typename std::enable_if<
		detail::is_iterator<InIt, std::input_iterator_tag>::value
		&& !detail::is_iterator<InIt, std::forward_iterator_tag>::value
		, std::pair<InIt, OutIt> >::type uninitialized_copy_up_to_n(InIt begin, InIt end, typename std::iterator_traits<InIt>::difference_type nmax, OutIt const &out, Ax &ax)
	{
		initialization_guard<Ax, OutIt> guard(ax, out);
		for (; nmax && begin != end; (void)++begin, --nmax)
		{
			guard.emplace_back(*begin);
		}
		return std::pair<InIt, OutIt>(std::move(begin), guard.release());
	}
}

template<class Pointer, bool = detail::is_vc_array_iterator<typename detail::convertible_array_iterator<Pointer>::type>::value>
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
	size_t member() const { return (*reinterpret_cast<size_t const (*)[2]>(reinterpret_cast<Pointer const *>(&static_cast<base_type const &>(*this)) + 1))[I]; }
	size_t size() const { return this->member<0>(); }
	size_t index() const { return this->member<1>(); }
	Pointer base() const { return this->base_type::base(); }
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
	operator Pointer() const { return this->base(); }
	// Normally, checked iterators don't allow comparing across different arrays, but we allow that for equality comparisons
	bool operator==(this_type const &other) const { return this->base() == other.base(); }
	bool operator!=(this_type const &other) const { return !this->operator==(other); }
	this_type operator+ (difference_type d) const { this_type result(*this); result += d; return result; }
	this_type operator- (difference_type d) const { this_type result(*this); result -= d; return result; }
	this_type operator-=(difference_type const &delta) { static_cast<base_type &>(*this) -= delta; return *this; }
	this_type operator+=(difference_type const &delta) { static_cast<base_type &>(*this) += delta; return *this; }
	this_type operator--() { --static_cast<base_type &>(*this); return *this; }
	this_type operator++() { ++static_cast<base_type &>(*this); return *this; }
	difference_type operator- (this_type const &other) const { return static_cast<base_type const &>(*this) - static_cast<base_type const &>(other); }
	template<class OtherPointer>
	difference_type operator- (stationary_vector_pointer<OtherPointer> const &other) const { return this->base() - other.base(); }
};

template<class Pointer, class Diff = typename std::iterator_traits<Pointer>::difference_type, class Size = size_t>
struct stationary_vector_payload;

template<class Pointer, class Diff = typename std::iterator_traits<Pointer>::difference_type, class Size = size_t>
struct stationary_vector_bucket
{
	typedef Pointer pointer;
	typedef Diff difference_type;
	typedef Size size_type;
	typedef stationary_vector_bucket this_type;
	struct ibegin_less { bool operator()(this_type const &a, this_type const &b) const { return a._ibegin < b._ibegin; } };
	pointer items_begin() const { return this->_begin; }
	pointer items_begin() const volatile { return detail::atomic_traits_of<pointer>::load(&this->_begin); }
	pointer items_end() const { pointer result = this->items_begin(); result += static_cast<difference_type>(this->bucket_size()); return result; }
	pointer items_end() const volatile { pointer result = this->items_begin(); result += static_cast<difference_type>(this->bucket_size()); return result; }
	size_type ibegin() const { return this->_ibegin; }
	size_type ibegin() const volatile { return detail::atomic_traits_of<size_type>::load(&this->_ibegin); }
	size_type iend() const /* This requires information from the next (adjacent) instance in memory if the pointer is valid! */
	{
		size_type const result = (this + !!this->items_begin())->ibegin();
		assert(~result >= result);
		assert(result >= this->ibegin());
		return result;
	}
	size_type iend() const volatile { return (this + !!this->items_begin())->ibegin(); }
	size_type bucket_size() const { return this->iend() - this->ibegin(); }
	size_type bucket_size() const volatile { return this->iend() - this->ibegin(); }
protected:  // Don't let outsiders create instances of a single bucket, since adjacent instances depend on each other in memory and this is bug-prone.
	stationary_vector_bucket() = default;
	void items_begin(pointer const p) { this->_begin = p; };
	void ibegin_add(size_type const value) { this->_ibegin += value; };
	static this_type create(pointer const b, size_type const ib) { stationary_vector_bucket result = this_type(); result._begin = b; result._ibegin = ib; return result; }
	friend struct stationary_vector_payload<Pointer, Diff, Size>;
	pointer _begin;
	size_type _ibegin;
};

template<class Pointer, class Diff, class Size>
struct stationary_vector_payload
{
	typedef stationary_vector_payload this_type;
	typedef typename std::remove_cv<Pointer>::type pointer;
	typedef Diff difference_type;
	typedef Size size_type;
	typedef stationary_vector_bucket<pointer, Diff, Size> bucket_type;
#if defined(_MSC_VER) && defined(_DEBUG)
#pragma warning(push)
#if _MSC_VER >= 1900
#pragma warning(disable: 4582)
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
#ifdef _DEBUG
		assert(!this->members.constructed_buckets);
#endif
	}
	explicit stationary_vector_payload() : members()
	{
		// Ensure we always have at least 1 valid bucket.
		// This is necessary to be able to detect past-the-end pointers without involving anything except a reference to the bucket (which we already have).
		unsigned char ensure_buckets_are_trivially_destructible[std::is_trivially_destructible<bucket_type>::value];
		(void)ensure_buckets_are_trivially_destructible;
#ifdef _DEBUG
		std::uninitialized_fill_n(reinterpret_cast<unsigned char *>(&*this->_buckets.begin()), (sizeof(*this->_buckets.begin()) * this->_buckets.size()), static_cast<unsigned char>(0xCD));
#endif
		struct super_type { bucket_type bucket; } temp = {} /* ensure value-initialization no matter what -- this is a workaround for Visual C++ 2013 */;
		this->construct_bucket(0, temp.bucket);
	}
#if defined(_MSC_VER) && defined(_DEBUG)
#pragma warning(pop)
#endif
	enum { static_bucket_capacity = (sizeof(size_t) >= sizeof(unsigned long long) ? sizeof(unsigned int) + sizeof(unsigned short) : sizeof(size_t)) * CHAR_BIT };
	typedef bucket_type buckets_type[static_bucket_capacity];
	struct members_except_bucket_count_type
	{
		detail::atomic_traits_of<size_t>::atomic_type nbuckets;
#ifdef _DEBUG
		size_t constructed_buckets /* bit-vector */;
#endif
		members_except_bucket_count_type() :
			nbuckets(0)
#ifdef _DEBUG
			, constructed_buckets()
#endif
		{
		}
	};
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4623)  // default constructor could not be generated because a base class default constructor is inaccessible or deleted
#endif
	struct members_type
	{
		/* We ultimately decided NOT use the allocator to construct and destroy these buckets.
			* This is to avoid the payload introducing a dependency on the allocator.
			*/
		typedef typename std::conditional<
			std::is_trivially_destructible<buckets_type>::value ||
			!detail::is_defined<std::aligned_storage<sizeof(buckets_type), std::alignment_of<buckets_type>::value> >::value,
			std::enable_if<true, buckets_type>,
			std::aligned_storage<sizeof(buckets_type), std::alignment_of<buckets_type>::value>
		>::type::type buckets_type_inner;
		buckets_type_inner _uninitialized_buckets;
		buckets_type &get() { return reinterpret_cast<buckets_type &>(this->_uninitialized_buckets); }
		buckets_type const &get() const { return reinterpret_cast<buckets_type const &>(this->_uninitialized_buckets); }
		buckets_type volatile &get() volatile { return reinterpret_cast<buckets_type volatile &>(this->_uninitialized_buckets); }
		buckets_type const volatile &get() const volatile { return reinterpret_cast<buckets_type const volatile &>(this->_uninitialized_buckets); }
		bucket_type &operator[](size_t const i) { return this->get()[i]; }
		bucket_type const &operator[](size_t const i) const { return this->get()[i]; }
		bucket_type volatile &operator[](size_t const i) volatile { return this->get()[i]; }
		bucket_type const volatile &operator[](size_t const i) const volatile { return this->get()[i]; }
		bucket_type *operator +(ptrdiff_t const i) { return this->get() + i; }
		bucket_type const *operator +(ptrdiff_t const i) const { return this->get() + i; }

		typedef typename detail::convertible_array_iterator<bucket_type *> bucket_pointer;
		size_t size() const { return sizeof(this->get()) / sizeof(*this->get()); }
		typename bucket_pointer::type begin() { return bucket_pointer(this->get(), this->size(), 0); }
		typename bucket_pointer::type end() { return bucket_pointer(this->get(), this->size(), this->size()); }
	};
#ifdef _MSC_VER
#pragma warning(pop)
#endif
	members_except_bucket_count_type members;
	members_type _buckets;
	size_t bucket_count() const          { return this->members.nbuckets; }
	size_t bucket_count() const volatile { return this->members.nbuckets; }
	size_t constructed_bucket_count() const { return this->bucket_count() + 1; }
	members_type                &buckets()                { return this->_buckets; }
	members_type const          &buckets() const          { return this->_buckets; }
	members_type       volatile &buckets()       volatile { return this->_buckets; }
	members_type const volatile &buckets() const volatile { return this->_buckets; }
	bucket_type                *bucket_address(size_t const i)                { return &this->buckets()[i]; } /* use this when the bucket might NOT have been constructed */
	bucket_type          const *bucket_address(size_t const i) const          { return &this->buckets()[i]; }
	bucket_type       volatile *bucket_address(size_t const i)       volatile { return &this->buckets()[i]; }
	bucket_type const volatile *bucket_address(size_t const i) const volatile { return &this->buckets()[i]; }
	pointer bucket_begin(size_t const ibucket) const          { return this->bucket(ibucket).items_begin(); }
	pointer bucket_begin(size_t const ibucket) const volatile { return this->bucket(ibucket).items_begin(); }
	pointer bucket_end(size_t const ibucket) const { return this->bucket(ibucket).items_end(); }
	pointer unsafe_index(size_type const i, size_t *const bucket_index = NULL) const          { size_t ibucket = this->bucket_index(i); pointer result(this->bucket_begin(ibucket)); result += static_cast<difference_type>(i - this->bucket_ibegin(ibucket)); if (bucket_index) { *bucket_index = ibucket; } return result; }
	pointer unsafe_index(size_type const i, size_t *const bucket_index = NULL) const volatile { size_t ibucket = this->bucket_index(i); pointer result(this->bucket_begin(ibucket)); result += static_cast<difference_type>(i - this->bucket_ibegin(ibucket)); if (bucket_index) { *bucket_index = ibucket; } return result; }
	size_type bucket_ibegin(size_t const i) const          { return this->bucket(i).ibegin(); }
	size_type bucket_ibegin(size_t const i) const volatile { return this->bucket(i).ibegin(); }
	size_type bucket_iend(size_t const i) const          { return this->bucket(i).iend(); }
	size_type bucket_iend(size_t const i) const volatile { return this->bucket(i).iend(); }
	size_t bucket_index(size_t const i) const          { size_t j; for (j = this->bucket_count(); j; --j) { if (this->bucket_ibegin(j) <= i) { break; } } return j; }
	size_t bucket_index(size_t const i) const volatile { size_t j; for (j = this->bucket_count(); j; --j) { if (this->bucket_ibegin(j) <= i) { break; } } return j; }
	pointer alloc_begin(size_t const ibucket) const { return this->bucket_begin(ibucket); }
	pointer alloc_end(size_t const ibucket) const { return this->bucket_end(ibucket); }
	size_type alloc_size(size_t const ibucket) const { assert(ibucket < this->bucket_count()); return this->bucket_size(ibucket); }
	size_type bucket_size(size_t const ibucket) const { return this->bucket(ibucket).bucket_size(); }
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
		(void)bucket_iend;
		assert(bucket_begin != bucket_end);
		bucket_type *const bucket = this->bucket_address(ibucket);
		size_t bucket_count = this->bucket_count();
		assert(ibucket <= bucket_count && "attempting to set a bucket before preceding ones have been constructed");
		assert(ibucket < bucket_count || !this->bucket_end(ibucket));
		if (bucket_count <= ibucket && ibucket + 1 < static_bucket_capacity)
		{
			this->construct_bucket(ibucket + 1, bucket_type::create(pointer(), bucket_iend));
		}
		assert(!bucket->items_begin() && bucket->ibegin() == bucket_iend - static_cast<size_type>(bucket_end - bucket_begin));
		bucket->items_begin(bucket_begin) /* only set the field that's being actually changing (the others should be untouched, for multithreading) */;
		if (ibucket + 1 >= bucket_count) { ++this->members.nbuckets; }
	}
	void append(this_type &&other)
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
		if (n < static_bucket_capacity) { this->destroy_bucket(n); }
		--this->members.nbuckets;
	}
	void swap(this_type &other)
	{
		members_type &this_buckets = this->buckets(), &other_buckets = other.buckets();
		detail::uninitialized_swap_ranges(
			this_buckets.begin(), this_buckets.begin() + static_cast<ptrdiff_t>(this->constructed_bucket_count()),
			other_buckets.begin(), other_buckets.begin() + static_cast<ptrdiff_t>(other.constructed_bucket_count()));
		using std::swap; swap(this->members, other.members);
	}
	friend void swap(this_type &a, this_type &b) { return a.swap(b); }
private:
	bucket_type                &bucket(size_t const ibucket)                { return *this->bucket_address(ibucket); }
	bucket_type const          &bucket(size_t const ibucket) const          { return *this->bucket_address(ibucket); }
	bucket_type       volatile &bucket(size_t const ibucket)       volatile { return *this->bucket_address(ibucket); }
	bucket_type const volatile &bucket(size_t const ibucket) const volatile { return *this->bucket_address(ibucket); }
	void destroy_bucket(size_t const ibucket)
	{
#ifdef _DEBUG
		assert((this->members.constructed_buckets & (static_cast<size_t>(1) << ibucket)));
#endif
		bucket_type *const bucket = this->bucket_address(ibucket);
		bucket->~bucket_type();
#ifdef _DEBUG
		this->members.constructed_buckets = this->members.constructed_buckets & ~(static_cast<size_t>(1) << ibucket);
#endif
		std::uninitialized_fill_n(reinterpret_cast<unsigned char *>(bucket), sizeof(*bucket), static_cast<unsigned char>(0xCD));
	}
	void construct_bucket(size_t const ibucket, bucket_type const &value)
	{
#ifdef _DEBUG
		assert(!(this->members.constructed_buckets & (static_cast<size_t>(1) << ibucket)));
#endif
		::new(static_cast<void *>(this->bucket_address(ibucket))) bucket_type(value);
#ifdef _DEBUG
		this->members.constructed_buckets = this->members.constructed_buckets | (static_cast<size_t>(1) << ibucket);
#endif
	}
	stationary_vector_payload(this_type const &other);
	stationary_vector_payload(this_type &&other);
	this_type &operator =(this_type other);
};

template<class T, class Pointer = T *, class Reference = T &, class Bucket = stationary_vector_bucket<T *> >
struct stationary_vector_iterator
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
	template<class T2, class P2, class R2>
	stationary_vector_iterator(stationary_vector_iterator<T2, P2, R2, typename std::enable_if<
		std::is_convertible<T2 *, T *>::value &&
		std::is_convertible<P2, Pointer>::value &&
		std::is_convertible<R2, Reference>::value
		, Bucket>::type> const &other) : this_type(other.b, other.p) { }
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
	pointer operator->() const { return this->p; }
	reference operator*() const { return *this->p; }
	reference operator[](difference_type const value) const { this_type i(*this); i += value; return *i; }
	template<class T2, class P2, class R2> bool operator==(stationary_vector_iterator<T2, P2, R2, Bucket> const &other) const { return this->p == other.p; }
	template<class T2, class P2, class R2> bool operator!=(stationary_vector_iterator<T2, P2, R2, Bucket> const &other) const { return !this->operator==(other); }
	template<class T2, class P2, class R2> bool operator< (stationary_vector_iterator<T2, P2, R2, Bucket> const &other) const { return other.b && (!this->b || this->operator- (other) < difference_type()); }
	template<class T2, class P2, class R2> bool operator> (stationary_vector_iterator<T2, P2, R2, Bucket> const &other) const { return  other.operator< (*this); }
	template<class T2, class P2, class R2> bool operator<=(stationary_vector_iterator<T2, P2, R2, Bucket> const &other) const { return !this->operator> (other); }
	template<class T2, class P2, class R2> bool operator>=(stationary_vector_iterator<T2, P2, R2, Bucket> const &other) const { return !this->operator< (other); }
	this_type &operator+=(difference_type value) { bucket_advance(const_cast<bucket_type const *&>(this->b), this->p, value); return *this; }
	this_type &operator-=(difference_type const value) { return this->operator+=(-value); }
	this_type &operator++() { ++this->p; if (this->p == this->b->items_end()) { ++this->b; this->p = this->b->items_begin(); } return *this; }
	this_type &operator--() { if (this->p == this->b->items_begin()) { --this->b; this->p = this->b->items_end(); } --this->p; return *this; }
	this_type  operator++(int) { this_type self(*this); ++this->operator++(); return self; }
	this_type  operator--(int) { this_type self(*this); --this->operator--(); return self; }
	this_type  operator+ (difference_type const value) const { this_type result(*this); result += value; return result; }
	this_type  operator- (difference_type const value) const { this_type result(*this); result -= value; return result; }
	friend this_type operator+ (difference_type const value, this_type const &me) { return me + value; }
	template<class T2, class P2, class R2>
	difference_type operator- (stationary_vector_iterator<T2, P2, R2, Bucket> const other) const
	{
		typedef typename stationary_vector_iterator<T2, P2, R2, Bucket>::pointer pointer2;
		difference_type d;
		if (!this->b && !other.b)
		{
			d = difference_type();
		}
		else
		{
			assert(this->b && other.b);
			{
				bucket_pointer b1 = this->b, b2 = other.b;
				if (!(b1 < b2)) /* swap */ { bucket_pointer const b3 = b1; b1 = b2; b2 = b3; }
				for (d = difference_type(); b1 != b2; ++b1)
				{
					d += static_cast<difference_type>(b1->bucket_size());
				}
			}
			{
				bucket_pointer const b1 = this->b, b2 = other.b;
				pointer const p1 = this->p;
				pointer2 p2 = other.p;
				d = (p1 - static_cast<pointer const &>(b1->items_begin())) - (p2 - static_cast<pointer2 const &>(b2->items_begin())) + (b1 < b2 ? -d : +d);
			}
		}
		return d;
	}
	bucket_pointer b;
	pointer p;
protected:
	static ptrdiff_t bucket_advance(bucket_type const *&bucket, pointer &ptr, difference_type value) /* returns the number of buckets advanced */
	{
		bucket_type const *b = bucket;
		pointer p = ptr;
		difference_type const zero = difference_type();
		if (value > zero)
		{
			while (value)
			{
				difference_type const remaining = b->items_end() - p;
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
				difference_type const remaining = b->items_begin() - p;
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
		assert(!(b && p != b->items_begin() && p == b->items_end()) && "pointer must never point to the end of an array, but to the beginning of the next array");
		ptrdiff_t const result = b - bucket;
		bucket = b;
		ptr = p;
		return result;
	}
};

namespace detail
{
	template<class T, class Pointer, class Reference, class Bucket>
	struct iterator_partitioner<stationary_vector_iterator<T, Pointer, Reference, Bucket> >
	{
		typedef stationary_vector_iterator<T, Pointer, Reference, Bucket> base_iterator;
		typedef typename base_iterator::pointer iterator;
		typedef typename std::iterator_traits<iterator>::difference_type difference_type;
		base_iterator _begin, _end;
		iterator_partitioner() = default;
		explicit iterator_partitioner(base_iterator const &begin, base_iterator const &end) : _begin(begin), _end(end) { }
		base_iterator begin() const { return this->_begin; }
		base_iterator end() const { return this->_end; }
		bool empty() const { return this->_begin == this->_end; }
		iterator begin_front() const { return this->_begin.p; }
		void begin_front(iterator const &value) { this->_begin.p = value; }
		iterator end_front() const { return this->_begin.b == this->_end.b ? this->_end.p : static_cast<iterator const &>(this->_begin.b->items_end()); }
		difference_type front_distance() const { return this->end_front() - this->begin_front(); }
		void pop_front() { this->_begin.p = this->_begin.b == this->_end.b ? this->_end.p : static_cast<iterator const &>((++this->_begin.b)->items_begin()); }
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
		difference_type back_distance() const { return this->end_back() - this->begin_back(); }
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
		bool needs_decrement() const { return !this->_end.b->items_begin() || this->_end.b->items_begin() == this->_end.p; }
	};
}

template<class T, class Ax = std::allocator<T> >
class stationary_vector
{
	// Some of the complicated logic here is to allow for the invalidation of the 'end' iterator when inserting elements (and removing elements, wherever we choose to auto-deallocate).
	typedef stationary_vector this_type;
public:
	typedef typename std::conditional<std::is_same<typename std::remove_cv<Ax>::type, void>::value, std::allocator<T>, Ax>::type allocator_type;
	typedef detail::allocator_traits<allocator_type> allocator_traits;
	typedef T value_type;
	typedef typename allocator_traits::reference reference;
	typedef typename allocator_traits::const_reference const_reference;
	typedef typename allocator_traits::volatile_reference volatile_reference;
	typedef typename allocator_traits::const_volatile_reference const_volatile_reference;
	typedef stationary_vector_pointer<typename allocator_traits::pointer> pointer_wrapper;
	typedef stationary_vector_pointer<typename allocator_traits::const_pointer> const_pointer_wrapper;
	typedef stationary_vector_pointer<typename allocator_traits::volatile_pointer> volatile_pointer_wrapper;
	typedef stationary_vector_pointer<typename allocator_traits::const_volatile_pointer> const_volatile_pointer_wrapper;
	typedef typename pointer_wrapper::type pointer;
	typedef typename const_pointer_wrapper::type const_pointer;
	typedef typename volatile_pointer_wrapper::type volatile_pointer;
	typedef typename const_volatile_pointer_wrapper::type const_volatile_pointer;
	typedef typename allocator_traits::difference_type difference_type;
	typedef typename allocator_traits::size_type size_type;
	typedef stationary_vector_payload<pointer, difference_type, size_type> payload_type;
	typedef typename payload_type::bucket_type bucket_type;
	typedef stationary_vector_iterator<value_type, pointer, reference, bucket_type const> iterator;
	typedef stationary_vector_iterator<value_type const, const_pointer, const_reference, bucket_type const> const_iterator;
	typedef stationary_vector_iterator<value_type volatile, volatile_pointer, volatile_reference, bucket_type const> multithreading_iterator;
	typedef stationary_vector_iterator<value_type const volatile, const_volatile_pointer, const_volatile_reference, bucket_type const> const_multithreading_iterator;
	typedef detail::iterator_partitioner<iterator> partitioner_type;
	typedef detail::iterator_partitioner<const_iterator> const_partitioner_type;
	typedef iterator
#ifdef NDEBUG
		const &
#endif
		iterator_or_cref;
	typedef const_iterator
#ifdef NDEBUG
		const &
#endif
		const_iterator_or_cref;
	typedef multithreading_iterator
#ifdef NDEBUG
		const &
#endif
		multithreading_iterator_or_cref;
	typedef const_multithreading_iterator
#ifdef NDEBUG
		const &
#endif
		const_multithreading_iterator_or_cref;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	~stationary_vector()
	{
		this->reset();
		assert(this->capacity() == 0);
	}
	stationary_vector(this_type const &other)
		: stationary_vector(static_cast<this_type const &>(other), static_cast<allocator_type const &>(allocator_traits::select_on_container_copy_construction(other.allocator()))) { }
	stationary_vector(this_type const &other, allocator_type const &ax) : stationary_vector(ax) { this->initialize<const_iterator>(other.begin(), other.end()); }
	stationary_vector(this_type &&other) noexcept(true) : stationary_vector(std::move(other.allocator()), std::integral_constant<bool, true>())
	{
		// We can't call the move-with-allocator overload due to the noexcept specification on this one being stronger
		this->do_init(std::move(other), std::integral_constant<int, 2>());
	}
	stationary_vector(this_type &&other, allocator_type const &ax) noexcept(allocator_traits::is_always_equal::value) : stationary_vector(ax)
	{
		this->do_init(std::move(other), std::integral_constant<int, allocator_traits::is_always_equal::value ? 2 : 1>());
	}
	stationary_vector() noexcept(std::is_nothrow_default_constructible<allocator_type>::value) : stationary_vector(allocator_type()) { }
	explicit stationary_vector(allocator_type const &ax) noexcept(true) : stationary_vector(ax, std::integral_constant<bool, false>()) { }
	stationary_vector(std::initializer_list<value_type> items, allocator_type const &ax = allocator_type()) : stationary_vector(items.begin(), items.end(), ax) { }
	template<class It>
	explicit stationary_vector(It begin, size_type n, typename std::enable_if<
		detail::is_iterator<It, std::forward_iterator_tag>::value,
		allocator_type>::type const &ax = allocator_type()) : stationary_vector(ax)
	{
		It end(begin); std::advance<It>(end, static_cast<typename std::iterator_traits<It>::difference_type>(n));
		this->initialize<It>(std::forward<It>(begin), std::forward<It>(end));
	}
	template<class It>
	explicit stationary_vector(It begin, It end, typename std::enable_if<
		detail::is_iterator<It, std::input_iterator_tag>::value,
		allocator_type>::type const &ax = allocator_type()) : stationary_vector(ax) { this->initialize<It>(begin, end); }
	explicit stationary_vector(size_type const count, T const &value, allocator_type const &ax = allocator_type()) : stationary_vector(ax) { this->resize(count, value); }
	explicit stationary_vector(size_type const count, allocator_type const &ax = allocator_type()) : stationary_vector(ax) { this->resize(count); }
	template<class It>
	typename std::enable_if<(
		detail::is_iterator<It, std::input_iterator_tag>::value
		), void>::type append(It begin, It end)
	{
		begin = this->append_up_to_reallocation<It>(begin, end);
		if (begin != end)
		{
			assert(this->size() == this->capacity());
			bool is_forward_iterator = detail::is_iterator<It, std::forward_iterator_tag>::value;
			typename std::iterator_traits<It>::difference_type const d = std::distance(begin, is_forward_iterator ? end : begin);
			this_type temp(this->allocator());
			temp.reserve(this->calculate_next_bucket_size_from_capacity(this->size() + (d ? static_cast<size_type>(d) : static_cast<size_type>(1))));
			temp.initialize<It>(begin, end);
			this->payload().append(std::move(temp.payload()));
			typename outer_payload_type::atomic_size_type temp_size(0);
			using std::swap; swap(temp.outer.size, temp_size);
			this->outer.size += temp_size;
			assert(temp.payload().bucket_count() == 0);
			begin = end;
		}
	}
	template<class It>
	typename std::enable_if<(
		detail::is_iterator<It, std::input_iterator_tag>::value
		), void>::type append(It begin, It end, size_type reps)
	{
		if (reps && begin != end)
		{
			size_type const m = this->size();
			append_guard revert(this, m);
			this->append<It>(begin, end);
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
		detail::is_iterator<It, std::input_iterator_tag>::value
		), It>::type append_up_to_reallocation(It begin, It end)
	{
		iterator i = this->end();
		typedef detail::iterator_partitioner<It> Partitioner;
		if (i.p)
		{
			for (Partitioner parts(begin, end); !parts.empty() && i.p != i.b->items_end(); i.p == i.b->items_end() ? (void)(i.p = (++i.b)->items_begin()) : void())
			{
				std::pair<typename Partitioner::iterator, pointer> endpoint = detail::uninitialized_copy_up_to_n(parts.begin_front(), parts.end_front(), i.b->items_end() - i.p, i.p, this->allocator());
				this->outer.size += static_cast<size_type>(endpoint.second - i.p);
				i.p = endpoint.second;
				if (endpoint.first != parts.end_front())
				{ parts.begin_front(endpoint.first); }
				else
				{ parts.pop_front(); }
				begin = parts.begin();
			}
		}
		return begin;
	}
	template<class It>
	typename std::enable_if<(
		detail::is_iterator<It, std::input_iterator_tag>::value
		), void>::type initialize(It begin, It end)
	{
		assert(this->empty() && "initialize() may fail on non-empty due to aliasing issues; use assign() for potentially non-empty containers.");
		for (bool first_run = true; begin != end; first_run = false)
		{
			if (!first_run)
			{
				bool is_forward_iterator = detail::is_iterator<It, std::forward_iterator_tag>::value;
				typename std::iterator_traits<It>::difference_type d = is_forward_iterator ? std::distance(begin, end) : typename std::iterator_traits<It>::difference_type();
				this->reserve(this->size() + (d ? static_cast<size_type>(d) : static_cast<size_type>(1)));
			}
			begin = this->append_up_to_reallocation(begin, end);
			assert(begin == end || this->size() == this->capacity());
		}
	}
	template<class It>
	typename std::enable_if<(
		detail::is_iterator<It, std::input_iterator_tag>::value
		), void>::type assign(It begin, It end)
	{
		std::pair<It, iterator> limits = detail::partitioned_copy_limited(std::forward<It>(begin), end, this->begin(), this->end());
		this->pop_back_n(static_cast<size_type>(this->end() - limits.second));
		this->append<It>(std::forward<It>(limits.first), end);
	}
	void assign(std::initializer_list<value_type> value)
	{
		return this->assign<value_type const *>(value.begin(), value.end());
	}
	void assign(size_type n, value_type const &value)
	{
		size_type const m = this->size();
		iterator mid = this->begin(); mid += static_cast<difference_type>(n < m ? n : m);
		std::fill(this->begin(), mid, value) /* PEF: std::fill() */;
		this->append<value_type const *>(std::addressof(value) + 0, std::addressof(value) + 1, n - (n < m ? n : m));
		this->pop_back_n(this->size() - n);
	}
	reference at(size_type const i) { this->check_index(i); return this->operator[](i); }
	const_reference at(size_type const i) const { this->check_index(i); return this->operator[](i); }
	reference back() noexcept(true) { return this->operator[](this->size() - 1); }
	const_reference back() const noexcept(true) { return this->operator[](this->size() - 1); }
	iterator                      begin()                { payload_type                &payload = this->payload(); iterator                      result(                                payload.bucket_address(size_t()) , payload.bucket_begin(size_t())); return result; }
	const_iterator                begin() const          { payload_type const          &payload = this->payload(); const_iterator                result(                                payload.bucket_address(size_t()) , payload.bucket_begin(size_t())); return result; }
	multithreading_iterator       begin()       volatile { payload_type       volatile &payload = this->payload(); multithreading_iterator       result(const_cast<bucket_type       *>(payload.bucket_address(size_t())), payload.bucket_begin(size_t())); return result; }
	const_multithreading_iterator begin() const volatile { payload_type const volatile &payload = this->payload(); const_multithreading_iterator result(const_cast<bucket_type const *>(payload.bucket_address(size_t())), payload.bucket_begin(size_t())); return result; }
	pointer begin_ptr() { return this->begin().p; }
	const_pointer begin_ptr() const { return this->begin().p; }
	size_type capacity() const { payload_type const &payload = this->payload(); return payload.bucket_ibegin(payload.bucket_count()); }
	const_iterator cbegin() const { return this->begin(); }
	const_iterator cend() const { return this->end(); }
	void clear() noexcept(true) { this->pop_back_n(this->size()); }
	const_reverse_iterator crbegin() const { return this->rbegin(); }
	const_reverse_iterator crend() const { return this->rend(); }
	value_type *data() /* DATA IS NOT CONTIGUOUS! DO NOT USE THIS! */ { return allocator_traits::ptr_address(this->allocator(), this->begin_ptr()); }
	value_type const *data() const /* DATA IS NOT CONTIGUOUS! DO NOT USE THIS! */ { return allocator_traits::ptr_address(this->allocator(), this->begin_ptr()); }
	template<class... Args>
	iterator emplace(const_iterator_or_cref pos, Args &&... args) { return this->emplace<Args...>(this->mutable_iterator(pos), std::forward<Args &&>(args)...); }
	template<class... Args>
	iterator emplace(iterator pos, Args &&... args)
	{
		if (this->is_at_end(pos))
		{
			this->emplace_back<Args...>(std::forward<Args>(args)...);
			pos = this->end(); --pos;
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
		size_type const size = this->size(), new_size = size + 1;
		if (this->capacity() == size)
		{
			this->reserve(new_size);
		}
		allocator_type &ax = this->allocator();
		pointer p = this->end_ptr();
		allocator_traits::construct(ax, allocator_traits::ptr_address(ax, p), std::forward<Args>(args)...);
		++this->outer.size;
		return *p;
	}
	void emplace_back_n(size_type n /* NOTE: There's no variadic version of this function because this is optimized for cases where an allocator-aware version of std::uninitialized_value_construct can be called. */)
	{
		size_type const m = this->size();
		if (this->capacity() < m + n)
		{
			this->reserve(m + n);
		}
		append_guard revert(this, m);
		iterator i = this->nth(m);
		while (n)
		{
			pointer p_end = i.b->items_end();
			if (p_end - i.p > static_cast<difference_type>(n)) { p_end = i.p; p_end += static_cast<difference_type>(n); }
			detail::uninitialized_value_construct(i.p, p_end, this->allocator());
			size_type const delta = static_cast<size_type>(p_end - i.p);
			this->outer.size += delta;
			n -= delta;
			if (!n) { break; }
			++i.b;
			i.p = i.b->items_begin();
		}
		revert.release();
	}
	bool empty() const          noexcept(true) { return !this->size(); }
	bool empty() const volatile noexcept(true) { return !this->size(); }
	iterator end() { return this->mutable_iterator(static_cast<this_type const *>(this)->end()); }
	const_iterator end() const { return this->nth(this->size()); }
	pointer end_ptr() { return this->end().p; }
	const_pointer end_ptr() const { return this->end().p; }
	iterator erase(const_iterator_or_cref begin, const_iterator_or_cref end) { return this->erase(this->mutable_iterator(begin), this->mutable_iterator(end)); }
	iterator erase(const_iterator_or_cref pos) { return this->erase(this->mutable_iterator(pos)); }
	iterator erase(iterator_or_cref pos) { iterator end(pos); ++end; return this->erase(pos, end); }
	iterator erase(iterator begin, iterator end)
	{
		if (difference_type const d = end - begin)
		{
			bool const is_end = this->is_at_end(end);
			iterator my_end = this->end(), my_new_end = my_end; my_new_end -= d;
			typedef std::move_iterator<iterator> MoveIt;
			detail::partitioned_copy_limited(MoveIt(end), MoveIt(my_end), begin, my_new_end);
			this->pop_back_n(static_cast<size_type>(d));
			if (is_end) { begin = this->end(); }
			end = begin;
		}
		return begin;
	}
	reference front() noexcept(true) { return *this->begin_ptr(); }
	const_reference front() const noexcept(true) { return *this->begin_ptr(); }
	allocator_type get_allocator() const { return this->allocator(); }
	size_type index_of(const_iterator_or_cref                pos) const          { return static_cast<size_type>(pos - this->begin()); }
	size_type index_of(const_multithreading_iterator_or_cref pos) const volatile { return static_cast<size_type>(pos - this->begin()); }
	bool is_at_begin(const_iterator_or_cref i) const { return i.p == this->begin_ptr(); }
	bool is_at_end(const_iterator_or_cref i) const { return i.p == this->end_ptr(); }
	template<class It>
	typename std::enable_if<detail::is_iterator<It, std::input_iterator_tag>::value, iterator>::type insert(iterator pos, It begin, It end)
	{
		return this->do_insert<It, std::false_type>(std::forward<iterator>(pos), std::forward<It>(begin), std::forward<It>(end), std::false_type());
	}
	template<class It>
	typename std::enable_if<detail::is_iterator<It, std::input_iterator_tag>::value, iterator>::type insert(iterator pos, It begin, It end, size_type reps)
	{
		return this->do_insert<It, size_type>(std::move(pos), std::forward<It>(begin), std::forward<It>(end), reps);
	}
	template<class It>
	typename std::enable_if<detail::is_iterator<It, std::input_iterator_tag>::value, iterator>::type insert(const_iterator_or_cref pos, It begin, It end)
	{
		return this->insert<It>(this->mutable_iterator(pos), std::forward<It>(begin), std::forward<It>(end));
	}
	template<class It>
	typename std::enable_if<detail::is_iterator<It, std::input_iterator_tag>::value, iterator>::type insert(const_iterator_or_cref pos, It begin, It end, size_type const reps)
	{
		return this->insert<It>(this->mutable_iterator(pos), std::forward<It>(begin), std::forward<It>(end), reps);
	}
	iterator insert(const_iterator_or_cref pos, const_iterator_or_cref begin, const_iterator_or_cref end) { return this->insert<const_iterator>(pos, begin, end); }
	iterator insert(const_iterator_or_cref pos, const_iterator_or_cref begin, const_iterator_or_cref end, size_type const reps) { return this->insert<const_iterator>(pos, begin, end, reps); }
	iterator insert(const_iterator_or_cref pos, value_type const &value) { return this->emplace<value_type const &>(pos, value); }
	iterator insert(const_iterator_or_cref pos, size_type const reps, value_type const &value)
	{
		return this->insert<value_type const *>(pos, std::addressof(value) + 0, std::addressof(value) + 1, reps);
	}
	iterator insert(const_iterator_or_cref pos, value_type &&value) { return this->emplace<value_type &&>(pos, std::move(value)); }
	iterator insert(const_iterator_or_cref pos, std::initializer_list<value_type> ilist) { return this->insert<value_type const *>(pos, ilist.begin(), ilist.end()); }
	size_type max_size() const
	{
		size_type result = allocator_traits::max_size(this->allocator());
		size_type const diffmax = static_cast<size_type>((std::numeric_limits<difference_type>::max)());
		if (result > diffmax) { result = diffmax; }
		return result;
	}
	iterator nth(size_type const i) { return this->mutable_iterator(static_cast<this_type const *>(this)->nth(i)); }
	const_iterator nth(size_type const i) const
	{
		/* COPY of below (volatile) -- keep in sync! */
		payload_type const &payload = this->payload();
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
	pointer                nth_ptr(size_type const i)                { return this->payload().unsafe_index(i); }
	const_pointer          nth_ptr(size_type const i) const          { return this->payload().unsafe_index(i); }
	volatile_pointer       nth_ptr(size_type const i)       volatile { return this->payload().unsafe_index(i); }
	const_volatile_pointer nth_ptr(size_type const i) const volatile { return this->payload().unsafe_index(i); }
	// We make these friend functions so that forced template instantiation doesn't instantiate them. (Otherwise they might not compile..)
	friend bool operator< (this_type const &a, this_type const &b)
	{
		// PERF: Overload std::lexicographical_compare so that this works for sub-ranges too.
		typedef detail::iterator_partitioner<const_iterator> Partitioner;
		bool result = false;
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
				result = a_part_size < b_part_size;
				break;
			}
			typename Partitioner::iterator i1 = a_parts.begin_front(), j1 = i1; j1 += part_size;
			typename Partitioner::iterator i2 = b_parts.begin_front(), j2 = i2; j2 += part_size;
			result = std::lexicographical_compare(i1, j1, i2, j2);
			if (result || std::lexicographical_compare(i2, j2, i1, j1) || !part_size)
			{
				break;
			}
			a_parts.begin_front(j1);
			b_parts.begin_front(j2);
			if (part_size == a_part_size) { a_parts.pop_front(); }
			if (part_size == b_part_size) { b_parts.pop_front(); }
		}
		assert(result == std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end()));
		return result;
	}
	friend bool operator> (this_type const &a, this_type const &b) { return (b < a); }
	friend bool operator<=(this_type const &a, this_type const &b) { return !(a > b); }
	friend bool operator>=(this_type const &a, this_type const &b) { return !(a < b); }
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
				difference_type const a_part_size = a_parts.front_distance();
				difference_type const b_part_size = b_parts.front_distance();
				difference_type const part_size = b_part_size < a_part_size ? b_part_size : a_part_size;
				if (!part_size)
				{
					assert(a_parts.empty() || b_parts.empty());
					result &= a_part_size == b_part_size;
					break;
				}
				typename Partitioner::iterator i1 = a_parts.begin_front(), j1 = i1; j1 += part_size;
				typename Partitioner::iterator i2 = b_parts.begin_front(), j2 = i2; j2 += part_size;
				result &= std::equal(i1, j1, i2);
				if (!result)
				{
					break;
				}
				a_parts.begin_front(j1);
				b_parts.begin_front(j2);
				if (part_size == a_part_size) { a_parts.pop_front(); }
				if (part_size == b_part_size) { b_parts.pop_front(); }
			}
		}
		assert(result == (a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin())));
		return result;
	}
	friend bool operator!=(this_type const &a, this_type const &b) { return !(a == b); }
	this_type &operator =(std::initializer_list<value_type> items) { this->assign(items.begin(), items.end()); return *this; }
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
				detail::swap_if<allocator_type>(temp, this->allocator(), std::integral_constant<bool, allocator_traits::propagate_on_container_copy_assignment::value>());
			}
		}
		return *this;
	}
	this_type &operator =(this_type &&other) noexcept(allocator_traits::propagate_on_container_move_assignment::value || allocator_traits::is_always_equal::value)
	{
		if (this != &other)
		{
			this->do_assign(std::move(other), typename std::integral_constant<int, allocator_traits::is_always_equal::value ? 2 : (allocator_traits::propagate_on_container_move_assignment::value ? 1 : 0)>::type());
		}
		return *this;
	}
	reference                operator[](size_type const i)                noexcept(true) { assert(i < this->size()); return *this->nth_ptr(i); }
	const_reference          operator[](size_type const i) const          noexcept(true) { assert(i < this->size()); return *this->nth_ptr(i); }
	volatile_reference       operator[](size_type const i) volatile       noexcept(true) { return *this->nth_ptr(i); }
	const_volatile_reference operator[](size_type const i) const volatile noexcept(true) { return *this->nth_ptr(i); }
	void pop_back()
	{
		allocator_type &ax = this->allocator();
		iterator i = this->end(); --i;
		bool const consider_shrinking = i.p == i.b->items_begin();
		allocator_traits::destroy(ax, allocator_traits::address(ax, *i));
		--this->outer.size;
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
		this->outer.size -= n;
		this->shrink_to_fit(true);
	}
	reference push_back(value_type const &value) { return this->emplace_back(value); }
	reference push_back(value_type &&value) { return this->emplace_back(std::move(value)); }
	reverse_iterator rbegin() { return reverse_iterator(this->end()); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(this->end()); }
	reverse_iterator rend() { return reverse_iterator(this->begin()); }
	const_reverse_iterator rend() const { return const_reverse_iterator(this->begin()); }
	size_type calculate_next_bucket_size_from_capacity(size_type const minimum_total_capacity) const
	{
		size_type result = size_type();
		if (minimum_total_capacity > this->capacity())
		{
			if (minimum_total_capacity > this->max_size())
			{
#if defined(_MSC_VER) && defined(_CRT_STRINGIZE)
				__if_exists(_Xlength_error)
				{
					_Xlength_error(_CRT_STRINGIZE(stationary_vector<T>) " too long");
				}
				__if_not_exists(_Xlength_error)
#endif
				{
					typedef std::vector<size_t> V;
					V v;
					typename V::size_type max_size = v.max_size();
					if (max_size < (std::numeric_limits<typename V::size_type>::max)()) { max_size = (std::numeric_limits<typename V::size_type>::max)(); }
					if (max_size + 1 > max_size) { ++max_size; }
					if (max_size != v.max_size()) { v.reserve(max_size); } /* throw std::length_error(...) */
				}
			}
			payload_type const &payload = this->payload();
			size_type const m = payload.bucket_index(minimum_total_capacity);
			if (m >= payload.bucket_count() && minimum_total_capacity)
			{
				size_type
					preceding_bucket_size = m >= 1 ? payload.bucket_size(m - 1) : size_type(),
					desired_extra_size = minimum_total_capacity - payload.bucket_ibegin(m);
				size_type const min_size =
#if !defined(_DEBUG) && defined(NDEBUG) && !(defined(STATIONARY_VECTOR_DEBUG) && ((2 * STATIONARY_VECTOR_DEBUG + 1) - 1))
					(!m ? (sizeof(this_type) + sizeof(value_type) - 1) / sizeof(value_type) /* Performance optimization to allocate at least as much as the size of the vector */ : 0) +
#endif
					0;
				if (desired_extra_size < min_size) { desired_extra_size = min_size; }
				result = (m > 1 ? preceding_bucket_size : 0) + (desired_extra_size < preceding_bucket_size ? preceding_bucket_size : desired_extra_size);
			}
		}
		return result;
	}
	void reserve(size_type const n)
	{
		size_type desired_bucket_size = this->calculate_next_bucket_size_from_capacity(n);
#if (__cplusplus >= 202002L || defined(__cpp_if_constexpr)) && defined(_MSC_VER) && defined(_CPPLIB_VER)
		if constexpr (detail::is_allocator_same_as_malloc<allocator_type>::value)
		{
			if (desired_bucket_size)
			{
				payload_type &payload = this->payload();
				size_type const m = payload.bucket_count();
				pointer const prev_alloc_begin = payload.alloc_begin(m - !!m), prev_alloc_end = payload.alloc_end(m - !!m);
				if (static_cast<void *>(prev_alloc_begin))
				{
					size_t const
						old_known_byte_size = static_cast<size_t>(reinterpret_cast<unsigned char const volatile *>(prev_alloc_end) - reinterpret_cast<unsigned char const volatile *>(prev_alloc_begin)),
						new_known_byte_size = old_known_byte_size + desired_bucket_size * sizeof(value_type);
					bool valid = true;
					__if_exists(_Big_allocation_threshold)
					{
						if (new_known_byte_size >= _Big_allocation_threshold)
						{
							valid = false;
						}
					}
					if (valid)
					{
						assert(!(new_known_byte_size % sizeof(value_type)));
						size_t const old_actual_size = _msize(static_cast<void *>(prev_alloc_begin));
						if (_expand(static_cast<void *>(prev_alloc_begin), new_known_byte_size) == static_cast<void *>(prev_alloc_begin))
						{
							payload.expand_bucket_size(m - !!m, new_known_byte_size / sizeof(value_type));
							desired_bucket_size = size_type();
						}
					}
				}
			}
		}
#endif
		if (desired_bucket_size)
		{
			this->push_bucket(desired_bucket_size);
		}
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
			auto p = static_cast<this_type & (this_type:: *)(size_type reps, value_type const &value)>(&this_type::append);
			(this->*p)(n - m, value);
		}
		else
		{
			this->pop_back_n(m - n);
		}
	}
	void shrink_to_fit() { return this->shrink_to_fit(false); }
	void shrink_to_fit(bool const advisory) /* Does NOT make elements contiguous! If you want that, move-construct + swap a fresh container. */
	{
		payload_type &payload = this->payload();
		size_t ibucket;
		pointer const p = payload.unsafe_index(this->size(), &ibucket);
		size_t min_buckets = ibucket + (p != payload.bucket_begin(ibucket)) + advisory;
		for (allocator_type &ax = this->allocator(); payload.bucket_count() > min_buckets; )
		{
			size_t const nb = payload.bucket_count() - 1;
			allocator_traits::deallocate(ax, payload.alloc_begin(nb), payload.alloc_size(nb));
			payload.pop_bucket();
		}
	}
	size_type size() const          noexcept(true) { return this->outer.size; }
	size_type size() const volatile noexcept(true) { return this->outer.size; }
	void swap(this_type &other)
		noexcept(allocator_traits::propagate_on_container_swap::value || noexcept(std::declval<this_type &>().swap_except_allocator(std::declval<this_type &>())) || allocator_traits::is_always_equal::value)
	{
		// Note: When propagate_on_container_swap is false, C++ requires allocators to be equal; otherwise, swapping is undefined behavior.
		return this->swap(other, std::integral_constant<bool, allocator_traits::propagate_on_container_swap::value>());
	}
	void swap(this_type &other, std::integral_constant<bool, true>) noexcept(true)
	{
		using std::swap; swap(this->outer.allocator(), other.outer.allocator());
		return this->swap(other, std::integral_constant<bool, false>());
	}
	void swap(this_type &other, std::integral_constant<bool, false>) noexcept(true)
	{
		this->swap_except_allocator(other);
	}
	friend void swap(this_type &a, this_type &b) noexcept(noexcept(std::declval<this_type &>().swap(std::declval<this_type &>()))) { return a.swap(b); }
	bool __invariants() const { return true /* TODO: Add invariants */; }
protected:
#if defined(__cpp_lib_is_layout_compatible)
	typedef unsigned char ensure_layout_compatibility[std::is_layout_compatible<const_iterator, iterator>::value];
#endif
	static pointer address(const_iterator_or_cref i)
	{
		/* HACK: There's no legal way to un-const arbitrary pointers, but the alternative seems to be code duplication everywhere... */
		return *reinterpret_cast<pointer const *>(reinterpret_cast<uintptr_t>(std::addressof(static_cast<const_pointer const &>(i.p))));
	}
	static iterator mutable_iterator(const_iterator_or_cref i)
	{
		/* HACK: There's no legal way to un-const arbitrary pointers, but the alternative seems to be code duplication everywhere... */
		return *reinterpret_cast<iterator const *>(reinterpret_cast<uintptr_t>(std::addressof(i)));
	}
	void check_index(size_type const i) const { if (i >= this->size()) { (void)std::vector<size_t>().at(i); } }
private:
	void _finish_append(size_type const d, size_type reps)
	{
		if (reps)
		{
			size_type extra_size = d;
			if (reps != 1) { extra_size *= reps; }
			this->reserve(this->size() + extra_size);
			bool first_run = false;
			iterator j = this->end();
			iterator i = j; i -= static_cast<difference_type>(d);
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
		typename Partitioner::iterator t = parts.end_back(); --t;
		parts.end_back(t);
		if (t == parts.begin_back()) { parts.pop_back(); }
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
	void push_bucket(size_type const desired_bucket_size)
	{
		payload_type &payload = this->payload();
		size_type const m = payload.bucket_count();
		assert(m == payload.bucket_index(this->size() + desired_bucket_size));
		allocator_type &ax = this->allocator();
		detail::heap_ptr<allocator_type> ptr(ax, desired_bucket_size, payload.alloc_end(m - !!m));
		pointer begin(pointer_wrapper::create(ptr.get(), desired_bucket_size, size_t()));
		pointer end(begin); end += static_cast<difference_type>(desired_bucket_size);
		payload.set_bucket(m, begin, end, payload.bucket_iend(m - !!m) + desired_bucket_size);
		assert(payload.bucket_size(m) == payload.bucket_iend(m) - payload.bucket_ibegin(m));
		ptr.release();
	}
	void reset() { this->clear(); this->shrink_to_fit(false); }
	void swap_except_allocator(this_type &other) noexcept(true)
	{
		using std::swap;
		swap(this->outer.inner, other.outer.inner);
		swap(this->outer.size, other.outer.size);
	}
	template<class It>
	void append(It begin, It end, std::false_type)
	{
		return this->append<It>(std::forward<It>(begin), std::forward<It>(end));
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
	typename std::enable_if<std::is_same<Reps, size_type>::value || std::is_same<Reps, std::false_type>::value, iterator>::type do_insert(iterator pos, It begin, It end, Reps reps)
	{
		if (begin != end)
		{
			size_type const m = this->size();
			bool const is_begin = this->is_at_begin(pos), is_end = this->is_at_end(pos);
			if (!is_begin) { --pos; }
			this->append<It>(begin, end, std::forward<Reps>(reps));
			assert(((void)(end = begin), true)) /* Make explicit that the iterators might invalidated at this point, because they might've pointed into this container. */;
			if (!is_begin) { ++pos; }
			if (is_end)
			{
				if (is_begin)
				{
					pos = this->nth(m);
				}
			}
			else
			{
				if (this->size() - m == 1)
				{
					detail::allocator_temporary<allocator_type> temp_alloc(this->allocator(), std::move(this->back())); this->pop_back();
					pos = this->_finish_insert(pos, std::move(temp_alloc.get()));
				}
				else
				{
					// Only rotate when it's difficult to avoid, since it's inefficient.
					iterator mid = this->begin(); mid += static_cast<difference_type>(m);
					detail::rotate(this->allocator(), pos, mid, this->end());
				}
			}
		}
		return pos;
	}
	void do_assign(this_type &&other, std::integral_constant<int, 2> const) noexcept(true)  // is_always_equal
	{
		assert(this != &other && "if (this != &other) must have already been checked");
		this->swap(other, std::integral_constant<bool, allocator_traits::propagate_on_container_move_assignment::value>());
		other.reset();
	}
	void do_assign(this_type &&other, std::integral_constant<int, 1> const) noexcept(noexcept(::new(static_cast<void *>(NULL)) this_type(std::declval<this_type &&>())))  // propagate_on_container_move_assignment
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
	struct outer_payload_type : public allocator_type
	{
		typedef typename detail::atomic_traits_of<size_type>::atomic_type atomic_size_type;
		explicit outer_payload_type(allocator_type &&ax) : allocator_type(std::move(ax)), inner(), size(0) { }
		explicit outer_payload_type(allocator_type const &ax) : allocator_type(ax), inner(), size(0) { }
		allocator_type &allocator() { return *this; }
		allocator_type const &allocator() const { return *this; }

		payload_type inner;
		atomic_size_type size;
		// *** STOP!!! WHEN ADDING FIELDS -- Make SURE to update move/copy/swap constructors and assignments!
	};
	class append_guard
	{
		append_guard(append_guard const &);
		append_guard &operator =(append_guard const &);
		this_type *me;
		size_type m;
	public:
		explicit append_guard(this_type *me, size_type m) : me(me), m(m) { }
		~append_guard()
		{
			if (me)
			{
				me->pop_back_n(me->size() - m);
			}
		}
		void release() { this->me = NULL; }
	};
	outer_payload_type outer;
	payload_type                &payload()                { return this->outer.inner; }
	payload_type const          &payload() const          { return this->outer.inner; }
	payload_type       volatile &payload()       volatile { return this->outer.inner; }
	payload_type const volatile &payload() const volatile { return this->outer.inner; }
	allocator_type       &allocator()       { return this->outer.allocator(); }
	allocator_type const &allocator() const { return this->outer.allocator(); }
	template<bool Move>
	explicit stationary_vector(typename std::conditional<Move, allocator_type &&, allocator_type const &>::type ax, std::integral_constant<bool, Move>) noexcept(true) : outer(std::move(ax)) { }
};

#if __cplusplus >= 202002L || defined(__cpp_deduction_guides)
template<class Ax> stationary_vector(Ax const &)->stationary_vector<typename detail::allocator_traits<Ax>::value_type, Ax>;
template<class It> stationary_vector(It, It)->stationary_vector<typename std::iterator_traits<It>::value_type>;
template<class It, class Ax> stationary_vector(It, It, Ax const &)->stationary_vector<typename std::iterator_traits<It>::value_type, Ax>;
template<class T> stationary_vector(size_t, T)->stationary_vector<T>;
template<class T, class Ax> stationary_vector(std::initializer_list<T>, Ax)->stationary_vector<T, Ax>;
template<class Ax> stationary_vector(typename detail::allocator_traits<Ax>::size_type, typename detail::allocator_traits<Ax>::value_type, Ax)->stationary_vector<typename detail::allocator_traits<Ax>::value_type, Ax>;
template<class Ax> stationary_vector(typename detail::allocator_traits<Ax>::value_type, typename std::iterator_traits<Ax>::value_type, Ax const &)->stationary_vector<typename detail::allocator_traits<Ax>::value_type, Ax>;
#endif

template<class T, class Ax, class U>
#ifdef _LIBCPP_VERSION
void
#else
typename stationary_vector<T, Ax>::size_type
#endif
erase(stationary_vector<T, Ax> &c, U const &value)
{
	// PERF: Optimize erase()
	typename stationary_vector<T, Ax>::iterator it = std::remove(c.begin(), c.end(), value);
	typename stationary_vector<T, Ax>::difference_type r = std::distance(it, c.end());
	c.erase(it, c.end());
	return
#ifndef _LIBCPP_VERSION
		r
#endif
		;
}

template<class T, class Ax, class Pred>
#ifdef _LIBCPP_VERSION
void
#else
typename stationary_vector<T, Ax>::size_type
#endif
erase_if(stationary_vector<T, Ax> &c, Pred pred)
{
	// PERF: Optimize erase_if()
	typename stationary_vector<T, Ax>::iterator it = std::remove_if(c.begin(), c.end(), std::forward<Pred>(pred));
	typename stationary_vector<T, Ax>::difference_type r = std::distance(it, c.end());
	c.erase(it, c.end());
	return
#ifndef _LIBCPP_VERSION
		r
#endif
		;
}

#if defined(STATIONARY_VECTOR_END_NAMESPACE)
STATIONARY_VECTOR_END_NAMESPACE
#else
}
}
#endif

#endif

