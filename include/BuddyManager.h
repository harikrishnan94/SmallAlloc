#ifndef BUDDY_MANAGER_H
#define BUDDY_MANAGER_H

#include "Utility.h"

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

template <size_t PageSize, size_t MinAllocSize>
class BuddyManager
{
public:
	BuddyManager(void *meta, void *chunk);

	static constexpr size_t get_meta_size();

	void *alloc(size_t size);
	bool free(void *ptr, size_t size);
	void get_allocable_sizes(std::vector<std::pair<size_t, size_t>> &allocable_sizes);

private:

	struct BuddyManagerImpl;

	BuddyManagerImpl *m_impl;
	const void *m_managed_chunk;

	ptrdiff_t get_ptr_offset(void *ptr);
	void *get_ptr(ptrdiff_t ptr_offset);
};

using Count = size_t;
using Size = size_t;
using SizeClass = size_t;
using Index = size_t;

template <size_t PageSize, size_t MinAllocSize>
struct BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl
{
	BuddyManagerImpl();

	static constexpr SizeClass get_sizeclass(Size size);

	ForwardList &get_freelist(SizeClass szc);
	ptrdiff_t get_buddy(ptrdiff_t ptr_offset, SizeClass szc);
	void mark_block_as_free(ptrdiff_t ptr_offset, SizeClass szc);
	void mark_block_as_in_use(ptrdiff_t ptr_offset, SizeClass szc);
	bool block_is_free(ptrdiff_t ptr_offset, SizeClass szc);

private:

	static constexpr Count get_num_sizeclasses();
	static constexpr Size get_bitmap_size();
	static constexpr Size get_size(SizeClass szc);
	static constexpr size_t get_normalized_ptr(ptrdiff_t ptr_offset);
	static constexpr Index get_bitmap_index(ptrdiff_t ptr_offset, SizeClass szc);

	ForwardList m_freelist[get_num_sizeclasses()];
	char m_bitmap[get_bitmap_size()];
};


template <size_t PageSize, size_t MinAllocSize>
BuddyManager<PageSize, MinAllocSize>::BuddyManager(void *meta, void *chunk)
	: m_impl(new (meta) BuddyManagerImpl()), m_managed_chunk(chunk)
{
	auto &freelist = m_impl->get_freelist(m_impl->get_sizeclass(PageSize));
	freelist.push(static_cast<ForwardList::Node *>(chunk));
}

template <size_t PageSize, size_t MinAllocSize>
ptrdiff_t BuddyManager<PageSize, MinAllocSize>::get_ptr_offset(void *ptr)
{
	assert(ptr >= m_managed_chunk);
	return static_cast<char *>(ptr) - static_cast<char *>(const_cast<void *>(m_managed_chunk));
}

template <size_t PageSize, size_t MinAllocSize>
void *BuddyManager<PageSize, MinAllocSize>::get_ptr(ptrdiff_t ptr_offset)
{
	assert(ptr_offset <= PageSize);
	return static_cast<void *>(static_cast<char *>(const_cast<void *>(m_managed_chunk)) + ptr_offset);
}

template <size_t PageSize, size_t MinAllocSize>
void *BuddyManager<PageSize, MinAllocSize>::alloc(size_t size)
{
	if (size > PageSize || size < MinAllocSize)
		return nullptr;

	auto szc = m_impl->get_sizeclass(size);
	auto &freelist = m_impl->get_freelist(szc);
	void *ret_mem = freelist.pop();
	void *buddy;

	if (ret_mem)
		goto found;

	ret_mem = alloc(size * 2);

	if (ret_mem == nullptr)
		return nullptr;

	buddy = get_ptr(m_impl->get_buddy(get_ptr_offset(ret_mem), szc));
	freelist.push(static_cast<ForwardList::Node *>(buddy));

found:
	m_impl->mark_block_as_in_use(get_ptr_offset(ret_mem), szc);
	return ret_mem;
}

template <size_t PageSize, size_t MinAllocSize>
bool BuddyManager<PageSize, MinAllocSize>::free(void *ptr, size_t size)
{
	auto szc = m_impl->get_sizeclass(size);
	auto &freelist = m_impl->get_freelist(szc);

	m_impl->mark_block_as_free(get_ptr_offset(ptr), szc);

	if (size == PageSize)
	{
		freelist.push(static_cast<ForwardList::Node *>(ptr));
		return true;
	}

	auto buddy = get_ptr(m_impl->get_buddy(get_ptr_offset(ptr), szc));

	if (m_impl->block_is_free(get_ptr_offset(buddy), szc))
	{
		freelist.remove(static_cast<ForwardList::Node *>(buddy));
		return free(std::min(ptr, buddy), size * 2);
	}

	freelist.push(static_cast<ForwardList::Node *>(ptr));
	return false;
}

template <size_t PageSize, size_t MinAllocSize>
constexpr size_t BuddyManager<PageSize, MinAllocSize>::get_meta_size()
{
	return sizeof(BuddyManagerImpl);
}

constexpr size_t log_2(size_t n)
{
	assert(n > 0);
	auto lz = leading_zeroes(n);
	return sizeof(n) * 8 - lz - 1;
}

template <size_t PageSize, size_t MinAllocSize>
BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::BuddyManagerImpl()
{
	memset(m_bitmap, 0, get_bitmap_size());
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Count BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_num_sizeclasses()
{
	return log_2(PageSize) - log_2(MinAllocSize) + 1;
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Size BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_bitmap_size()
{
	return ((PageSize / MinAllocSize) * 2) / 8;
}

template <size_t PageSize, size_t MinAllocSize>
constexpr SizeClass BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_sizeclass(
	Size size)
{
	assert(size >= MinAllocSize && size <= PageSize);

	return log_2(size) - log_2(MinAllocSize);
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Size BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_size(SizeClass szc)
{
	assert(szc < get_num_sizeclasses());

	return MinAllocSize * (1 << szc);
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Index BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_bitmap_index(
	ptrdiff_t ptr_offset, SizeClass szc)
{
	return (1 << (get_num_sizeclasses() - (szc + 1))) - 1 + ptr_offset / get_size(szc);
}

template <size_t PageSize, size_t MinAllocSize>
ForwardList &BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_freelist(SizeClass szc)
{
	return m_freelist[szc];
}

template <size_t PageSize, size_t MinAllocSize>
ptrdiff_t BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_buddy(ptrdiff_t ptr_offset,
																			SizeClass szc)
{
	auto size = get_size(szc);
	auto n_ptr = ptr_offset >> log_2(size);

	return (n_ptr ^ 0x1) << log_2(size);
}

template <size_t PageSize, size_t MinAllocSize>
void BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::mark_block_as_free(
	ptrdiff_t ptr_offset, SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr_offset, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	assert(!block_is_free(ptr_offset, szc));

	m_bitmap[bitmap_byte] &= ~(1 << bitmap_bit);
}

template <size_t PageSize, size_t MinAllocSize>
void BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::mark_block_as_in_use(
	ptrdiff_t ptr_offset, SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr_offset, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	assert(block_is_free(ptr_offset, szc));

	m_bitmap[bitmap_byte] |= (1 << bitmap_bit);
}

template <size_t PageSize, size_t MinAllocSize>
bool BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::block_is_free(ptrdiff_t ptr_offset,
																		   SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr_offset, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	return (m_bitmap[bitmap_byte] & (1 << bitmap_bit)) == false;
}

template <size_t PageSize, size_t MinAllocSize>
void BuddyManager<PageSize, MinAllocSize>::get_allocable_sizes(
	std::vector<std::pair<size_t, size_t>> &allocable_sizes)
{
	for (Size size = MinAllocSize; size <= PageSize; size *= 2)
	{
		auto &fl = m_impl->get_freelist(m_impl->get_sizeclass(size));
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