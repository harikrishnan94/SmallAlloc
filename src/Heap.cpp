#include "BuddyManager/BuddyManagerMeta.h"
#include "SlabAllocator.h"
#include "Heap.h"

// constexpr static size_t BuddyPageSize = 4 * 1024 * 1024;
// constexpr static size_t BuddyMinAllocSize = 4 * 1024;

// using ForwardList = SmallAlloc::utility::ForwardList;
// using Size = SmallAlloc::Size;
// using SizeClass = SmallAlloc::SizeClass;

// constexpr int NUM_SLABS = 100;
// constexpr SizeClass size_to_sizeclass[] = {};
// constexpr Size sizeclass_to_pagesize[] = {};

// class SmallAlloc::Heap::HeapImpl
// {
// public:
// 	// constexpr static auto NUM_BUDDY_SIZECLASSES = ::BuddyManager::get_num_sizeclasses();
// 	constexpr static auto NUM_BUDDY_SIZECLASSES = 0;

// 	HeapImpl(Size alloc_limit) : m_alloc_limit(alloc_limit), m_buddy_count(0)
// 	{}

// 	void *alloc(Size size)
// 	{
// 		auto size_class = size_to_sizeclass[size];
// 		auto alloc_page = [&](Size size)
// 		{
// 			auto null = static_cast<void *>(nullptr);
// 			return std::make_pair(null, null);
// 		};

// 		return m_slab[size_class].alloc(alloc_page);
// 	}

// 	void free(void *ptr, size_t size)
// 	{
// 		auto size_class = size_to_sizeclass[size];
// 		auto free_page = [&](void *super_page, void *page, Size size)
// 		{
// 			return static_cast<void *>(nullptr);
// 		};

// 		m_slab[size_class].free(ptr, free_page);
// 	}

// private:
// 	SlabManager m_slab[NUM_SLABS];
// 	ForwardList m_sizeclass_fl[NUM_BUDDY_SIZECLASSES];
// 	ForwardList m_buddy_fl;
// 	Size m_alloc_limit;
// 	Size m_buddy_count;
// };

// SmallAlloc::Heap::Heap(size_t alloc_limit) : impl(std::make_unique<HeapImpl>(alloc_limit))
// {}

// SmallAlloc::Heap::Heap(Heap &&heap_rhs) : impl(std::move(heap_rhs.impl))
// {}

// SmallAlloc::Heap::~Heap() = default;

// void *SmallAlloc::Heap::alloc(size_t size)
// {
// 	return impl->alloc(size);
// }

// void SmallAlloc::Heap::free(void *ptr, size_t size)
// {
// 	impl->free(ptr, size);
// }
