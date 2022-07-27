#pragma once

#ifndef VIT_CORE_ENUM_HPP
#define VIT_CORE_ENUM_HPP

// Requires C++14 or later

#if defined(VIT_ENUM_NAMESPACE)
#if !defined(VIT_ENUM_BEGIN_NAMESPACE)
#define VIT_ENUM_BEGIN_NAMESPACE namespace VIT_ENUM_NAMESPACE {
#endif
#if !defined(VIT_ENUM_END_NAMESPACE)
#define VIT_ENUM_END_NAMESPACE }
#endif
#else
#define VIT_ENUM_NAMESPACE vit::core
#define VIT_ENUM_BEGIN_NAMESPACE namespace vit { namespace core {
#define VIT_ENUM_END_NAMESPACE } }
#endif

#include <stddef.h>

#include <iosfwd>
#include <type_traits>
#include <utility>

#if 201703L <= __cplusplus
#include <string_view>
#endif

#if defined(_CPPLIB_VER) && (!defined(_MSVC_STL_VERSION) || _MSVC_STL_VERSION < 142)
#pragma message(__FILE__ ": " "warning: This header may trigger various issues under Visual C++ 2017 or earlier; Visual Studio 2019 or later is highly recommended")
#endif

VIT_ENUM_BEGIN_NAMESPACE

template<class T>
struct enum_member
{
	typedef std::underlying_type_t<T> underlying_type;

	T value;

	struct value_less
	{
		constexpr bool operator()(enum_member const &m1, enum_member const &m2) const
		{
			return m1.underlying_value() < m2.underlying_value();
		}
	};

	constexpr underlying_type underlying_value() const { return static_cast<underlying_type>(this->value); }
	constexpr void underlying_value(underlying_type v) { this->value = static_cast<T>(v); }
};

namespace detail
{

template<bool Signed, unsigned char Size> struct enum_int;
template<> struct enum_int<true, sizeof(signed char)> { typedef signed char type; };
template<> struct enum_int<false, sizeof(unsigned char)> { typedef unsigned char type; };
template<> struct enum_int<true, sizeof(signed short)> { typedef signed short type; };
template<> struct enum_int<false, sizeof(unsigned short)> { typedef unsigned short type; };
template<> struct enum_int<true, sizeof(signed int)> { typedef signed int type; };
template<> struct enum_int<false, sizeof(unsigned int)> { typedef unsigned int type; };
template<> struct enum_int<true, sizeof(signed long long)> { typedef signed long long type; };
template<> struct enum_int<false, sizeof(unsigned long long)> { typedef unsigned long long type; };

#if (201703L <= __cplusplus || defined(__cpp_lib_string_view)) && !(defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION < 142 /* There seems to be a bug in VS2017's string_view */)
typedef std::string_view enum_name_view;
#else
struct enum_name_view
{
	char const *s = nullptr;
	size_t n = 0;
	constexpr explicit enum_name_view() = default;
	constexpr enum_name_view(char const *name, size_t name_length = ~size_t()) : s(name), n(name_length)
	{
		if (this->n == ~size_t())
		{
			this->n = 0;
			if (this->s != nullptr)
			{
				while (this->s[this->n] != '\0')
				{
					++this->n;
				}
			}
		}
	}
	constexpr char const *begin() const { return this->s; }
	constexpr char const *end() const { return this->begin() + this->n; }
	constexpr char const *data() const { return this->begin(); }
	constexpr size_t size() const { return static_cast<size_t>(this->end() - this->begin()); }
	constexpr char const &operator[](size_t i) const { return this->data()[static_cast<ptrdiff_t>(i)]; }
	constexpr size_t find_first_not_of(enum_name_view const s, size_t start_index = 0) const
	{
		size_t result = ~size_t();
		for (char const *i = this->begin() + static_cast<ptrdiff_t>(start_index), *j = this->end(); i < j; ++i)
		{
			bool found = false;
			for (size_t k = 0; k < s.size(); ++k)
			{
				if (s[k] == *i)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				result = static_cast<size_t>(i - this->begin());
				break;
			}
		}
		return result;
	}
	constexpr int compare(enum_name_view const &other) const
	{
		int c = 0;
		for (size_t i = 0, j = 0; i < this->n && j < other.n; ++i, ++j)
		{
			char const c1 = (*this)[i];
			char const c2 = other[j];
			if (c1 != c2)
			{
				c = c1 < c2 ? -1 : +1;
				break;
			}
		}
		if (c == 0)
		{
			c = this->n <= other.n ? other.n <= this->n ? 0 : -1 : +1;
		}
		return c;
	}
	constexpr bool operator<(enum_name_view const &other) const { return this->compare(other) < 0; }
	constexpr bool operator==(enum_name_view const &other) const { return this->compare(other) == 0; }

	template<class Traits, class Alloc>
	friend std::basic_string<char, Traits, Alloc> &operator+=(std::basic_string<char, Traits, Alloc> &out, enum_name_view const &me)
	{
		out.append(me.data(), me.size());
		return out;
	}

	template<class Traits>
	friend std::basic_ostream<char, Traits> &operator<<(std::basic_ostream<char, Traits> &out, enum_name_view const &me)
	{
		out.write(me.data(), static_cast<std::streamsize>(me.size()));
		return out;
	}
};
#endif

template<class T = void>
struct enum_singleton_factory;

template<>
struct enum_singleton_factory<void>
{
	enum : bool
	{
		have_inline_variables =
#if 201703L <= __cplusplus || defined(__cpp_inline_variables)
		true
#else
		false
#endif
	};
};

template<class T>
struct enum_singleton_factory<T const> : protected enum_singleton_factory<>
{
	static constexpr T create()
	{
		return T();
	}
};

template<class T>
struct enum_singleton_factory : public enum_singleton_factory<T const>
{
private:
	typedef enum_singleton_factory<T const> base_type;

public:
	typedef T type;
	typedef std::conditional_t<base_type::have_inline_variables, type const &, type> result_type;

	static_assert(sizeof(type) /* suppress C4686 */, "invalid");

	static constexpr result_type get()
	{
		return _get();
	}

private:
#if 202002L <= __cplusplus || defined(__cpp_nontype_template_args) && __cpp_nontype_template_args >= 201911L
	static constexpr type const &_get()
	{
		return std::integral_constant<type, (void(), base_type::create())>::value;
	}
#elif 201703L <= __cplusplus || defined(__cpp_inline_variables)
	static constexpr type const value = base_type::create();
	static constexpr type const &_get()
	{
		return value;
	}
#else
	static constexpr type _get()
	{
		return base_type::create();
	}
#endif
};

template<class It, class T, class Less>
constexpr It enum_lower_bound(It begin, It end, T const &val, Less less)
{
	while (begin != end)
	{
		It mid(begin);
		mid += (end - begin) / 2;
		bool const b = less(*mid, val);
		(b ? begin : end) = mid + b;
	}
	return begin;
}

template<class It, class Less>
constexpr void enum_mergesort(It const begin, It const end, It const scratch, Less less)
{
	typedef decltype(end - begin) Diff;
	struct Merger : Less
	{
		constexpr explicit Merger(Less &&less) : Less(std::forward<Less>(less)) { }
		static constexpr Diff(min)(Diff a, Diff b)
		{
			return b < a ? b : a;
		}
		constexpr It operator()(It begin1, It const end1, It begin2, It const end2, It out)
		{
			for (;;)
			{
				bool const more1 = begin1 != end1;
				bool const more2 = begin2 != end2;
				if (more2 && (!more1 || this->Less::operator()(*begin2, *begin1)))
				{
					*out = std::move(*begin2);
					++begin2;
				}
				else if (more1)
				{
					*out = std::move(*begin1);
					++begin1;
				}
				else { break; }
				++out;
			}
			return out;
		}
	};

	bool sorted = true;
	if (begin != end)
	{
		It i = begin;
		It j = begin;
		++j;
		while (j != end)
		{
			if (less(*j, *i))
			{
				sorted = false;
				break;
			}
			i = j;
			++j;
		}
	}
	if (sorted)
	{
		return;
	}

	auto const &min = Merger::min;
	Merger merge(std::forward<Less>(less));
	bool in_buf = false;
	Diff const n = end - begin;
	for (Diff m = 1; m < n; in_buf = !in_buf, m += m)
	{
		for (Diff j0 = 0, k = n + n - m; j0 < k; j0 += m + m)
		{
			Diff j1 = j0 + m;
			Diff j2 = j0 + (m + m);
			Diff d2 = min(j2, n);
			Diff d1 = min(j1, n);
			Diff d0 = min(j0, n);
			in_buf
				? (void)merge(scratch + d0, scratch + d1, scratch + d1, scratch + d2, begin + d0)
				: (void)merge(begin + d0, begin + d1, begin + d1, begin + d2, scratch + d0);
		}
	}
	if (in_buf)
	{
		merge(end, end, scratch, scratch + (end - begin), begin) /* NOLINT(readability-suspicious-call-argument) */;
		in_buf = !in_buf;
	}
}

template<class T, class U>
struct enum_member_initializer : public enum_member<T>
{
	bool implicit = true;

	constexpr enum_member_initializer() : enum_member_initializer(U()) { }

	constexpr enum_member_initializer(U v) /* NOLINT(hicpp-explicit-conversions) */ : enum_member<T>()
	{
		this->value = static_cast<T>(v);
	}

	constexpr operator U() const /* NOLINT(hicpp-explicit-conversions) */ { return static_cast<U>(this->value); }

	constexpr enum_member_initializer &operator =(U const &other)
	{
		this->value = static_cast<T>(other);
		this->implicit = false;
		return *this;
	}
};

template<class T, class U>
struct enum_initializer
{
	enum_member_initializer<T, U> *p;
	T prev;

	constexpr enum_initializer &operator,(enum_member_initializer<T, U> &member)
	{
		if (member.implicit)
		{
			member.value = static_cast<T>(static_cast<U>(this->prev) + static_cast<U>(1));
		}
		*this->p++ = member;
		this->prev = member.value;
		return *this;
	}
};

static inline constexpr bool enum_extract_member_names(char const body[], size_t body_length, size_t name_offset_lengths[][2] /* MUST be zero-initialized before call */) /* NOLINT(readability-function-cognitive-complexity) */
{
	bool success = true;
	bool started_member = false;
	for (size_t i = 0, j = 0, nparens = 0; success && (body_length == ~size_t() ? (body[i] != '\0') : (i < body_length)); ++i)
	{
		char const ch = body[i];
		switch (ch)
		{
		case '{':
		case '}':
			if (nparens == 0)
			{
				success = false;
			}
			break;
		case '\'':
		case '\"':
			success = false;
			(void)success;
			throw;
		case '(':
			++nparens;
			break;
		case ')':
			--nparens;
			break;
		case '\0':
		case ',':
			if (nparens == 0)
			{
				++j;
				started_member = false;
			}
			break;
		case '=':
			break;
		case ' ':
			if (nparens == 0 && started_member && name_offset_lengths[j][0] == i)
			{
				++name_offset_lengths[j][0];
			}
			break;
		default:
			if (nparens == 0)
			{
				if (!started_member || (j > 0 && name_offset_lengths[j][0] == 0))
				{
					name_offset_lengths[j][0] = i;
				}
				if (name_offset_lengths[j][0] + 0U + name_offset_lengths[j][1] == i)
				{
					++name_offset_lengths[j][1];
				}
				started_member = true;
			}
			break;
		}
	}
	return success;
}

std::false_type _enum_flag_lookup_adl(void const *dummy);

template<class T>
struct enum_tag;

template<class T>
struct enum_tag
{
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif
	// Exploit C++ loophole: https://alexpolt.github.io/type-loophole.html
	friend auto _enum_traits_lookup_adl(enum_tag *tag);
	friend auto _enum_flag_lookup_adl(enum_tag *tag);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
};

template<class T, class Traits>
struct enum_friend : enum_tag<T>
{
	friend auto _enum_traits_lookup_adl(enum_tag<T> *tag) { (void)tag; return Traits((void(), *static_cast<Traits const *>(nullptr))); }
};

template<class T, bool V>
struct enum_flag : enum_tag<T>
{
	friend auto _enum_flag_lookup_adl(enum_tag<T> *tag) { (void)tag; return std::integral_constant<bool, V>(); }
};

extern void enum_flag_lookup(void const *dummy);

template<class T>
extern decltype(_enum_flag_lookup_adl(static_cast<enum_tag<T> *>(nullptr))) enum_flag_lookup(T *dummy);

template<class T>
struct enum_traits_wrapper;

template<class T>
using enum_base_traits = decltype(_enum_traits_lookup_adl(static_cast<enum_tag<T> *>(nullptr)));

template<class T>
struct enum_member_count : std::integral_constant<size_t, sizeof(enum_base_traits<T>) / sizeof(enum_member_initializer<T, std::underlying_type_t<T> >)> { };

}

template<class T>
using enum_traits = detail::enum_traits_wrapper<T>;

namespace detail
{

template<class T>
class enum_named_member;

template<class T>
class enum_members_info
{
	typedef enum_members_info this_type;
	typedef enum_base_traits<T> base_type;
	enum : size_t { N = enum_member_count<T>::value };

public:

	static constexpr auto calculate_sizes()
	{
		struct compile_time_only
		{
			size_t total_length, max_length;
			static constexpr compile_time_only calculate()
			{
				decltype(auto) body = base_type::_enum_create_body();
				size_t name_offset_lengths[N][2] = {};
				if (!detail::enum_extract_member_names(body, ~size_t(), name_offset_lengths))
				{
					throw;
				}
				compile_time_only result = { 0, 0 };
				for (size_t i = 0; i != N; ++i)
				{
					result.total_length += name_offset_lengths[i][1];
					if (result.max_length < name_offset_lengths[i][1])
					{
						result.max_length = name_offset_lengths[i][1];
					}
				}
				result.total_length += N;
				return result;
			}
		};
		constexpr compile_time_only result = compile_time_only::calculate();
		return result;
	}
	enum : size_t
	{
		body_length = calculate_sizes().total_length,
		max_offset = body_length,
		max_length = calculate_sizes().max_length,
	};

	typedef typename detail::enum_int<
		false,
		(void(), max_length) <= static_cast<unsigned char>(-1) ? sizeof(unsigned char) :
		(void(), max_length) <= static_cast<unsigned short>(-1) ? sizeof(unsigned short) :
		(void(), max_length) <= static_cast<unsigned int>(-1) ? sizeof(unsigned int) :
		sizeof(unsigned long long)
	>::type length_type;

	typedef typename detail::enum_int<
		false,
		(void(), max_offset) <= static_cast<unsigned char>(-1) ? sizeof(unsigned char) :
		(void(), max_offset) <= static_cast<unsigned short>(-1) ? sizeof(unsigned short) :
		(void(), max_offset) <= static_cast<unsigned int>(-1) ? sizeof(unsigned int) :
		sizeof(unsigned long long)
	>::type offset_type;

	// We use 'friend' here to prevent explicit template instantiation from instantiating this. Otherwise the full body string literal will get embedded in the binary.
	friend constexpr this_type _enum_create_body_without_values(this_type const *dummy)
	{
		(void)dummy;
		struct compile_time_only
		{
			static constexpr this_type create()
			{
				decltype(auto) body = base_type::_enum_create_body();
				this_type result = this_type();
				if (!detail::enum_extract_member_names(body, ~size_t(), result.name_offset_lengths))
				{
					throw;
				}
				size_t k = 0;
				for (size_t i = 0; i != N; ++i)
				{
					size_t const old_k = k;
					for (size_t j = 0, j0 = result.name_offset_lengths[i][0], jn = result.name_offset_lengths[i][1]; j < jn; ++j)
					{
						result._names[k] = body[j0 + j];
						++k;
					}
					result._names[k] = '\0';
					result.name_offset_lengths[i][0] = old_k;
					++k;
				}
				return result;
			}
		};
		constexpr this_type result = compile_time_only::create();
		return result;
	}

	constexpr decltype(auto) names() const { return (this->_names); }

	constexpr char const *name_c_str(enum_named_member<T> const &member) const { return &this->_names[member.name_offset]; }
	constexpr char const *name_c_str(enum_named_member<T> const *member) const { return member != nullptr ? this->name_c_str(*member) : nullptr; }

	constexpr detail::enum_name_view name(enum_named_member<T> const &member) const { return detail::enum_name_view(this->name_c_str(member), member.name_length); }
	constexpr detail::enum_name_view name(enum_named_member<T> const *member) const { return member != nullptr ? this->name(*member) : detail::enum_name_view(); }

	char _names[body_length];  // private member; it's only public to allow this type to be structural
	size_t name_offset_lengths[N][2];
};

template<class T>
struct enum_singleton_factory<enum_members_info<T> const> : protected enum_singleton_factory<>
{
	typedef enum_members_info<T> type;

	static constexpr type create()
	{
		return _enum_create_body_without_values(static_cast<type const *>(nullptr));
	}
};

template<class T>
class enum_named_member : public enum_member<T>
{
	typedef enum_named_member this_type;
public:
	typename enum_members_info<T>::offset_type name_offset;
	typename enum_members_info<T>::length_type name_length;

#if 201703L <= __cplusplus || (defined(__cpp_inline_variables) && defined(__cpp_lib_string_view))
	constexpr char const *name_c_str() const
	{
		return enum_traits<T>::members_info().names() + this->name_offset;
	}

	constexpr detail::enum_name_view name() const
	{
		return detail::enum_name_view(this->name_c_str(), this->name_length);
	}
#endif

	struct name_less
	{
		char const *base;

		constexpr bool operator()(this_type const &a, this_type const &b) const
		{
			return this->operator()(detail::enum_name_view(&base[a.name_offset], a.name_length), detail::enum_name_view(&base[b.name_offset], b.name_length));
		}

		constexpr bool operator()(this_type const &a, detail::enum_name_view const &b) const
		{
			return this->operator()(detail::enum_name_view(&base[a.name_offset], a.name_length), b);
		}

		constexpr bool operator()(detail::enum_name_view const &a, this_type const &b) const
		{
			return this->operator()(a, detail::enum_name_view(&base[b.name_offset], b.name_length));
		}

		constexpr bool operator()(detail::enum_name_view const &a, detail::enum_name_view const &b) const
		{
			return a < b;
		}
	};

	constexpr bool valid() const
	{
		return this->name_offset < enum_members_info<T>::max_offset;
	}
};

template<class T>
class enum_members
{
	typedef enum_members this_type;
	typedef enum_named_member<T> member_type;
public:
	member_type _members[enum_member_count<T>::value] = {};

	constexpr enum_members()
	{
		typedef enum_base_traits<T> base_type;
		decltype(auto) members_info = detail::enum_singleton_factory<enum_members_info<T> >::get();
		struct /* the extra struct wrapper avoids an ICE in MSVC 14.32.31326 under /std:c++14 */
		{
			detail::enum_member_initializer<T, std::underlying_type_t<T> > inits[enum_member_count<T>::value];
		} s = {};
		(void)static_cast<base_type>(s.inits);
		size_t o = 0;
		for (size_t i = 0; i != enum_member_count<T>::value; ++i)
		{
			member_type &member = (*this)[i];
			member.name_offset = static_cast<typename enum_members_info<T>::offset_type>(o);
			member.name_length = static_cast<typename enum_members_info<T>::length_type>(members_info.name_offset_lengths[i][1]);
			member.value = s.inits[i].value;
			o += members_info.name_offset_lengths[i][1];
			++o;
		}
	}

	constexpr member_type const &operator[](size_t i) const { return this->_members[i]; }
	constexpr member_type &operator[](size_t i) { return const_cast<member_type &>(static_cast<this_type const *>(this)->operator[](i)); }
	constexpr member_type const *begin() const { return this->_members + 0; }
	constexpr member_type *begin() { return const_cast<member_type *>(static_cast<this_type const *>(this)->begin()); }
	constexpr member_type const *end() const { return this->_members + this->size(); }
	constexpr member_type *end() { return const_cast<member_type *>(static_cast<this_type const *>(this)->end()); }
	enum : size_t { static_size = enum_member_count<T>::value };
	constexpr size_t size() const { return static_size; }
};

template<class T>
class enum_members_sorted_by_name : public enum_members<T>
{
	typedef enum_members_sorted_by_name this_type;
	typedef enum_members<T> base_type;
	typedef enum_named_member<T> member_type;

public:
	constexpr enum_members_sorted_by_name() : base_type(enum_singleton_factory<base_type>::get())
	{
		member_type scratch[enum_member_count<T>::value] = {};
		detail::enum_mergesort(this->begin(), this->end(), scratch, typename member_type::name_less{enum_traits<T>::members_info().names()});
	}

	template<decltype(nullptr) Dummy = nullptr /* This is just to avoid a template instantiation error */>
	constexpr member_type const *find(char const *name, size_t name_length = ~size_t(), detail::enum_members_info<T> const &members_info = std::enable_if_t<(sizeof(Dummy) > 0), enum_traits<T> >::members_info()) const
	{
		if (name_length == ~size_t())
		{
			name_length = 0;
			if (name != nullptr)
			{
				while (name[name_length] != '\0')
				{
					++name_length;
				}
			}
		}
		detail::enum_name_view needle(name, name_length);
		typename member_type::name_less less = { members_info.names() };
		member_type const *b = this->begin();
		member_type const *e = this->end();
		member_type const *f = detail::enum_lower_bound(b, e, needle, less);
		return f == e || less(needle, *f) ? nullptr : f;
	}
};

template<class T>
class enum_members_sorted_by_value : public enum_members<T>
{
	typedef enum_members_sorted_by_value this_type;
	typedef enum_members<T> base_type;
	typedef enum_named_member<T> member_type;

public:
	constexpr enum_members_sorted_by_value() : base_type(enum_singleton_factory<base_type>::get())
	{
		member_type scratch[enum_member_count<T>::value] = {};
		detail::enum_mergesort(this->begin(), this->end(), scratch, typename member_type::value_less());
	}

	constexpr member_type const *find(T value) const
	{
		member_type needle = member_type();
		needle.value = value;
		typename member_type::value_less less = { };
		member_type const *b = this->begin();
		member_type const *e = this->end();
		member_type const *f = detail::enum_lower_bound(b, e, needle, less);
		return f == e || less(needle, *f) ? nullptr : f;
	}
};

template<class T>
struct enum_traits_wrapper : protected enum_base_traits<T>
{
	typedef T enum_type;
	enum : bool { is_flag = decltype(detail::enum_flag_lookup(static_cast<typename std::enable_if<std::is_enum<T>::value, T>::type *>(nullptr)))::value };
	static constexpr size_t size() { return enum_members<T>::static_size; }

	static constexpr typename enum_singleton_factory<enum_members_info<T> >::result_type members_info() { return enum_singleton_factory<enum_members_info<T> >::get(); }
	static constexpr typename enum_singleton_factory<enum_members<T> >::result_type members() { return enum_singleton_factory<enum_members<T> >::get(); }
	static constexpr typename enum_singleton_factory<enum_members_sorted_by_name<T> >::result_type members_sorted_by_name() { return enum_singleton_factory<enum_members_sorted_by_name<T> >::get(); }
	static constexpr typename enum_singleton_factory<enum_members_sorted_by_value<T> >::result_type members_sorted_by_value() { return enum_singleton_factory<enum_members_sorted_by_value<T> >::get(); }

#if 201703L <= __cplusplus || (defined(__cpp_inline_variables) && defined(__cpp_lib_string_view))
	static constexpr enum_named_member<T> const *find(T value) { return members_sorted_by_value().find(value); }
	static constexpr enum_named_member<T> const *find(enum_name_view name) { return members_sorted_by_name().find(name.data(), name.size()); }

	static constexpr enum_name_view find_name(T value)
	{
		enum_named_member<T> const *f = find(value);
		return f ? f->name() : enum_name_view();
	}

	template<class U = enum_name_view>
	static constexpr enum_name_view name_or(T value, U &&fallback
#if defined(__clang__) && __clang_major__ >= 7
		[[clang::lifetimebound]]
#endif
		= std::decay_t<U>())
	{
		enum_named_member<T> const *f = find(value);
		if (!f)
		{
			return std::forward<U>(fallback);
		}
		return f->name();
	}

	static constexpr T const *find_value(enum_name_view name)
	{
		enum_named_member<T> const *f = find(name);
		return f ? &f->value : nullptr;
	}
	
	template<class U = T>
	static constexpr std::decay_t<U> value_or(enum_name_view name, U &&fallback
#if defined(__clang__) && __clang_major__ >= 7
		[[clang::lifetimebound]]
#endif
		= std::decay_t<U>())
	{
		enum_named_member<T> const *f = find(name);
		if (!f)
		{
			return std::forward<U>(fallback);
		}
		std::decay_t<U> result(f->value);
		return result;
	}
#endif
};

}

template<class T> using is_flag = std::integral_constant<bool, enum_traits<typename std::enable_if<std::is_enum<T>::value, T>::type>::is_flag>;

namespace flag_ops
{

#define X(Op)  \
	template<class T> constexpr typename std::enable_if<is_flag<std::decay_t<T> >::value, std::decay_t<T> >::type operator Op(T a, T b)  \
	{ return static_cast<std::decay_t<T> >(static_cast<std::underlying_type_t<T> >(a) Op static_cast<std::underlying_type_t<T> >(b)); } /* NOLINT(bugprone-macro-parentheses, hicpp-signed-bitwise) */  \
	template<class T> constexpr typename std::enable_if<is_flag<std::decay_t<T> >::value, T>::type operator Op##=(T &a, T b) { return (a = a Op b); }  \
	template<class T> constexpr typename std::enable_if<is_flag<std::decay_t<T> >::value, std::decay_t<T> >::type operator Op(T a, T b)
X(&);
X(|);
X(^);
#undef  X

#define X(Op, ReturnType)  \
	template<class T> constexpr typename std::enable_if<is_flag<std::decay_t<T> >::value, std::decay_t<ReturnType> >::type operator Op(T value)  \
	{ return static_cast<ReturnType>(Op static_cast<std::underlying_type_t<T> >(value)); } /* NOLINT(bugprone-macro-parentheses) */  \
	template<class T> constexpr typename std::enable_if<is_flag<std::decay_t<T> >::value, std::decay_t<ReturnType> >::type operator Op(T value)
X(~, T);
X(!, bool);
#undef  X

template<class T> constexpr typename std::enable_if<is_flag<std::decay_t<T> >::value, bool>::type is_set(T value, T mask)
{
	using flag_ops::operator|;
	return value == (value | mask);
}

#if 201703L <= __cplusplus || defined(__cpp_nontype_template_parameter_auto)
template<auto Mask> constexpr typename std::enable_if<is_flag<std::decay_t<decltype(Mask)> >::value, bool>::type is_set(std::decay_t<decltype(Mask)> value)
{
	return flag_ops::is_set(value, Mask);
}
#endif

}

using namespace flag_ops;

template<class T>
struct enum_decomposition
{
	typedef enum_traits<T> traits_type;
	typedef detail::enum_named_member<T> member_type;
	struct element_type : member_type
	{
		using member_type::member_type;
		constexpr operator T() const /* NOLINT(hicpp-explicit-conversions) */ { return this->member_type::value; }
	};
	size_t capacity = traits_type::size(), index = 0, size = 0;
	T value = T(), accumulated = T();

#if 201703L <= __cplusplus || defined(__cpp_inline_variables)
#else
	detail::enum_members<T> cached_members = detail::enum_singleton_factory<detail::enum_members<T> >::get();
#endif

	constexpr enum_decomposition() = default;

	constexpr explicit enum_decomposition(T v)
	{
		this->reset(v);
	}

	constexpr void reset(T v = T())
	{
		this->index = 0;
		this->size = this->capacity;
		this->value = v;
		this->accumulated = T();
	}

	constexpr element_type pop_front(size_t nth = 1, bool only_match_zero_if_value_is_zero = true)
	{
		return this->pop(false, nth, only_match_zero_if_value_is_zero);
	}

	constexpr element_type pop_back(size_t nth = 1, bool only_match_zero_if_value_is_zero = true)
	{
		return this->pop(true, nth, only_match_zero_if_value_is_zero);
	}

	// Returns the popped element, or an element with an empty name if nothing was popped.
	constexpr element_type pop(bool reverse, size_t nth, bool only_match_zero_if_value_is_zero)
	{
#if 201703L <= __cplusplus || defined(__cpp_inline_variables)
		decltype(auto) members = traits_type::members();
		static_assert(std::is_reference<decltype(members)>::value, "unexpected performance impact");
#else
		auto const &members = this->cached_members;
#endif
		typedef std::underlying_type_t<T> U;
		bool is_flag = traits_type::is_flag;
		element_type result = element_type();
		result.name_offset = detail::enum_members_info<T>::max_offset;
		while (nth > 0 && this->size > 0)
		{
			size_t i = ~size_t();
			if (reverse)
			{
				i = this->index + static_cast<ptrdiff_t>(this->size);
				--i;
			}
			else
			{
				i = this->index;
			}
			T member_value = members[i].value;
			T expected = member_value;
			if (is_flag)
			{
				expected = static_cast<T>(static_cast<U>(expected) | static_cast<U>(this->value)) /* NOLINT(hicpp-signed-bitwise) */;
			}
			--this->size;
			if (!reverse)
			{
				++i;
				this->index = i;
			}
			if ((!only_match_zero_if_value_is_zero || (member_value == T()) == (this->value == T())) && expected == this->value)
			{
				if (!reverse)
				{
					--i;
				}
				--nth;
				if (nth == 0)
				{
					static_cast<member_type &>(result) = members[i];
				}
				this->accumulated = static_cast<T>(static_cast<U>(this->accumulated) | static_cast<U>(member_value)) /* NOLINT(hicpp-signed-bitwise) */;
			}
		}
		if (nth > 0 && this->value != this->accumulated)
		{
			--nth;
			if (nth == 0)
			{
				result.name_offset = static_cast<typename detail::enum_members_info<T>::offset_type>(detail::enum_members_info<T>::max_offset - 1);
				result.value = static_cast<T>(static_cast<U>(static_cast<U>(this->value) & static_cast<U>(~static_cast<U>(this->accumulated))));
			}
			this->accumulated = this->value;
		}
		return result;
	}

	template<class F>
	constexpr F &&for_each(F &&callback, bool backward = false)
	{
		for (;;)
		{
			element_type element = backward ? this->pop_back() : this->pop_front();
			if (!element.valid()) { break; }
			callback(std::move(element));
		}
		return std::forward<F>(callback);
	}
};

template<class T, class F>
constexpr F &&for_each_enum_component(T const &value, F &&callback, bool backward = false)
{
	return enum_decomposition<T>(value).for_each(std::forward<F>(callback), backward);
}

template<class T>
constexpr T parse_enum(
	typename std::conditional<false, T, detail::enum_name_view>::type text,
	/* out */ size_t *end_index = nullptr,
	detail::enum_name_view const &delim = "|",
	detail::enum_name_view const &spaces = " \t\r\n",
	detail::enum_members_sorted_by_name<T> const &members_sorted_by_name = detail::enum_singleton_factory<detail::enum_members_sorted_by_name<T> >::get()
)
{
	typedef std::underlying_type_t<T> U;
	T result = T();
	size_t const n = text.size();
	bool started = false;
	size_t i = 0;
	while (i < n)
	{
		i = text.find_first_not_of(spaces, i);
		if (i > n) { i = n; }
		if (started)
		{
			detail::enum_name_view suffix(text.data() + static_cast<ptrdiff_t>(i), text.size() - i);
			if (suffix.size() >= delim.size() && detail::enum_name_view(suffix.data(), delim.size()) == delim)
			{
				i += delim.size();
			}
			else
			{
				// Unrecognized delimiter
				break;
			}
			i = text.find_first_not_of(spaces, i);
			if (i > n) { i = n; }
		}
		bool first_char_was_digit = false, negative = false;
		size_t j = i;
		if (i < j && text[i] == '-')
		{
			negative = true;
			++i;
		}
		while (j < n)
		{
			char const ch = text[j];
			bool const is_digit = static_cast<unsigned>(ch - '0') <= '9' - '0';
			if (is_digit)
			{
				if (i == j)
				{
					first_char_was_digit = true;
				}
				// Keep going; digits are valid identifier characters
			}
			else if (static_cast<unsigned>(ch - 'a') <= 'z' - 'a' || static_cast<unsigned>(ch - 'A') <= 'Z' - 'A' || ch == '_')
			{
				// Keep going; normal identifier characters
			}
			else
			{
				break;
			}
			++j;
		}
		if (first_char_was_digit)
		{
			// TODO: This is a raw numeric value; parse it as such
			unsigned char radix = 10;
			if (text[i] == '0' && i + 1 < j)
			{
				char const radix_spec = text[i + 1];
				if (static_cast<unsigned>(radix_spec - '0') > '9' - '0')
				{
					// Second letter isn't a digit; we have a prefix like 0x or something like that.
					if (radix_spec == 'x' || radix_spec == 'X')
					{
						radix = 16;
					}
					else if (radix_spec == 'o' || radix_spec == 'O')
					{
						radix = 8;
					}
					else if (radix_spec == 'b' || radix_spec == 'B')
					{
						radix = 2;
					}
					else
					{
						// Error - unrecognized radix specifier
						break;
					}
					i += 2;
				}
				else
				{
					radix = 8;
				}
			}
			if (i == j)
			{
				// We had a prefix like "0x" with no actual number; stop here
				break;
			}
			// Everything should be numeric past this point, but we'll need to verify
			std::make_unsigned_t<U> v = std::make_unsigned_t<U>();
			bool failed = false;
			while (i < j)
			{
				char const ch = text[i];
				unsigned char const digit = static_cast<unsigned char>(ch <= '9' ? ch - '0' : 10 + (ch - ((ch - 'a') <= 'z' - 'a' ? 'a' : 'A')));
				if (digit >= radix)
				{
					// Invalid digit
					failed = true;
					break;
				}
				v = v * radix + digit;
				++i;
			}
			if (failed)
			{
				break;
			}
			result = static_cast<T>(static_cast<U>(result) | (negative ? U() - v : v));
		}
		else if (negative)
		{
			// Can't have negative without a digit
			break;
		}
		else if (i < j)
		{
			// We have a valid identifier; check to see that it's a valid member
			if (detail::enum_named_member<T> const *found = members_sorted_by_name.find(text.data() + i, j - i))
			{
				result = static_cast<T>(static_cast<U>(result) | static_cast<U>(found->value));
			}
			else
			{
				// Invalid name encountered
				break;
			}
		}
		else
		{
			// Invalid character encountered
			break;
		}
		i = j;
		started = true;
	}
	if (end_index)
	{
		*end_index = i;
	}
	return result;
}

template<class T>
typename std::enable_if<std::is_enum<T>::value, std::basic_string<char> >::type to_string(
	T value,
	detail::enum_name_view const &delim = " | ",
	typename std::enable_if<std::is_enum<T>::value, std::basic_string<char> >::type &&string_to_append_to = typename std::enable_if<std::is_enum<T>::value, std::basic_string<char> >::type(),
	detail::enum_members_info<T> const &members_info = detail::enum_singleton_factory<detail::enum_members_info<T> >::get()
)
{
	typedef typename std::enable_if<std::is_enum<std::decay_t<T> >::value, std::basic_string<char> >::type string_type;
	typedef std::underlying_type_t<T> U;
	bool backward = false;
	detail::enum_name_view delimiter(delim);
	string_type result(std::move(string_to_append_to));
	size_t original_result_size = result.size();
	size_t result_length = 0;
	for (int pass = 0; pass < 2; ++pass)
	{
		size_t m = 0;
		for (enum_decomposition<T> decomp(value); ; ++m)
		{
			detail::enum_named_member<T> element = backward ? decomp.pop_back() : decomp.pop_front();
			if (!element.valid()) { break; }
			if (m > 0)
			{
				if (!pass)
				{
					result_length += delimiter.size();
				}
				else
				{
					result.insert(element.name_length > 0 ? result.end() : result.begin() + static_cast<ptrdiff_t>(original_result_size), delimiter.begin(), delimiter.end());
				}
			}
			enum : size_t { capacity = 1 /* minus sign */ + 2 /* 0x prefix */ + sizeof(T) * 8 / 4 + /* hex digits */ +1 /* null terminator just in case */ };
			if (!pass)
			{
				if (element.name_length > 0)
				{
					result_length += element.name_length;
				}
				else
				{
					result_length += capacity;
				}
			}
			else
			{
				if (element.name_length > 0)
				{
					result += members_info.name(element);
				}
				else
				{
					char buf[capacity];
					size_t k = 0;
					bool negative = false;
					if (element.value != T())
					{
						U elem = static_cast<U>(element.value);
						negative |= std::is_signed<T>::value && elem < 0;
						if (negative)
						{
							elem = static_cast<U>(~static_cast<typename std::make_unsigned<U>::type>(elem) + 1U);
						}
						for (size_t i = 0; i != sizeof(elem) * 8 / 4; ++i)
						{
							unsigned const digit = static_cast<unsigned>(elem % (1U << 4U));
							buf[capacity - ++k] = digit <= 9 ? static_cast<char>('0' + digit) : static_cast<char>('A' + (digit - 10U));
							elem >>= 4U;
							if (elem == U()) { break; }
						}
						buf[capacity - ++k] = 'x';
					}
					buf[capacity - ++k] = '0';
					if (negative)
					{
						buf[capacity - ++k] = '-';
					}
					result.insert(original_result_size, &buf[capacity - k], k);
				}
			}
		}
		if (!pass)
		{
			result.reserve(original_result_size + result_length);
		}
	}
	return result;
}

VIT_ENUM_END_NAMESPACE

#ifndef VIT_ENUM_DETAIL_STRINGIZE
#define VIT_ENUM_DETAIL_STRINGIZE_(...) #__VA_ARGS__
#define VIT_ENUM_DETAIL_STRINGIZE(...)  VIT_ENUM_DETAIL_STRINGIZE_(__VA_ARGS__)
#endif

#ifndef VIT_ENUM_DETAIL_UNPARENTHESIZE
#define VIT_ENUM_DETAIL_UNPARENTHESIZE_(...) __VA_ARGS__
#define VIT_ENUM_DETAIL_UNPARENTHESIZE(...)  VIT_ENUM_DETAIL_UNPARENTHESIZE_ __VA_ARGS__
#endif

#ifndef VIT_ENUM_DETAIL_X_STRINGIZE
#define VIT_ENUM_DETAIL_X_STRINGIZE(Member) #Member ","
#endif

#ifndef VIT_ENUM_DETAIL_X_FIELD
#define VIT_ENUM_DETAIL_X_FIELD(Member) _enum_member_initializer_t Member = static_cast<_enum_underlying_type>(_enum_type::Member);
#endif

#ifndef VIT_ENUM_DETAIL_X_ASSIGN
#define VIT_ENUM_DETAIL_X_ASSIGN(Member) , Member = static_cast<_enum_underlying_type>(_enum_type::Member)
#endif

#ifndef VIT_ENUM_DETAIL_X_CASE
#define VIT_ENUM_DETAIL_X_CASE(Member) case _enum_type::Member: break;
#endif

#define VIT_ENUM_POST_DEFINITION_IMPL(Name, IsFlag, X)  \
struct Name##_enum_detail  \
{  \
	static constexpr decltype(auto) _enum_create_body() { return (X(VIT_ENUM_DETAIL_X_STRINGIZE)); }  \
	typedef typename std::enable_if<(sizeof(VIT_ENUM_NAMESPACE::detail::enum_friend<Name, Name##_enum_detail>) + sizeof(VIT_ENUM_NAMESPACE::detail::enum_flag<Name, IsFlag>) > 0), Name>::type _enum_type;  \
	typedef std::underlying_type_t<_enum_type> _enum_underlying_type;  \
	typedef VIT_ENUM_NAMESPACE::detail::enum_member_initializer<_enum_type, _enum_underlying_type> _enum_member_initializer_t;  \
	typedef VIT_ENUM_NAMESPACE::detail::enum_initializer<_enum_type, _enum_underlying_type> _enum_initializer_t;  \
	Name##_enum_detail() = default;  \
	constexpr explicit Name##_enum_detail(_enum_member_initializer_t _enum_initializers[])  \
	{  \
		_enum_initializer_t _enum_initializer = { _enum_initializers, static_cast<_enum_type>(_enum_underlying_type() - static_cast<_enum_underlying_type>(1)) };  \
		(void(), _enum_initializer) X(VIT_ENUM_DETAIL_X_ASSIGN);  \
		switch ((void(), _enum_type())) { X(VIT_ENUM_DETAIL_X_CASE) }  \
	}  \
private:  \
	X(VIT_ENUM_DETAIL_X_FIELD)  \
}

#define VIT_EXTERNAL_ENUM(Name, X)  VIT_ENUM_POST_DEFINITION_IMPL(Name, false, X)
#define VIT_EXTERNAL_FLAG(Name, X)  VIT_ENUM_POST_DEFINITION_IMPL(Name, true, X)

#define VIT_ENUM_DEFINITION(PreDecl, Name, PostDecl, IsFlag, Members)  \
PreDecl Name PostDecl { Members };  \
struct Name##_enum_detail  \
{  \
	static constexpr decltype(auto) _enum_create_body() { return (VIT_ENUM_DETAIL_STRINGIZE(Members)); }  \
	typedef typename std::enable_if<(sizeof(VIT_ENUM_NAMESPACE::detail::enum_friend<Name, Name##_enum_detail>) + sizeof(VIT_ENUM_NAMESPACE::detail::enum_flag<Name, IsFlag>) > 0), Name>::type _enum_type;  \
	typedef std::underlying_type_t<_enum_type> _enum_underlying_type;  \
	typedef VIT_ENUM_NAMESPACE::detail::enum_member_initializer<_enum_type, _enum_underlying_type> _enum_member_initializer_t;  \
	typedef VIT_ENUM_NAMESPACE::detail::enum_initializer<_enum_type, _enum_underlying_type> _enum_initializer_t;  \
	Name##_enum_detail() = default;  \
	constexpr explicit Name##_enum_detail(_enum_member_initializer_t _enum_initializers[])  \
	{  \
		_enum_initializer_t _enum_initializer = { _enum_initializers, static_cast<_enum_type>(_enum_underlying_type() - static_cast<_enum_underlying_type>(1)) };  \
		(void(), _enum_initializer), Members;  \
	}  \
private:  \
	_enum_member_initializer_t Members;  \
}

#define VIT_DEFINE_ENUM_T(PreDecl, Name, PostDecl, ...)  VIT_ENUM_DEFINITION(PreDecl, Name, PostDecl, false, VIT_ENUM_DETAIL_UNPARENTHESIZE((__VA_ARGS__)))
#define VIT_DEFINE_FLAG_T(PreDecl, Name, PostDecl, ...)  VIT_ENUM_DEFINITION(PreDecl, Name, PostDecl, true, VIT_ENUM_DETAIL_UNPARENTHESIZE((__VA_ARGS__)))
#define VIT_DEFINE_UNSCOPED_ENUM(Name, ...)  VIT_DEFINE_ENUM_T(enum, Name, : int, VIT_ENUM_DETAIL_UNPARENTHESIZE((__VA_ARGS__)))
#define VIT_DEFINE_UNSCOPED_FLAG(Name, ...)  VIT_DEFINE_FLAG_T(enum, Name, : int, VIT_ENUM_DETAIL_UNPARENTHESIZE((__VA_ARGS__)))
#define VIT_DEFINE_ENUM(Name, ...)  VIT_DEFINE_ENUM_T(enum class, Name, : int, VIT_ENUM_DETAIL_UNPARENTHESIZE((__VA_ARGS__)))
#define VIT_DEFINE_FLAG(Name, ...)  VIT_DEFINE_FLAG_T(enum class, Name, : int, VIT_ENUM_DETAIL_UNPARENTHESIZE((__VA_ARGS__)))


#if (!defined(VIT_ENUM_ENABLE_TESTS) ? !defined(NDEBUG) : VIT_ENUM_ENABLE_TESTS)

VIT_ENUM_BEGIN_NAMESPACE

namespace test_detail
{

VIT_DEFINE_FLAG(test_enum_0, test_enum_0_A, test_enum_0_B, test_enum_0_C, test_enum_0_D);
VIT_DEFINE_ENUM_T(enum, test_enum_1, : long long, test_enum_1_A = static_cast<long long>(~0ULL << 63U), test_enum_1_B, test_enum_1_C = -1, test_enum_1_D = ((void)0, test_enum_1_C + 1), test_enum_1_E = static_cast<long long>(~0ULL >> 1U));
enum /* space */ class test_enum_2 : long long { test_enum_2_A = static_cast<long long>(~0ULL << 63U), test_enum_2_B, test_enum_2_C = -1, test_enum_2_D = ((void)0, test_enum_2_C + 1), test_enum_2_E = static_cast<long long>(~0ULL >> 1U) };
template<decltype(nullptr) = nullptr> struct test_enum_outer_3 { VIT_DEFINE_UNSCOPED_ENUM(test_enum_3, test_enum_3_A, test_enum_3_B); };

namespace
{
	struct enum_test
	{
		constexpr enum_test();
		constexpr void verify(bool cond, char const msg[]);
	} const enum_test_;
}

constexpr void enum_test::verify(bool cond, char const msg[])
{
	if (!cond)
	{
		throw msg;
	}
}

constexpr enum_test::enum_test()
{
	struct string_helper
	{
		static constexpr bool equal(char const *a, char const *b)
		{
			while (*a != '\0' && *b != '\0' && *a == *b)
			{
				++a;
				++b;
			}
			return *a == *b;
		}
		static constexpr size_t end_of_parse_test_enum_1(detail::enum_name_view text)
		{
			size_t i = 0;
			parse_enum<test_enum_1>(text, &i);
			return i;
		}
	};

	verify(is_flag<test_enum_0>::value, "invalid flag bit");
	verify(detail::enum_members<test_enum_0>()[0].value == test_enum_0::test_enum_0_A, "invalid enum members");
	verify(enum_traits<test_enum_0>::members()[0].value == test_enum_0::test_enum_0_A, "invalid enum members");
	verify(is_set(test_enum_0::test_enum_0_B, test_enum_0::test_enum_0_A) && !is_set(test_enum_0::test_enum_0_B, test_enum_0::test_enum_0_C), "invalid enum members");
	verify((test_enum_0::test_enum_0_A | test_enum_0::test_enum_0_B | test_enum_0::test_enum_0_C) == test_enum_0::test_enum_0_D, "invalid enum members");

	verify(string_helper::end_of_parse_test_enum_1("0xx") == 2 && string_helper::end_of_parse_test_enum_1("0b0") == 3, "parsing error");
	verify(string_helper::end_of_parse_test_enum_1("0") == 1 && string_helper::end_of_parse_test_enum_1("0x") == 2 && string_helper::end_of_parse_test_enum_1("0x0") == 3, "parsing error");
	verify(string_helper::end_of_parse_test_enum_1(" 0x1 | 2 | test_enum_1_A ") == 25, "parsing error");
	verify(parse_enum<test_enum_1>("0") == test_enum_1() && parse_enum<test_enum_1>("") == test_enum_1() && parse_enum<test_enum_1>(" test_enum_1_A| test_enum_1_B |0x1000|0") == (test_enum_1_A | test_enum_1_B | static_cast<test_enum_1>(0x1000)), "parsing error");
	verify(!is_flag<test_enum_1>::value, "invalid flag bit");
	constexpr decltype(auto) members_info = enum_traits<test_enum_1>::members_info();
	constexpr decltype(auto) members = enum_traits<test_enum_1>::members();
	constexpr decltype(auto) members_sorted_by_name = enum_traits<test_enum_1>::members_sorted_by_name();
	constexpr decltype(auto) members_sorted_by_value = enum_traits<test_enum_1>::members_sorted_by_value();
	verify(members_sorted_by_name.find("test_enum_1_E") != nullptr && members_sorted_by_name.find("test_enum_") == nullptr, "invalid enum name lookup; this seems to occur in VS2017's string_view");
	verify(members_sorted_by_value.find(test_enum_1::test_enum_1_E) != nullptr && members_sorted_by_value.find(static_cast<test_enum_1>(0xBAADF00D /* NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers) */)) == nullptr, "invalid enum value lookup");
	verify(
		sizeof(members) / sizeof(members[0]) == members.size() &&
		members.size() == 5 /* NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers) */ &&
		members[0].value == test_enum_1_A && string_helper::equal(&members_info.names()[members[0].name_offset], "test_enum_1_A") &&
		members[1].value == test_enum_1_B && string_helper::equal(&members_info.names()[members[1].name_offset], "test_enum_1_B") &&
		members[2].value == test_enum_1_C && string_helper::equal(&members_info.names()[members[2].name_offset], "test_enum_1_C") &&
		members[3].value == test_enum_1_D && string_helper::equal(&members_info.names()[members[3].name_offset], "test_enum_1_D") &&
		members[4].value == test_enum_1_E && string_helper::equal(&members_info.names()[members[4].name_offset], "test_enum_1_E"),
		"invalid enum members");
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(error: 4061 4062)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wswitch"
#endif
	test_enum_1 v = test_enum_1();
	switch (v)
	{
	case static_cast<test_enum_1>(test_enum_2::test_enum_2_A):
	case static_cast<test_enum_1>(test_enum_2::test_enum_2_B):
	case static_cast<test_enum_1>(test_enum_2::test_enum_2_C):
	case static_cast<test_enum_1>(test_enum_2::test_enum_2_D):
	case static_cast<test_enum_1>(test_enum_2::test_enum_2_E):
		break;
	}
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
	verify(test_enum_outer_3<>::test_enum_3_B == 1, "nested enum error");

	typedef test_enum_0 E;
	struct Decomposer
	{
		enum : size_t { num_expected = 4 };
		E expected[num_expected];
		size_t i, j;

		constexpr void operator()(E value)
		{
			i = (i > num_expected || value != expected[i]) ? ~size_t() : i + 1;
		}

		static constexpr auto test()
		{
			E const invalid = static_cast<E>(0xBAADF00D);
			Decomposer callback = { { E::test_enum_0_D, E::test_enum_0_C, E::test_enum_0_B, invalid & ~(E::test_enum_0_D | E::test_enum_0_C | E::test_enum_0_B)}, {}, 0};
			vit::core::for_each_enum_component(callback.expected[0] | invalid, callback, true);
			return callback;
		}
	};
	verify(Decomposer::test().i == Decomposer::num_expected, "flag decomposition error");
}

static_assert(((void)enum_test(), true), "enum test failed");

}

VIT_ENUM_END_NAMESPACE

#endif

#endif
