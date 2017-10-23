#ifndef BUDDY_MANAGER_H
#define BUDDY_MANAGER_H

#include "Utility/IList.h"

#include <cstddef>
#include <algorithm>
#include <vector>
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

namespace
{
using Count = size_t;
using Size = size_t;
using SizeClass = size_t;
using Index = size_t;
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
class BuddyManager
{
public:
	BuddyManager(void *chunk);
	BuddyManager(void *chunk, void *freelist);

	void *alloc(size_t size);
	bool free(void *ptr, size_t size);
	void get_allocable_sizes(std::vector<std::pair<size_t, size_t>> &allocable_sizes);

private:

	static constexpr Count get_num_sizeclasses();
	static constexpr Size get_bitmap_size();
	static constexpr Size get_size(SizeClass szc);
	static constexpr SizeClass get_sizeclass(Size size);

	utility::ForwardList &get_freelist(SizeClass szc);
	Index get_bitmap_index(void *ptr, SizeClass szc);
	void mark_block_as_free(void *ptr, SizeClass szc);
	void *get_buddy(void *ptr, SizeClass szc);
	void mark_block_as_in_use(void *ptr, SizeClass szc);
	bool block_is_free(void *ptr, SizeClass szc);

	void *m_managed_chunk;
	char m_bitmap[get_bitmap_size()];
	utility::ForwardList *m_freelist;
	utility::ForwardList freelist_array[OwnFreelist ? get_num_sizeclasses() : 0];
};

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
BuddyManager<PageSize, MinAllocSize, OwnFreelist>::BuddyManager(
	void *chunk) : m_managed_chunk(chunk)
{
	static_assert(OwnFreelist, "Freelist not passed with OwnFreelist == false");

	m_freelist = this->freelist_array;

	auto &freelist = get_freelist(get_sizeclass(PageSize));
	freelist.push(static_cast<utility::ForwardList::Node *>(chunk));

	memset(m_bitmap, 0, get_bitmap_size());
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
BuddyManager<PageSize, MinAllocSize, OwnFreelist>::BuddyManager(void *chunk, void *fl)
	: m_managed_chunk(chunk), m_freelist(static_cast<utility::ForwardList *>(fl))
{
	static_assert(!OwnFreelist, "Freelist passed with OwnFreelist == true");

	auto &freelist = get_freelist(get_sizeclass(PageSize));
	freelist.push(static_cast<utility::ForwardList::Node *>(chunk));
	memset(m_bitmap, 0, get_bitmap_size());
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
void *BuddyManager<PageSize, MinAllocSize, OwnFreelist>::alloc(size_t size)
{
	if (size > PageSize || size < MinAllocSize)
		return nullptr;

	auto szc = get_sizeclass(size);
	auto &freelist = get_freelist(szc);
	void *ret_mem = freelist.pop();
	void *buddy;

	if (ret_mem)
		goto found;

	ret_mem = alloc(size * 2);

	if (ret_mem == nullptr)
		return nullptr;

	buddy = get_buddy(ret_mem, szc);
	freelist.push(static_cast<utility::ForwardList::Node *>(buddy));

found:
	mark_block_as_in_use(ret_mem, szc);
	return ret_mem;
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
bool BuddyManager<PageSize, MinAllocSize, OwnFreelist>::free(void *ptr, size_t size)
{
	auto szc = get_sizeclass(size);
	auto &freelist = get_freelist(szc);

	mark_block_as_free(ptr, szc);

	if (size == PageSize)
	{
		freelist.push(static_cast<utility::ForwardList::Node *>(ptr));
		return true;
	}

	auto buddy = get_buddy(ptr, szc);

	if (block_is_free(buddy, szc))
	{
		freelist.remove(static_cast<utility::ForwardList::Node *>(buddy));
		return free(std::min(ptr, buddy), size * 2);
	}

	freelist.push(static_cast<utility::ForwardList::Node *>(ptr));
	return false;
}

static size_t log_2(size_t n)
{
	assert(n > 0);
	auto lz = leading_zeroes(n);
	return sizeof(n) * 8 - lz - 1;
}

constexpr size_t const_log_2(size_t n)
{
	assert(n > 0);

	size_t lz = 0;

	while (n)
	{
		if (n >> (sizeof(n) * 8 - 1))
			break;

		lz++;
		n <<= 1;
	}

	return sizeof(n) * 8 - lz - 1;
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
constexpr Count BuddyManager<PageSize, MinAllocSize, OwnFreelist>::get_num_sizeclasses()
{
	return const_log_2(PageSize) - const_log_2(MinAllocSize) + 1;
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
constexpr Size BuddyManager<PageSize, MinAllocSize, OwnFreelist>::get_bitmap_size()
{
	return ((PageSize / MinAllocSize) * 2) / 8;
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
constexpr SizeClass BuddyManager<PageSize, MinAllocSize, OwnFreelist>::get_sizeclass(Size size)
{
	assert(size >= MinAllocSize && size <= PageSize);

	return log_2(size) - log_2(MinAllocSize);
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
constexpr Size BuddyManager<PageSize, MinAllocSize, OwnFreelist>::get_size(SizeClass szc)
{
	assert(szc < get_num_sizeclasses());

	return MinAllocSize * (1ULL << szc);
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
Index BuddyManager<PageSize, MinAllocSize, OwnFreelist>::get_bitmap_index(void *ptr,
																		  SizeClass szc)
{
	auto ptr_offset = static_cast<char *>(ptr) - static_cast<char *>(m_managed_chunk);
	return (1 << (get_num_sizeclasses() - (szc + 1))) - 1 + ptr_offset / get_size(szc);
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
utility::ForwardList &BuddyManager<PageSize, MinAllocSize, OwnFreelist>::get_freelist(SizeClass szc)
{
	return m_freelist[szc];
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
void *BuddyManager<PageSize, MinAllocSize, OwnFreelist>::get_buddy(void *ptr, SizeClass szc)
{
	auto ptr_offset = static_cast<char *>(ptr) - static_cast<char *>(m_managed_chunk);
	auto size = get_size(szc);
	auto n_ptr = ptr_offset >> log_2(size);

	return static_cast<void *>(static_cast<char *>(m_managed_chunk) + ((n_ptr ^ 0x1) << log_2(size)));
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
void BuddyManager<PageSize, MinAllocSize, OwnFreelist>::mark_block_as_free(void *ptr,
																		   SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	assert(!block_is_free(ptr, szc));

	m_bitmap[bitmap_byte] &= ~(1 << bitmap_bit);
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
void BuddyManager<PageSize, MinAllocSize, OwnFreelist>::mark_block_as_in_use(void *ptr,
																			 SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	assert(block_is_free(ptr, szc));

	m_bitmap[bitmap_byte] |= (1 << bitmap_bit);
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
bool BuddyManager<PageSize, MinAllocSize, OwnFreelist>::block_is_free(void *ptr, SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	return (m_bitmap[bitmap_byte] & (1 << bitmap_bit)) == false;
}

template <size_t PageSize, size_t MinAllocSize, bool OwnFreelist>
void BuddyManager<PageSize, MinAllocSize, OwnFreelist>::get_allocable_sizes(
	std::vector<std::pair<size_t, size_t>> &allocable_sizes)
{
	for (Size size = MinAllocSize; size <= PageSize; size *= 2)
	{
		auto &fl = get_freelist(get_sizeclass(size));
		auto chunk = fl.peek();
		size_t num_alloc_chunks = 0;

		while (chunk)
		{
			num_alloc_chunks++;
			chunk = chunk->get_next();
		}

		if (num_alloc_chunks)
			allocable_sizes.push_back({size, num_alloc_chunks});
	}
}

}

#endif /* BUDDY_MANAGER_H */