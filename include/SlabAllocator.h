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

	SlabAllocator(Size alloc_size, Size page_size, AlignedAlloc aligned_alloc_page,
				  Free free_page) :
		m_aligned_alloc_page(aligned_alloc_page), m_free_page(free_page),
		m_alloc_size(alloc_size), m_page_size(page_size), m_freelist(), m_zerofree_list()
	{}

	void *alloc();
	void free(void *ptr);
	void remote_free(void *ptr);
	Size size();

private:

	using SlabFreeList = utility::List;

	const AlignedAlloc m_aligned_alloc_page;
	const Free m_free_page;
	const Size m_alloc_size;
	const Size m_page_size;
	SlabFreeList m_freelist;
	SlabFreeList m_zerofree_list;
	Count m_max_chunks = 0;
	Count page_count = 0;

	struct SlabPageHeader;

	SlabPageHeader *alloc_page();
	void *alloc_from_page(SlabPageHeader *page, bool new_page);
	void *alloc_from_new_page();
	SlabPageHeader *get_page(void *ptr);
};

}
}

#endif /* SLAB_ALLOCATOR_H */