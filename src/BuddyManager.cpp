/**
 * File: /BuddyManager.cpp
 * Project: src
 * Created Date: Saturday, October 28th 2017, 6:38:45 pm
 * Author: Harikrishnan
 */


#include "BuddyManager/BuddyManager.h"

#include <vector>

using namespace SmallAlloc::BuddyManager;
using Size = SmallAlloc::Size;
using SizeClass = SmallAlloc::SizeClass;
using Count = SmallAlloc::Count;

#define PTR_TO_INT(p)	reinterpret_cast<uintptr_t>(p)
#define INT_TO_PTR(i)	reinterpret_cast<void *>(i)

BuddyManager::BuddyManager(Size alloc_limit, std::function<void *(Size, Size)> aligned_alloc_chunk,
						   std::function<void (void *, Size)> free_chunk)
	: m_aligned_alloc_chunk(aligned_alloc_chunk), m_free_chunk(free_chunk),
	  m_freelist(), m_alloc_limit(alloc_limit), m_chunk_count(0),
	  m_num_class_sizes(BMMeta::get_num_sizeclasses()), m_chunk_meta_map()
{}

BuddyManager::~BuddyManager()
{
	std::for_each(m_chunk_meta_map.begin(), m_chunk_meta_map.end(), [this](auto cell)
	{
		if (cell.key)
		{
			m_free_chunk(cell.key, BuddyPageSize);
			m_free_chunk(cell.val, sizeof(BMMetaNode));
		}
	});
}

BuddyManager::BMMetaNode *BuddyManager::alloc_chunk()
{
	auto meta_ptr = static_cast<BMMetaNode *>(m_aligned_alloc_chunk(16, sizeof(BMMetaNode)));

	if (!meta_ptr)
		return nullptr;

	auto chunk = m_aligned_alloc_chunk(BuddyPageSize, BuddyPageSize);

	if (!chunk)
	{
		m_free_chunk(meta_ptr, sizeof(BMMetaNode));
		return nullptr;
	}

	new (meta_ptr) BMMetaNode();
	m_chunk_meta_map.insert(chunk, static_cast<void *>(meta_ptr));
	meta_ptr->m_managed_chunk = chunk;
	m_alloc_limit -= BuddyPageSize;
	m_chunk_count++;

	return meta_ptr;
}

void BuddyManager::free_chunk(BMMetaNode *meta_ptr)
{
	m_free_chunk(meta_ptr->m_managed_chunk, BuddyPageSize);
	m_chunk_meta_map.erase(meta_ptr->m_managed_chunk);

	meta_ptr->~BMMetaNode();
	m_free_chunk(meta_ptr, sizeof(BMMetaNode));
	m_alloc_limit += BuddyPageSize;
	m_chunk_count--;
}

Offset BuddyManager::get_ptr_offset(BMMetaNode *meta, BuddyManager::BuddyFreeNode *ptr) const
{
	auto ptr_offset = reinterpret_cast<char *>(ptr) - static_cast<char *>(meta->m_managed_chunk);

	assert(ptr_offset >= 0 && ptr_offset < BuddyPageSize);

	return ptr_offset;
}

BuddyManager::BuddyFreeNode *BuddyManager::get_ptr(BMMetaNode *meta, Offset ptr_offset) const
{
	return reinterpret_cast<BuddyFreeNode *>(reinterpret_cast<char *>(meta->m_managed_chunk) +
											 ptr_offset);
}

BuddyManager::BuddyFreeNode *BuddyManager::get_buddy(BMMetaNode *meta, BuddyFreeNode *ptr,
													 SizeClass szc) const
{
	return get_ptr(meta, meta->meta.get_buddy(get_ptr_offset(meta, ptr), szc));
}

bool BuddyManager::block_is_free(BMMetaNode *meta, BuddyFreeNode *ptr, SizeClass szc) const
{
	return meta->meta.block_is_free(get_ptr_offset(meta, ptr), szc);
}

void BuddyManager::mark_block_as_free(BMMetaNode *meta, BuddyFreeNode *ptr, SizeClass szc)
{
	meta->meta.mark_block_as_free(get_ptr_offset(meta, ptr), szc);
}

void BuddyManager::mark_block_as_in_use(BMMetaNode *meta, BuddyFreeNode *ptr, SizeClass szc)
{
	meta->meta.mark_block_as_in_use(get_ptr_offset(meta, ptr), szc);
}

BuddyManager::BuddyFreeNode *BuddyManager::alloc_internal(SizeClass szc)
{
	if (szc == m_num_class_sizes)
		return nullptr;

	auto &freelist = m_freelist[szc];
	auto ret_mem = static_cast<BuddyFreeNode *>(freelist.pop());

	if (ret_mem)
	{
		mark_block_as_in_use(ret_mem->meta, ret_mem, szc);
		return ret_mem;
	}

	if ((ret_mem = alloc_internal(szc + 1)))
	{
		auto buddy = get_buddy(ret_mem->meta, ret_mem, szc);
		buddy->meta = ret_mem->meta;
		freelist.push(buddy);

		mark_block_as_in_use(ret_mem->meta, ret_mem, szc);
		return ret_mem;
	}

	return nullptr;
}

bool BuddyManager::free_internal(BMMetaNode *meta, BuddyFreeNode *ptr, SizeClass szc)
{
	auto &freelist = m_freelist[szc];

	mark_block_as_free(meta, ptr, szc);

	if (szc == BMMeta::get_sizeclass(BuddyPageSize))
	{
		ptr->meta = meta;
		return true;
	}

	auto buddy = get_buddy(meta, ptr, szc);

	if (block_is_free(meta, buddy, szc))
	{
		freelist.remove(buddy);
		return free_internal(meta, std::min(ptr, buddy), szc + 1);
	}

	ptr->meta = meta;
	freelist.push(ptr);
	return false;
}

void *BuddyManager::alloc(Size size)
{
	if (size > BuddyPageSize || size < BuddyMinAllocSize)
		return nullptr;

	auto ret_mem = alloc_internal(BMMeta::get_sizeclass(size));

	if (ret_mem)
		return ret_mem;

	if (m_alloc_limit)
	{
		auto chunk_meta = alloc_chunk();

		if (chunk_meta)
		{
			auto &freelist = m_freelist[BMMeta::get_sizeclass(BuddyPageSize)];
			auto chunk = static_cast<BuddyFreeNode *>(chunk_meta->m_managed_chunk);
			chunk->meta = chunk_meta;
			freelist.push(chunk);

			return alloc_internal(BMMeta::get_sizeclass(size));
		}
	}

	return nullptr;
}

void BuddyManager::free(void *ptr, Size size)
{
	if (!ptr || size > BuddyPageSize || size < BuddyMinAllocSize)
		return;

	auto get_page = [&](auto ptr)
	{
		return INT_TO_PTR(PTR_TO_INT(ptr) - (PTR_TO_INT(ptr) & (BuddyPageSize - 1)));
	};

	auto ptr_node = static_cast<BuddyFreeNode *>(ptr);
	auto chunk = get_page(ptr);
	auto meta = static_cast<BMMetaNode *>(m_chunk_meta_map.find(chunk));

	assert(meta != nullptr);

	if (free_internal(meta, ptr_node, BMMeta::get_sizeclass(size)))
	{
		free_chunk(meta);
	}
}

SmallAlloc::Size BuddyManager::size()
{
	return m_chunk_count * (BuddyPageSize + sizeof(BMMetaNode));
}