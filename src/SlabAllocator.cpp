/**
 * File: /SlabAllocator.cpp
 * Project: src
 * Created Date: Sunday, October 29th 2017, 4:51:36 pm
 * Author: Harikrishnan
 */


#include "SlabAllocator.h"

#include <cassert>

using namespace SmallAlloc::SlabAllocator;

using SlabPageFreeList = SmallAlloc::utility::FreeList;
using SlabPageRemoteFreeList = SmallAlloc::utility::FreeListAtomic;
using FreeNode = SmallAlloc::utility::FreeList::Node;

#define VOID_PTR(p)			static_cast<void *>(p)
#define FREE_NODE(p)		static_cast<FreeNode *>(p)
#define SLAB_HEADER(p)		static_cast<SlabPageHeader *>(p)
#define LIST_FREE_NODE(p)	static_cast<SlabFreeList::Node *>(p)

#define PTR_TO_INT(p)	reinterpret_cast<uintptr_t>(p)
#define INT_TO_PTR(i)	reinterpret_cast<void *>(i)

struct SlabAllocator::SlabPageHeader : public SmallAlloc::utility::List::Node
{
	static SlabPageHeader *build(void *page, void *extra, size_t page_size, Size alloc_size)
	{
		static_assert(sizeof(SlabAllocator::SlabPageHeader) == 64,
					  "SlabPageHeader cannot be stored in 48 bytes");

		return new (page) SlabPageHeader(extra, page_size, alloc_size);
	}

	void *get_extra()
	{
		return m_extra;
	}

	Count free_count()
	{
		return m_num_free + m_freelist_size;
	}

	void *alloc()
	{
		auto free_node = m_native_fl.pop();

		if (free_node)
		{
			m_freelist_size--;
			return VOID_PTR(free_node);
		}

		if (m_num_free)
			return VOID_PTR(this->data + (--m_num_free) * m_alloc_size);

		if (reclaim_remote_free())
			return alloc();

		return nullptr;
	}

	bool reclaim_remote_free()
	{
		auto free_node = m_remote_fl.popAll();

		if (free_node)
		{
			do
			{
				auto next = free_node->get_next();
				m_native_fl.push(free_node);
				++m_freelist_size;
				free_node = next;
			}
			while (free_node);

			return true;
		}

		return false;
	}

	Count free(void *ptr)
	{
		++m_freelist_size;
		m_native_fl.push(FREE_NODE(ptr));

		return m_num_free + m_freelist_size;
	}

	void remote_free(void *ptr)
	{
		m_remote_fl.push(FREE_NODE(ptr));
	}

	Count total_alloc_chunks(Size page_size)
	{
		return (page_size - sizeof(*this)) / m_alloc_size;
	}

private:
	void *m_extra;
	SlabPageFreeList m_native_fl;
	SlabPageRemoteFreeList m_remote_fl;
	Size m_alloc_size;
	Count m_num_free;
	Count m_freelist_size;
	char data[];

	SlabPageHeader(void *extra, Size page_size, Size alloc_size)
		: m_extra(extra), m_native_fl(), m_remote_fl(), m_alloc_size(alloc_size),
		  m_num_free((page_size - sizeof(SlabPageHeader)) / alloc_size),
		  m_freelist_size(0)
	{
		assert(m_num_free > 0);
	}
};

SlabAllocator::SlabPageHeader *SlabAllocator::alloc_page()
{
	auto page_info = m_aligned_alloc_page(m_page_size, m_page_size);
	auto super_page = page_info.first;
	auto page = page_info.second;

	if (!page)
		return nullptr;

	auto header = SlabPageHeader::build(page, super_page, m_page_size, m_alloc_size);

	if (m_max_chunks == 0)
		m_max_chunks = header->total_alloc_chunks(m_page_size);

	page_count++;

	return header;
}

void *SlabAllocator::alloc_from_page(SlabPageHeader *page, bool new_page)
{
	auto ret_ptr = page->alloc();

	if (ret_ptr)
	{
		if (page->free_count() != 0)
		{
			if (!new_page)
				m_freelist.move_front(page);
			else
				m_freelist.push_front(page);

		}
		else
		{
			m_freelist.remove(page);
			m_zerofree_list.push_back(page);
		}
	}
	else
	{
		assert(!new_page);

		if (page->reclaim_remote_free())
			return alloc_from_page(page, new_page);

		m_freelist.remove(page);
		m_zerofree_list.push_back(page);

	}

	return ret_ptr;
}

void *SlabAllocator::alloc_from_new_page()
{
	auto page = alloc_page();

	if (!page)
		return nullptr;

	return alloc_from_page(page, true);
}

void *SlabAllocator::alloc()
{
	if (!m_freelist.empty())
	{
		auto active_page = SLAB_HEADER(m_freelist.front());

		if (auto ret_ptr = alloc_from_page(active_page, false))
			return ret_ptr;

		if (!m_freelist.empty())
		{
			active_page = SLAB_HEADER(m_freelist.front());

			assert(active_page->free_count() != 0);

			return alloc_from_page(active_page, false);
		}
	}

	return alloc_from_new_page();
}

SlabAllocator::SlabPageHeader *SlabAllocator::get_page(void *ptr)
{
	return SLAB_HEADER(INT_TO_PTR(PTR_TO_INT(ptr) - (PTR_TO_INT(ptr) & (m_page_size - 1))));
}

void SlabAllocator::free(void *ptr)
{
	auto page = get_page(ptr);

	if (page->free_count() == 0)
	{
		auto free_count = page->free(ptr);
		m_zerofree_list.remove(page);

		assert(free_count != m_max_chunks);
		(void) free_count;

		m_freelist.push_front(page);
	}
	else
	{
		if (page->free(ptr) != m_max_chunks)
		{
			m_freelist.move_front(page);
		}
		else
		{
			m_freelist.remove(page);
			m_free_page(page->get_extra(), VOID_PTR(page), m_page_size);
			page_count--;
		}
	}
}

SmallAlloc::Size SlabAllocator::size()
{
	return page_count * m_page_size;
}

void SlabAllocator::remote_free(void *ptr)
{
	auto page = get_page(ptr);
	page->remote_free(ptr);
}