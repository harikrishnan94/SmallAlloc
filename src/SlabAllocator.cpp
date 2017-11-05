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
using SlabFullPageList = SmallAlloc::utility::List;
using SlabObjectRemoteFreeList = SmallAlloc::utility::FreeListAtomic;
using FreeNode = SlabObjectRemoteFreeList::Node;

#define VOID_PTR(p)			static_cast<void *>(p)
#define FREE_NODE(p)		static_cast<FreeNode *>(p)
#define SLAB_HEADER(p)		static_cast<SlabPageHeader *>(p)
#define LIST_FREE_NODE(p)	static_cast<SlabFreeList::Node *>(p)

#define PTR_TO_INT(p)	reinterpret_cast<uintptr_t>(p)
#define INT_TO_PTR(i)	reinterpret_cast<void *>(i)

#define PAGE_IS_FULL(page)	((page->free_count()) == 0)
#define PAGE_WAS_FULL(page)	((page->free_count()) == 1)
#define PAGE_IS_EMPTY(page)	((page->free_count()) == m_max_alloc_count)

struct SlabAllocator::SlabPageHeader : public SlabFullPageList::Node
{
	static SlabPageHeader *build(void *page, uint32_t page_size, uint32_t alloc_size,
								 uint32_t max_alloc_count)
	{
		static_assert(sizeof(SlabAllocator::SlabPageHeader) == 40,
					  "SlabPageHeader cannot be stored in 40 bytes");
		assert(max_alloc_count > 1);

		return new (page) SlabPageHeader(page_size, alloc_size, max_alloc_count);
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
	utility::FreeList m_native_fl;
	uint32_t m_alloc_size;
	uint32_t m_num_free;
	uint32_t m_freelist_size;
	uint32_t padding;
	char data[];

	SlabPageHeader(uint32_t page_size, uint32_t alloc_size, uint32_t max_alloc_count)
		: m_native_fl(), m_alloc_size(alloc_size),
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
	return SlabPageHeader::build(page, m_page_size, m_alloc_size, m_max_alloc_count);
}

void *SlabAllocator::alloc_from_first_page()
{
	auto ret_ptr = m_first_page->alloc();

	assert(ret_ptr != nullptr);

	if (PAGE_IS_FULL(m_first_page))
	{
		m_fullpages_list.push_back(m_first_page);
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
		m_first_page = SLAB_HEADER(m_freelist.pop_front());
		return alloc_from_first_page();
	}

	return alloc_from_new_page();
}

void SlabAllocator::free(void *ptr)
{
	auto page = get_page(ptr);

	page->free(ptr);

	if (PAGE_WAS_FULL(page))
	{
		assert(page != m_first_page);

		m_fullpages_list.remove(page);
		m_freelist.push_back(page);
	}
	else
	{
		if (PAGE_IS_EMPTY(page))
		{
			if (page != m_first_page)
			{
				m_freelist.remove(page);
				m_free_page(page, m_page_size);
				m_page_count--;
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