#include "BuddyManager.h"
#include "Utility.h"

#include <algorithm>
#include <cassert>


#ifdef __LINUX__
#define leading_zeroes(x) __builtin_clz(x)
#endif /* __LINUX__ */

#ifdef __APPLE__
#define leading_zeroes(x) __builtin_clz(x)
#endif /* __APPLE__ */

#ifdef _WIN32
#include <intrin.h>
#define leading_zeroes(x) __lzcnt64(x)
#endif /* _WIN32 */

using namespace SmallAlloc;

using Count = size_t;
using Size = size_t;
using SizeClass = size_t;
using Index = size_t;

template <size_t PageSize, size_t MinAllocSize>
struct BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl
{
	BuddyManagerImpl();

	static constexpr SizeClass get_sizeclass(Size size);

	List &get_freelist(SizeClass szc);
	void *get_buddy(void *ptr, SizeClass szc);
	void mark_block_as_free(void *ptr, SizeClass szc);
	void mark_block_as_in_use(void *ptr, SizeClass szc);
	bool block_is_free(void *ptr, SizeClass szc);

private:

	static constexpr Count get_num_sizeclasses();
	static constexpr Size get_bitmap_size();
	static constexpr Size get_size(SizeClass szc);
	static constexpr size_t get_normalized_ptr(void *ptr);
	static constexpr Index get_bitmap_index(void *ptr, SizeClass szc);

	List m_freelist[get_num_sizeclasses()];
	char m_bitmap[get_bitmap_size()];
};


template <size_t PageSize, size_t MinAllocSize>
BuddyManager<PageSize, MinAllocSize>::BuddyManager(void *meta, void *chunk)
	: m_managed_chunk(chunk), m_impl(new (meta) BuddyManagerImpl())
{
	auto freelist = m_impl->get_freelist(m_impl->get_sizeclass(PageSize));
	freelist.push_front(m_managed_chunk);
}

template <size_t PageSize, size_t MinAllocSize>
void *BuddyManager<PageSize, MinAllocSize>::alloc(size_t size)
{
	auto szc = m_impl->get_sizeclass(size);
	auto freelist = m_impl->get_freelist(szc);
	void *ret_mem = freelist->pop_front();
	void *buddy;

	if (ret_mem)
		goto found;

	ret_mem = alloc(size * 2);

	if (ret_mem == nullptr)
		return nullptr;

	buddy = m_impl->get_buddy(ret_mem, szc);
	m_impl->mark_block_as_free(buddy, szc);
	freelist->push_front(buddy);

found:
	m_impl->mark_block_as_in_use(ret_mem, szc);
	return ret_mem;
}

template <size_t PageSize, size_t MinAllocSize>
bool BuddyManager<PageSize, MinAllocSize>::free(void *ptr, size_t size)
{
	auto szc = m_impl->get_sizeclass(size);
	auto freelist = m_impl->get_freelist(szc);

	m_impl->mark_block_as_free(ptr, szc);
	freelist->push_front(ptr);

	if (size == PageSize)
		return true;

	auto buddy = m_impl->get_buddy(ptr, szc);

	if (m_impl->block_is_free(buddy, szc))
	{
		freelist->remove(ptr);
		freelist->remove(buddy);

		return free(std::min(ptr, buddy), size * 2);
	}

	return false;
}

template <size_t PageSize, size_t MinAllocSize>
constexpr size_t BuddyManager<PageSize, MinAllocSize>::get_meta_size()
{
	return sizeof(BuddyManagerImpl);
}

size_t log_2(size_t n)
{
	assert(n > 0);
	return sizeof(n) * 8 - leading_zeroes(n) - 1;
}

template <size_t PageSize, size_t MinAllocSize>
BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::BuddyManagerImpl()
{
	memset(m_bitmap, 0, get_bitmap_size());
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Count BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_num_sizeclasses()
{
	return log_2(PageSize) - log_2(MinAllocSize) + 1;
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Size BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_bitmap_size()
{
	return ((PageSize / MinAllocSize) * 2) / 8;
}

template <size_t PageSize, size_t MinAllocSize>
constexpr SizeClass BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_sizeclass(
	Size size)
{
	assert(size >= MinAllocSize && size <= PageSize);

	return log_2(size) - log_2(MinAllocSize);
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Size BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_size(SizeClass szc)
{
	assert(szc < get_num_sizeclasses());

	return MinAllocSize * (1 << szc);
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Index BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_normalized_ptr(
	void *ptr)
{
	return reinterpret_cast<size_t>(ptr) % PageSize;
}

template <size_t PageSize, size_t MinAllocSize>
constexpr Index BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_bitmap_index(void *ptr,
																						 SizeClass szc)
{
	return (1 << (get_num_sizeclasses() - (szc + 1))) - 1 + get_normalized_ptr(ptr) / get_size(szc) - 1;
}

template <size_t PageSize, size_t MinAllocSize>
List &BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_freelist(SizeClass szc)
{
	return m_freelist[szc];
}

template <size_t PageSize, size_t MinAllocSize>
void *BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::get_buddy(void *ptr, SizeClass szc)
{
	auto ptr_start = reinterpret_cast<size_t>(ptr) / PageSize;
	auto n_ptr = reinterpret_cast<size_t>(ptr) % PageSize;

	return reinterpret_cast<void *>(ptr_start + n_ptr ^ 0x1);
}

template <size_t PageSize, size_t MinAllocSize>
void BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::mark_block_as_free(void *ptr,
																				SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	assert(!block_is_free(ptr, szc));

	m_bitmap[bitmap_byte] &= ~(1 << bitmap_bit);
}

template <size_t PageSize, size_t MinAllocSize>
void BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::mark_block_as_in_use(void *ptr,
																				  SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	assert(block_is_free(ptr, szc));

	m_bitmap[bitmap_byte] |= (1 << bitmap_bit);
}

template <size_t PageSize, size_t MinAllocSize>
bool BuddyManager<PageSize, MinAllocSize>::BuddyManagerImpl::block_is_free(void *ptr, SizeClass szc)
{
	auto bitmap_index = get_bitmap_index(ptr, szc);
	auto bitmap_byte = bitmap_index / 8;
	auto bitmap_bit = bitmap_index % 8;

	return m_bitmap[bitmap_byte] & (1 << bitmap_bit);
}
