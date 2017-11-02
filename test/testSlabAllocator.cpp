
#include "BuddyManager/BuddyManager.h"
#include "SlabAllocator.h"
#include "test/catch.hpp"
#include "test/testBase.h"

#include <cstdlib>
#include <random>
#include <iostream>
#include <unordered_set>

using random_gen = std::ranlux24_base;

TEST_CASE("SlabAllocatorTest", "[allocator]")
{
	using namespace std;
	using Size = SmallAlloc::Size;
	using namespace SmallAlloc::SlabAllocator;
	using namespace SmallAlloc::BuddyManager;

	constexpr uint32_t SlabAllocSize = 64;
	constexpr uint32_t SlabPageSize = 4 * 1024;

	std::random_device r;
	std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
	random_gen rand_op(seed);
	unordered_set<void *> ptr_set;

	auto free_page = [](void *page, Size size)
	{
		test_aligned_free(page);
	};

	BuddyManager bm{64LL * 1024 * 1024 * 1024, test_aligned_alloc, free_page};
	auto buddy_alloc = [&bm](Size align, Size size)
	{
		void *mem = bm.alloc(size);
		REQUIRE(mem != nullptr);
		return std::make_pair(static_cast<void *>(&bm), mem);
	};
	auto buddy_free = [](void *extra, void *page, Size size)
	{
		auto bm = static_cast<BuddyManager *>(extra);
		bm->free(page, size);
	};

	SlabAllocator slab{SlabAllocSize, SlabPageSize, buddy_alloc, buddy_free};

	constexpr int Low = 1, High = 100;
	std::uniform_int_distribution<int> uniform_dist(Low, High);
	enum
	{
		ALLOC,
		FREE,
		REMOTE_FREE
	};

	auto alloc_type = [](auto val)
	{
		if (val > 30)
			return ALLOC;

		if (val < 15)
			return FREE;

		return REMOTE_FREE;
	};
	auto gen_op = [&rand_op, &uniform_dist]()
	{
		return uniform_dist(rand_op);
	};

	int testIterations = 100 * 1000;

	for (int i = 0; i < testIterations; i++)
	{
		switch (alloc_type(gen_op()))
		{
			case ALLOC:
			{
				auto mem = slab.alloc();

				REQUIRE(mem != nullptr);
				REQUIRE(ptr_set.count(mem) == 0);
				ptr_set.insert(mem);
				break;
			}

			case FREE:
			{
				auto iter = ptr_set.begin();

				if (iter != ptr_set.end())
				{
					auto mem = *iter;
					ptr_set.erase(mem);
					slab.free(mem);
				}

				break;
			}

			case REMOTE_FREE:
			{
				auto iter = ptr_set.begin();

				if (iter != ptr_set.end())
				{
					auto mem = *iter;
					ptr_set.erase(mem);
					slab.remote_free(mem);
				}
			}
		}
	}

	for (auto ptr : ptr_set)
		slab.free(ptr);

	slab.reclaim_remote_free();

	// REQUIRE(slab.size() == 0);

	SlabAllocator dummy{SlabAllocSize, SlabPageSize, [](Size, Size)
	{
		void *null = nullptr;
		return std::make_pair(null, null);
	}, [](void *, void *, Size) {}};

	REQUIRE(dummy.alloc() == nullptr);
	REQUIRE(dummy.size() == 0);
}