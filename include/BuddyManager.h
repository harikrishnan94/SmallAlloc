#ifndef BUDDY_MANAGER_H
#define BUDDY_MANAGER_H

#include <cstddef>

namespace SmallAlloc
{

template <size_t PageSize, size_t MinAllocSize>
class BuddyManager
{
public:
	BuddyManager(void *meta, void *chunk);

	static constexpr size_t get_meta_size();

	void *alloc(size_t size);
	bool free(void *ptr, size_t size);

private:

	struct BuddyManagerImpl;

	BuddyManagerImpl *m_impl;
	const void *m_managed_chunk;
};

}

#endif /* BUDDY_MANAGER_H */