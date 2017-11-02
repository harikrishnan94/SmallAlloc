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

#define PAGE_IS_FULL(free_count)	((free_count) == 0)
#define PAGE_IS_EMPTY(free_count)	((free_count) == m_max_alloc_count)

struct SlabAllocator::SlabPageHeader : public SmallAlloc::utility::List::Node
{
	static SlabPageHeader *build(void *page, void *extra, uint32_t page_size, uint32_t alloc_size,
								 uint32_t max_alloc_count)
	{
		static_assert(sizeof(SlabAllocator::SlabPageHeader) == 48,
					  "SlabPageHeader cannot be stored in 48 bytes");
		assert(max_alloc_count > 1);

		return new (page) SlabPageHeader(extra, page_size, alloc_size, max_alloc_count);
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

		return nullptr;
	}

	Count free(void *ptr)
	{
		++m_freelist_size;
		m_native_fl.push(FREE_NODE(ptr));

		return m_num_free + m_freelist_size;
	}

private:
	void *m_extra;
	SlabPageFreeList m_native_fl;
	uint32_t m_alloc_size;
	uint32_t m_num_free;
	uint32_t m_freelist_size;
	uint32_t padding;
	char data[];

	SlabPageHeader(void *extra, uint32_t page_size, uint32_t alloc_size, uint32_t max_alloc_count)
		: m_extra(extra), m_native_fl(), m_alloc_size(alloc_size),
		  m_num_free(max_alloc_count), m_freelist_size(0)
	{
		assert(m_num_free > 0);
		(void) padding;
	}
};

SlabAllocator::SlabAllocator(uint32_t alloc_size, uint32_t page_size,
							 AlignedAlloc aligned_alloc_page, Free free_page)
	: m_aligned_alloc_page(aligned_alloc_page), m_free_page(free_page),
	  m_alloc_size(alloc_size), m_page_size(page_size),
	  m_max_alloc_count((page_size - sizeof(SlabPageHeader)) / alloc_size),
	  m_freelist(), m_fullpages_list(), m_remote_freelist()
{}

SlabAllocator::SlabPageHeader *SlabAllocator::alloc_page()
{
	auto page_info = m_aligned_alloc_page(m_page_size, m_page_size);
	auto super_page = page_info.first;
	auto page = page_info.second;

	if (!page)
		return nullptr;

	page_count++;
	return SlabPageHeader::build(page, super_page, m_page_size, m_alloc_size, m_max_alloc_count);
}

void *SlabAllocator::alloc_from_page(SlabPageHeader *page, bool new_page)
{
	auto ret_ptr = page->alloc();

	assert(ret_ptr != nullptr);

	if (PAGE_IS_FULL(page->free_count()))
	{
		m_freelist.remove(page);
		m_fullpages_list.push_back(page);
	}
	else
	{
		if (new_page)
			m_freelist.push_front(page);
		else
			m_freelist.move_front(page);
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
		return alloc_from_page(SLAB_HEADER(m_freelist.front()), false);

	if (reclaim_remote_free())
		return alloc_from_page(SLAB_HEADER(m_freelist.front()), false);

	return alloc_from_new_page();
}

SlabAllocator::SlabPageHeader *SlabAllocator::get_page(void *ptr)
{
	return SLAB_HEADER(INT_TO_PTR(PTR_TO_INT(ptr) - (PTR_TO_INT(ptr) & (m_page_size - 1))));
}

void SlabAllocator::free(void *ptr)
{
	auto page = get_page(ptr);

	if (PAGE_IS_FULL(page->free_count()))
	{
		page->free(ptr);
		m_fullpages_list.remove(page);
		m_freelist.push_front(page);
	}
	else
	{
		if (PAGE_IS_EMPTY(page->free(ptr)) && page_count > 1)
		{
			m_freelist.remove(page);
			m_free_page(page->get_extra(), VOID_PTR(page), m_page_size);
			page_count--;
		}
		else
		{
			m_freelist.move_front(page);
		}
	}
}

SmallAlloc::Size SlabAllocator::size()
{
	return page_count * m_page_size;
}

void SlabAllocator::remote_free(void *ptr)
{
	m_remote_freelist.push(FREE_NODE(ptr));
}

bool SlabAllocator::reclaim_remote_free()
{
	auto free_node = m_remote_freelist.popAll();

	if (!free_node)
		return false;

	do
	{
		auto ptr = VOID_PTR(free_node);
		free_node = free_node->get_next();
		free(ptr);
	}
	while (free_node);

	return !PAGE_IS_FULL(SLAB_HEADER(m_freelist.front())->free_count());
}