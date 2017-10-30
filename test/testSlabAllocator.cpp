
#include "BuddyManager/BuddyManager.h"
#include "SlabAllocator.h"
#include "test/catch.hpp"

#include <cstdlib>
#include <random>
#include <iostream>
#include <unordered_set>

static void *aligned_alloc(size_t align, size_t size)
{
	void *ptr = nullptr;

	posix_memalign(&ptr, align, size);

	return ptr;
}

using random_gen = std::ranlux24_base;

TEST_CASE("SlabAllocatorTest", "[allocator]")
{
	using namespace std;
	using Size = SmallAlloc::Size;
	using namespace SmallAlloc::SlabAllocator;
	using namespace SmallAlloc::BuddyManager;

	constexpr auto SlabAllocSize = 64;
	constexpr auto SlabPageSize = 4 * 1024;

	std::random_device r;
	std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
	random_gen rand_op(seed);
	unordered_set<void *> ptr_set;

	auto free_page = [](void *page, Size size)
	{
		free(page);
	};

	BuddyManager bm{64 * 1024 * 1024 * 1024L, aligned_alloc, free_page};
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
	auto is_alloc = [](auto val)
	{
		return val > 30;
	};
	auto gen_op = [&rand_op, &uniform_dist]()
	{
		return uniform_dist(rand_op);
	};

	int testIterations = 100 * 1000;

	for (int i = 0; i < testIterations; i++)
	{
		if (is_alloc(gen_op()))
		{
			auto mem = slab.alloc();

			REQUIRE(mem != nullptr);
			REQUIRE(ptr_set.count(mem) == 0);
			ptr_set.insert(mem);
		}
		else
		{
			auto iter = ptr_set.begin();

			if (iter != ptr_set.end())
			{
				auto mem = *iter;
				ptr_set.erase(mem);
				slab.free(mem);
			}
		}
	}

	for (auto ptr : ptr_set)
		slab.free(ptr);

	REQUIRE(slab.size() == 0);
}