#ifndef SLAB_ALLOCATOR_H
#define SLAB_ALLOCATOR_H

#include "common.h"
#include "Utility/IList.h"

#include <functional>

namespace SmallAlloc
{

namespace SlabAllocator
{

class SlabAllocator
{
public:
	using AlignedAlloc = std::function<std::pair<void *, void *> (Size, Size)>;
	using Free = std::function<void (void *extra, void *page, Size)>;

	SlabAllocator(uint32_t alloc_size, uint32_t page_size, AlignedAlloc aligned_alloc_page,
				  Free free_page);

	void *alloc();
	void free(void *ptr);
	void remote_free(void *ptr);
	bool reclaim_remote_free();
	Size size();

private:

	using SlabFreeList = utility::List;
	using SlabRemoteFreeList = utility::FreeListAtomic;

	const AlignedAlloc m_aligned_alloc_page;
	const Free m_free_page;
	const Size m_alloc_size;
	const Size m_page_size;
	const Count m_max_alloc_count;
	Count page_count = 0;
	SlabFreeList m_freelist;
	SlabFreeList m_fullpages_list;
	SlabRemoteFreeList m_remote_freelist;

	struct SlabPageHeader;

	SlabPageHeader *alloc_page();
	void *alloc_from_page(SlabPageHeader *page, bool new_page);
	void *alloc_from_new_page();
	SlabPageHeader *get_page(void *ptr);
};

}
}

#endif /* SLAB_ALLOCATOR_H */