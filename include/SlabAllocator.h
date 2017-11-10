#ifndef SLAB_ALLOCATOR_H
#define SLAB_ALLOCATOR_H

#include "common.h"
#include "Utility/IList.h"

#include <functional>
#include <cassert>

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

	using object_pointer_t = uint16_t;
	using object_count_t = uint16_t;

#define SLAB_PAGE_SKIP_SIZE (SLAB_PAGE_HEADER_SIZE + sizeof(SlabPageList::Node))
#define ADDRESS_OF(ind)		(reinterpret_cast<void *>(reinterpret_cast<char *>(this) + \
								SLAB_PAGE_SKIP_SIZE + (ind) * m_object_size))
#define INDEX_OF(ptr)		((static_cast<char *>(ptr) - SLAB_PAGE_SKIP_SIZE - \
								reinterpret_cast<char *>(this)) / m_object_size)
#define INDEX_AT(ind)		(*static_cast<object_pointer_t *>(ADDRESS_OF(ind)))


	struct SlabPageHeader
	{
		SlabPageHeader()
		{}

		SlabPageHeader(const SlabPageHeader &rhs)
		{
			this->align = rhs.align;
		}

		static SlabPageHeader *build(void *page, uint16_t object_size, object_count_t max_object_count)
		{
			static_assert(sizeof(SlabAllocator::SlabPageHeader) == SLAB_PAGE_HEADER_SIZE,
						  "SlabPageHeader cannot be stored in 16 bytes");

			return new (page) SlabPageHeader(object_size, max_object_count);
		}

		void *alloc()
		{
			auto free_ind = m_next_object;

			assert(free_ind <= m_max_object_count);

			if (free_ind < m_max_object_count)
			{
				m_next_object++;
				m_free_count--;
				return ADDRESS_OF(free_ind);
			}

			free_ind = m_native_fl;

			assert(free_ind <= m_max_object_count);

			if (free_ind < m_max_object_count)
			{
				m_native_fl = INDEX_AT(free_ind);
				m_free_count--;
				return ADDRESS_OF(free_ind);
			}

			return nullptr;
		}

		void free(void *ptr)
		{
			auto free_ind = INDEX_OF(ptr);

			assert(free_ind <= m_max_object_count);

			INDEX_AT(free_ind) = m_native_fl;
			m_native_fl = free_ind;
			m_free_count++;

			assert(m_free_count <= m_max_object_count);
		}

		inline bool is_page_full()
		{
			return m_free_count == 0;
		}

		inline bool was_page_full()
		{
			return m_free_count == 1;
		}

		inline bool is_page_empty()
		{
			return m_free_count == m_max_object_count;
		}

	private:

		static constexpr auto SLAB_PAGE_HEADER_SIZE = 16;

		union
		{
			struct
			{
				object_pointer_t m_next_object;
				object_pointer_t m_native_fl;
				object_count_t m_free_count;
				object_pointer_t m_object_size;
				object_count_t m_max_object_count;
			};

			std::aligned_storage_t<SLAB_PAGE_HEADER_SIZE> align;
		};

		SlabPageHeader(uint16_t object_size, object_count_t max_object_count) : m_next_object(0),
			m_native_fl(max_object_count), m_free_count(max_object_count), m_object_size(object_size),
			m_max_object_count(max_object_count)
		{}
	};

#undef SLAB_PAGE_SKIP_SIZE
#undef ADDRESS_OF
#undef INDEX_OF
#undef INDEX_AT

private:

	using SlabPageList = utility::List;
	using SlabObjectRemoteFreeList = utility::FreeListAtomic;

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