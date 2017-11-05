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
	using AlignedAlloc = std::function<void *(Size, Size)>;
	using Free = std::function<void (void *page, Size)>;

	SlabAllocator(uint32_t alloc_size, uint32_t page_size, AlignedAlloc aligned_alloc_page,
				  Free free_page);

	void *alloc();
	void free(void *ptr);
	void remote_free(void *ptr);
	bool reclaim_remote_free();
	Size size();

private:

	using SlabPageList = utility::List;
	using SlabObjectRemoteFreeList = utility::FreeListAtomic;

	struct SlabPageHeader;

	const AlignedAlloc m_aligned_alloc_page;
	const Free m_free_page;
	const Size m_alloc_size;
	const Size m_page_size;
	const Count m_max_alloc_count;
	Count m_page_count = 0;
	SlabPageHeader *m_first_page;
	SlabPageList m_freelist;
	SlabPageList m_fullpages_list;
	SlabObjectRemoteFreeList m_remote_freelist;

	SlabPageHeader *alloc_page();
	void *alloc_from_first_page();
	void *alloc_from_new_page();
	SlabPageHeader *get_page(void *ptr);
};

}
}

#endif /* SLAB_ALLOCATOR_H */