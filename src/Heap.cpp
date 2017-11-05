#include "BuddyManager/BuddyManager.h"
#include "SlabSizeClass.h"
#include "SlabAllocator.h"
#include "Heap.h"
#include "jemalloc/jemalloc.h"

void *je_aligned_alloc(size_t, size_t);
void je_free(void *);

namespace SmallAlloc
{

class Heap::HeapImpl
{
public:
	static std::unique_ptr<HeapImpl> build(Size alloc_limit)
	{
		Size slab_size = sizeof(SlabAllocator::SlabAllocator) * NUM_SIZE_CLASSES;
		Size buddy_size = sizeof(BuddyManager::BuddyManager);
		auto impl = static_cast<HeapImpl *>(malloc(buddy_size + slab_size));

		new (&impl->bm) BuddyManager::BuddyManager(alloc_limit, [](auto align, auto size)
		{
#ifdef _WIN32
			return _aligned_malloc(size, align);
#else
			void *ptr = nullptr;
			posix_memalign(&ptr, align, size);
			return ptr;
#endif // _WIN32
		}, [](auto ptr, auto size)
		{
#ifdef _WIN32
			_aligned_free(ptr);
#else
			::free(ptr);
#endif // _WIN32
		});

		auto buddy_alloc = [impl](Size align, Size size)
		{
			return impl->bm.alloc(size);
		};
		auto buddy_free = [impl](void *page, Size size)
		{
			impl->bm.free(page, size);
		};

		for (auto szc = 0; szc < NUM_SIZE_CLASSES; szc++)
		{
			new (&impl->m_slab[szc]) SlabAllocator::SlabAllocator(sizeclass_to_allocsize[szc],
																  sizeclass_to_pagesize[szc],
																  buddy_alloc, buddy_free);
		}

		return std::unique_ptr<HeapImpl>(impl);
	}

	void *alloc(size_t size)
	{
		return m_slab[size_to_sizeclass[size - 1]].alloc();
	}

	void free(void *ptr, size_t size)
	{
		m_slab[size_to_sizeclass[size - 1]].free(ptr);
	}

	void remote_free(void *ptr, size_t size)
	{
		m_slab[size_to_sizeclass[size - 1]].remote_free(ptr);
	}

	size_t size()
	{
		return bm.size();
	}

private:
	BuddyManager::BuddyManager bm;
	SlabAllocator::SlabAllocator m_slab[0];
};

Heap::Heap(size_t alloc_limit) : impl(HeapImpl::build(alloc_limit))
{}

Heap::Heap(Heap &&heap_rhs) : impl(std::move(heap_rhs.impl))
{}

Heap::~Heap() = default;

void *Heap::alloc(size_t size)
{
	return impl->alloc(size);
}

void Heap::free(void *ptr, size_t size)
{
	impl->free(ptr, size);
}

void Heap::remote_free(void *ptr, size_t size)
{
	impl->free(ptr, size);
}

size_t Heap::size()
{
	return impl->size();
}

}