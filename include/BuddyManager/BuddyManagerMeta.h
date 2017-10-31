/**
 * File: /BuddyManagerMeta.h
 * Project: BuddyManager
 * Created Date: Saturday, October 28th 2017, 2:27:30 pm
 * Author: Harikrishnan
 */


#ifndef BUDDYMANAGERMETA_H
#define BUDDYMANAGERMETA_H

#include "common.h"

#include <cstddef>
#include <algorithm>
#include <cassert>

#ifdef __LINUX__
#define leading_zeroes(x) __builtin_clzl(x)
#endif /* __LINUX__ */

#ifdef __APPLE__
#define leading_zeroes(x) __builtin_clzl(x)
#endif /* __APPLE__ */

#ifdef _WIN32
#include <intrin.h>
#define leading_zeroes(x) __lzcnt64(x)
#endif /* _WIN32 */


namespace SmallAlloc
{

namespace BuddyManager
{

using Index = size_t;
using Offset = ptrdiff_t;

template <size_t PageSize, size_t MinAllocSize>
class BuddyManagerMeta
{
public:
	BuddyManagerMeta() : m_num_class_sizes(get_num_sizeclasses())
	{
		memset(m_bitmap, 0, get_bitmap_size());
	}

	static constexpr Count get_num_sizeclasses_const();
	static Count get_num_sizeclasses();
	static SizeClass get_sizeclass(Size size);
	static Offset get_buddy(Offset ptr_offset, SizeClass szc);

	Index get_bitmap_index(Offset ptr_offset, SizeClass szc) const;
	void mark_block_as_free(Offset ptr_offset, SizeClass szc);
	void mark_block_as_in_use(Offset ptr_offset, SizeClass szc);
	bool block_is_free(Offset ptr_offset, SizeClass szc) const;

private:

	static constexpr Size get_bitmap_size();
	static Size get_size(SizeClass szc);

	uint32_t m_num_class_sizes;
	uint8_t m_bitmap[get_bitmap_size()];

	static constexpr size_t log_2(size_t n)
	{
		assert(n > 0);

		auto lz = leading_zeroes(n);
		return sizeof(n) * 8 - lz - 1;
	}
};

template <size_t PageSize, size_t MinAllocSize>
constexpr Count BuddyManagerMeta<PageSize, MinAllocSize>::get_num_sizeclasses_const()
{
	constexpr auto const_log_2 = [](auto n)
	{
		size_t lz = 0;

		while (n)
		{
			if (n >> (sizeof(n) * 8 - 1))
				break;

			lz++;
			n <<= 1;
		}

		return sizeof(n) * 8 - lz - 1;
	};

	return const_log_2(PageSize) - const_log_2(MinAllocSize) + 1;
}

template <size_t PageSize, size_t MinAllocSize>
Count BuddyManagerMeta<PageSize, MinAllocSize>::get_num_sizeclasses()
{
	return log_2(PageSize) - log_2(MinAllocSize) + 1;
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Size BuddyManagerMeta<PageSize, MinAllocSize>::get_bitmap_size()
{
	return ((PageSize / MinAllocSize) * 2) / 8;
}

template <size_t PageSize, size_t MinAllocSize>
SizeClass BuddyManagerMeta<PageSize, MinAllocSize>::get_sizeclass(Size size)
{
	assert(size >= MinAllocSize && size <= PageSize);

	return log_2(size) - log_2(MinAllocSize);
}

template <size_t PageSize, size_t MinAllocSize>
Size BuddyManagerMeta<PageSize, MinAllocSize>::get_size(SizeClass szc)
{
	return MinAllocSize * (1ULL << szc);
}

template <size_t PageSize, size_t MinAllocSize>
Index BuddyManagerMeta<PageSize, MinAllocSize>::get_bitmap_index(Offset ptr_offset,
																 SizeClass szc) const
{
	return (1 << (m_num_class_sizes - (szc + 1))) - 1 + ptr_offset / get_size(szc);
}

template <size_t PageSize, size_t MinAllocSize>
Offset BuddyManagerMeta<PageSize, MinAllocSize>::get_buddy(Offset ptr_offset,
														   SizeClass szc)
{
	auto size = get_size(szc);
	auto n_ptr = ptr_offset >> log_2(size);

	return (n_ptr ^ 0x1) << log_2(size);
}

template <size_t PageSize, size_t MinAllocSize>
void BuddyManagerMeta<PageSize, MinAllocSize>::mark_block_as_free(Offset ptr_offset, SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr_offset, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	assert(!block_is_free(ptr_offset, szc));

	m_bitmap[bitmap_byte] &= ~(1 << bitmap_bit);
}

template <size_t PageSize, size_t MinAllocSize>
void BuddyManagerMeta<PageSize, MinAllocSize>::mark_block_as_in_use(Offset ptr_offset,
																	SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr_offset, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	assert(block_is_free(ptr_offset, szc));

	m_bitmap[bitmap_byte] |= (1 << bitmap_bit);
}

template <size_t PageSize, size_t MinAllocSize>
bool BuddyManagerMeta<PageSize, MinAllocSize>::block_is_free(Offset ptr_offset, SizeClass szc) const
{
	auto bitmap_index = get_bitmap_index(ptr_offset, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	return (m_bitmap[bitmap_byte] & (1 << bitmap_bit)) == false;
}

}
}

#endif /* BUDDYMANAGERMETA_H */
