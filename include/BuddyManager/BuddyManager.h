/**
 * File: /BuddyManager.h
 * Project: BuddyManager
 * Created Date: Saturday, October 28th 2017, 3:01:36 pm
 * Author: Harikrishnan
 */


#ifndef BUDDYMANAGER_H
#define BUDDYMANAGER_H

#include "Utility/IList.h"
#include "BuddyManager/BuddyManagerMeta.h"
#include "Utility/PointerHashMap.h"

#include <functional>

namespace SmallAlloc
{

using BuddyFreeList = utility::ForwardList;

namespace BuddyManager
{

class BuddyManager
{
public:
	BuddyManager(Size alloc_limit, std::function<void *(Size, Size)> aligned_alloc_chunk,
				 std::function<void (void *, Size)> free_chunk);
	~BuddyManager();

	BuddyManager(const BuddyManager &bm) = delete;
	BuddyManager(BuddyManager &&bm) = delete;

	void *alloc(Size size);
	void free(void *ptr, Size size);
	Size size();

	constexpr auto get_min_alloc_size()
	{
		return BuddyMinAllocSize;
	}

	constexpr auto get_page_size()
	{
		return BuddyPageSize;
	}

private:

	constexpr static size_t BuddyPageSize = 4 * 1024 * 1024;
	constexpr static size_t BuddyMinAllocSize = 4 * 1024;

	using BMMeta = BuddyManagerMeta<BuddyPageSize, BuddyMinAllocSize>;

	struct BMMetaNode
	{
		BMMeta meta;
		void *m_managed_chunk;

		BMMetaNode() = default;
		~BMMetaNode() = default;
	};

	struct BuddyFreeNode : BuddyFreeList::Node
	{
		BMMetaNode *meta;

		BuddyFreeNode() = default;
		~BuddyFreeNode() = default;
	};

	BuddyManager::BuddyFreeNode *alloc_internal(SizeClass szc);
	bool free_internal(BMMetaNode *meta, BuddyFreeNode *ptr, SizeClass szc);
	Offset get_ptr_offset(BMMetaNode *meta, BuddyFreeNode *ptr) const;
	BuddyFreeNode *get_ptr(BMMetaNode *meta, Offset ptr_offset) const;
	BuddyFreeNode *get_buddy(BMMetaNode *meta, BuddyFreeNode *ptr, SizeClass szc) const;
	void mark_block_as_free(BMMetaNode *meta, BuddyFreeNode *ptr, SizeClass szc);
	void mark_block_as_in_use(BMMetaNode *meta, BuddyFreeNode *ptr, SizeClass szc);
	bool block_is_free(BMMetaNode *meta, BuddyFreeNode *ptr, SizeClass szc) const;

	BMMetaNode *alloc_chunk();
	void free_chunk(BMMetaNode *meta);

	std::function<void *(Size, Size)> m_aligned_alloc_chunk;
	std::function<void (void *, Size)> m_free_chunk;
	BuddyFreeList m_freelist[BMMeta::get_num_sizeclasses()];
	Size m_alloc_limit;
	Size m_chunk_count;
	Count m_num_class_sizes;
	SmallAlloc::utility::PointerHashMap m_chunk_meta_map;
};

}

}

#endif /* BUDDYMANAGER_H */
