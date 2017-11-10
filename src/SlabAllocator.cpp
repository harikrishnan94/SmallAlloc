/**
 * File: /SlabAllocator.cpp
 * Project: src
 * Created Date: Sunday, October 29th 2017, 4:51:36 pm
 * Author: Harikrishnan
 */


#include "SlabAllocator.h"

#include <cassert>
#include <iostream>

using namespace SmallAlloc::SlabAllocator;

using SlabPageList = SmallAlloc::utility::List;
using SlabObjectRemoteFreeList = SmallAlloc::utility::FreeListAtomic;
using FreeNode = SlabObjectRemoteFreeList::Node;


#define VOID_PTR(p)		static_cast<void *>(p)
#define FREE_NODE(p)	static_cast<FreeNode *>(p)
#define SLAB_HEADER(p)	reinterpret_cast<SlabPageHeader *>(p)

#define PTR_TO_INT(p)	reinterpret_cast<uintptr_t>(p)
#define INT_TO_PTR(i)	reinterpret_cast<void *>(i)

#define PAGE_IS_FULL(page)	((page)->free_count == 0)
#define PAGE_WAS_FULL(page)	((page)->free_count == 1)
#define PAGE_IS_EMPTY(page)	((page)->free_count == (page)->m_max_object_count)

struct SlabPageNode
{
	SlabAllocator::SlabPageHeader header;
	SlabPageList::Node list_node;
};

#define FREE_NODE_PTR_FROM_PAGE(p) reinterpret_cast<SlabPageList::Node *>(reinterpret_cast<char *>(p) + \
																			sizeof(SlabPageHeader))
#define PAGE_PTR_FROM_FREE_NODE(p) SLAB_HEADER(reinterpret_cast<char *>(p) - sizeof(SlabPageList::Node))

SlabAllocator::SlabAllocator(uint32_t alloc_size, uint32_t page_size,
							 AlignedAlloc aligned_alloc_page, Free free_page)
	: m_aligned_alloc_page(aligned_alloc_page), m_free_page(free_page),
	  m_alloc_size(alloc_size), m_page_size(page_size),
	  m_max_alloc_count((page_size - sizeof(SlabPageNode)) / alloc_size),
	  m_first_page(nullptr), m_freelist(), m_fullpages_list(), m_remote_freelist()
{}

SlabAllocator::SlabPageHeader *SlabAllocator::get_page(void *ptr)
{
	return SLAB_HEADER(INT_TO_PTR(PTR_TO_INT(ptr) - (PTR_TO_INT(ptr) & (m_page_size - 1))));
}

SmallAlloc::Size SlabAllocator::size()
{
	return m_page_count * m_page_size;
}

void SlabAllocator::remote_free(void *ptr)
{
	m_remote_freelist.push(FREE_NODE(ptr));
}

SlabAllocator::SlabPageHeader *SlabAllocator::alloc_page()
{
	auto page = m_aligned_alloc_page(m_page_size, m_page_size);

	if (!page)
		return nullptr;

	m_page_count++;
	return SlabPageHeader::build(page, m_alloc_size, m_max_alloc_count);
}

void *SlabAllocator::alloc_from_first_page()
{
	auto ret_ptr = m_first_page->alloc();

	assert(ret_ptr != nullptr);

	if (m_first_page->is_page_full())
	{
		m_fullpages_list.push_back(FREE_NODE_PTR_FROM_PAGE(m_first_page));
		m_first_page = nullptr;
	}

	return ret_ptr;
}

void *SlabAllocator::alloc_from_new_page()
{
	assert(m_first_page == nullptr);

	m_first_page = alloc_page();

	return m_first_page ? alloc_from_first_page() : nullptr;
}

void *SlabAllocator::alloc()
{
	if (m_first_page)
	{
		return alloc_from_first_page();

		if (reclaim_remote_free())
			return alloc_from_first_page();
	}

	if (!m_freelist.empty())
	{
		m_first_page = PAGE_PTR_FROM_FREE_NODE(m_freelist.pop_front());
		return alloc_from_first_page();
	}

	return alloc_from_new_page();
}

void SlabAllocator::free(void *ptr)
{
	auto page = get_page(ptr);

	page->free(ptr);

	if (page->was_page_full())
	{
		assert(page != m_first_page);

		m_fullpages_list.remove(FREE_NODE_PTR_FROM_PAGE(page));
		m_freelist.push_back(FREE_NODE_PTR_FROM_PAGE(page));
	}
	else
	{
		if (page->is_page_empty())
		{
			if (page != m_first_page)
			{
				m_freelist.remove(FREE_NODE_PTR_FROM_PAGE(page));

				if (m_page_count)
				{
					m_free_page(page, m_page_size);
					m_page_count--;
				}
				else
				{
					m_freelist.push_front(FREE_NODE_PTR_FROM_PAGE(page));
				}
			}
		}
	}
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

	return m_first_page != nullptr;
}