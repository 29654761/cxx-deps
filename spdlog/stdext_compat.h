// stdext_compat.h
//
// 兼容性垫片：新版 MSVC STL(14.51+/VS2026) 已彻底移除
// stdext::checked_array_iterator，而本目录随 spdlog 自带的旧版 fmt
// (fmt/bundled/format.h) 在调试构建(_SECURE_SCL==1)时仍会引用该类型，导致
// C2653 “stdext 不是命名空间”等大量编译错误。
//
// STL 强制要求 _ITERATOR_DEBUG_LEVEL>0 时 _SECURE_SCL==1，因此无法通过定义
// _SECURE_SCL=0 绕过 fmt 中的该分支，只能补回被移除的类型本身。
// 本头文件由 fmt/bundled/format.h 顶部包含，为其补回该类型。
#pragma once

#if defined(_MSC_VER)

#include <cstddef>
#include <iterator>

// 仅当当前工具链确实不再提供该类型时才补充定义，避免与旧工具链冲突。
#if !defined(_STDEXT_CHECKED_ARRAY_ITERATOR_COMPAT_) && !defined(__cpp_lib_stdext_checked_array_iterator)
#define _STDEXT_CHECKED_ARRAY_ITERATOR_COMPAT_

namespace stdext
{
	// 还原旧版 MSVC 的 stdext::checked_array_iterator（随机访问迭代器）。
	// 模板参数为底层指针类型（如 T*），与 fmt 的用法 checked_array_iterator<T*> 一致。
	template <class _Iterator>
	class checked_array_iterator
	{
	public:
		typedef std::random_access_iterator_tag									iterator_category;
		typedef typename std::iterator_traits<_Iterator>::value_type			value_type;
		typedef typename std::iterator_traits<_Iterator>::difference_type		difference_type;
		typedef typename std::iterator_traits<_Iterator>::pointer				pointer;
		typedef typename std::iterator_traits<_Iterator>::reference				reference;

		checked_array_iterator() : array_(), size_(0), index_(0) {}

		checked_array_iterator(_Iterator array, size_t size, size_t index = 0)
			: array_(array), size_(size), index_(index) {}

		_Iterator base() const { return array_ + index_; }

		reference operator*() const { return array_[index_]; }
		pointer operator->() const { return &array_[index_]; }
		reference operator[](difference_type off) const { return array_[index_ + off]; }

		checked_array_iterator& operator++() { ++index_; return *this; }
		checked_array_iterator operator++(int) { checked_array_iterator t = *this; ++index_; return t; }
		checked_array_iterator& operator--() { --index_; return *this; }
		checked_array_iterator operator--(int) { checked_array_iterator t = *this; --index_; return t; }

		checked_array_iterator& operator+=(difference_type off) { index_ += off; return *this; }
		checked_array_iterator& operator-=(difference_type off) { index_ -= off; return *this; }

		checked_array_iterator operator+(difference_type off) const { return checked_array_iterator(array_, size_, index_ + off); }
		checked_array_iterator operator-(difference_type off) const { return checked_array_iterator(array_, size_, index_ - off); }

		difference_type operator-(const checked_array_iterator& rhs) const
		{
			return static_cast<difference_type>(index_) - static_cast<difference_type>(rhs.index_);
		}

		bool operator==(const checked_array_iterator& rhs) const { return index_ == rhs.index_; }
		bool operator!=(const checked_array_iterator& rhs) const { return index_ != rhs.index_; }
		bool operator<(const checked_array_iterator& rhs) const { return index_ < rhs.index_; }
		bool operator>(const checked_array_iterator& rhs) const { return index_ > rhs.index_; }
		bool operator<=(const checked_array_iterator& rhs) const { return index_ <= rhs.index_; }
		bool operator>=(const checked_array_iterator& rhs) const { return index_ >= rhs.index_; }

	private:
		_Iterator array_;
		size_t size_;
		size_t index_;
	};

	template <class _Iterator>
	inline checked_array_iterator<_Iterator> operator+(
		typename checked_array_iterator<_Iterator>::difference_type off,
		checked_array_iterator<_Iterator> it)
	{
		return it + off;
	}
}

#endif // stdext shim guard

#endif // _MSC_VER
