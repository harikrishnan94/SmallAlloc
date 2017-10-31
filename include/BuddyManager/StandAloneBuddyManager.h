#ifndef STAND_ALONE_BUDDY_MANAGER_H
#define STAND_ALONE_BUDDY_MANAGER_H

#include "BuddyManager/BuddyManagerMeta.h"
#include "Utility/IList.h"

#include <cstddef>
#include <algorithm>
#include <vector>
#include <cassert>

namespace SmallAlloc
{

using BuddyFreeList = utility::ForwardList;

namespace BuddyManager
{

template <size_t PageSize, size_t MinAllocSize>
class StandAloneBuddyManager
{
public:
	StandAloneBuddyManager(void *chunk);

	void *alloc(size_t size);
	bool free(void *ptr, size_t size);

	void get_allocable_sizes(std::vector<std::pair<size_t, size_t>> &allocable_sizes) const;

private:

	Offset get_ptr_offset(void *ptr) const;
	void *get_ptr(Offset ptr_offset) const;
	void *get_buddy(void *ptr, SizeClass szc) const;
	void mark_block_as_free(void *ptr, SizeClass szc);
	void mark_block_as_in_use(void *ptr, SizeClass szc);
	bool block_is_free(void *ptr, SizeClass szc) const;

	void *m_managed_chunk;
	BuddyFreeList m_freelist[BuddyManagerMeta<PageSize, MinAllocSize>::get_num_sizeclasses_const()];
	BuddyManagerMeta<PageSize, MinAllocSize> m_meta;
};

template <size_t PageSize, size_t MinAllocSize>
StandAloneBuddyManager<PageSize, MinAllocSize>::StandAloneBuddyManager(void *chunk)
	: m_managed_chunk(chunk), m_meta()
{
	auto &freelist = m_freelist[m_meta.get_sizeclass(PageSize)];
	freelist.push(static_cast<BuddyFreeList::Node *>(chunk));
}

template <size_t PageSize, size_t MinAllocSize>
void *StandAloneBuddyManager<PageSize, MinAllocSize>::alloc(size_t size)
{
	if (size > PageSize || size < MinAllocSize)
		return nullptr;

	auto szc = m_meta.get_sizeclass(size);
	auto &freelist = m_freelist[szc];
	void *ret_mem = freelist.pop();
	void *buddy;

	if (ret_mem)
		goto found;

	ret_mem = alloc(size * 2);

	if (ret_mem == nullptr)
		return nullptr;

	buddy = get_buddy(ret_mem, szc);
	freelist.push(static_cast<BuddyFreeList::Node *>(buddy));

found:
	mark_block_as_in_use(ret_mem, szc);
	return ret_mem;
}

template <size_t PageSize, size_t MinAllocSize>
bool StandAloneBuddyManager<PageSize, MinAllocSize>::free(void *ptr, size_t size)
{
	auto szc = m_meta.get_sizeclass(size);
	auto &freelist = m_freelist[szc];

	mark_block_as_free(ptr, szc);

	if (size == PageSize)
	{
		freelist.push(static_cast<BuddyFreeList::Node *>(ptr));
		return true;
	}

	auto buddy = get_buddy(ptr, szc);

	if (block_is_free(buddy, szc))
	{
		freelist.remove(static_cast<BuddyFreeList::Node *>(buddy));
		return free(std::min(ptr, buddy), size * 2);
	}

	freelist.push(static_cast<BuddyFreeList::Node *>(ptr));
	return false;
}

template <size_t PageSize, size_t MinAllocSize>
Offset StandAloneBuddyManager<PageSize, MinAllocSize>::get_ptr_offset(void *ptr) const
{
	return static_cast<char *>(ptr) - static_cast<char *>(m_managed_chunk);
}

template <size_t PageSize, size_t MinAllocSize>
void *StandAloneBuddyManager<PageSize, MinAllocSize>::get_ptr(Offset ptr_offset) const
{
	return static_cast<char *>(m_managed_chunk) + ptr_offset;
}

template <size_t PageSize, size_t MinAllocSize>
void *StandAloneBuddyManager<PageSize, MinAllocSize>::get_buddy(void *ptr, SizeClass szc) const
{
	return get_ptr(m_meta.get_buddy(get_ptr_offset(ptr), szc));
}

template <size_t PageSize, size_t MinAllocSize>
void StandAloneBuddyManager<PageSize, MinAllocSize>::mark_block_as_free(void *ptr, SizeClass szc)
{
	m_meta.mark_block_as_free(get_ptr_offset(ptr), szc);
}

template <size_t PageSize, size_t MinAllocSize>
void StandAloneBuddyManager<PageSize, MinAllocSize>::mark_block_as_in_use(void *ptr, SizeClass szc)
{
	m_meta.mark_block_as_in_use(get_ptr_offset(ptr), szc);
}

template <size_t PageSize, size_t MinAllocSize>
bool StandAloneBuddyManager<PageSize, MinAllocSize>::block_is_free(void *ptr, SizeClass szc) const
{
	return m_meta.block_is_free(get_ptr_offset(ptr), szc);
}

template <size_t PageSize, size_t MinAllocSize>
void StandAloneBuddyManager<PageSize, MinAllocSize>::get_allocable_sizes(
	std::vector<std::pair<size_t, size_t>> &allocable_sizes) const
{
	for (Size size = MinAllocSize; size <= PageSize; size *= 2)
	{
		auto &fl = get_freelist(m_meta.get_sizeclass(size));
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
}

#endif /* STAND_ALONE_BUDDY_MANAGER_H */