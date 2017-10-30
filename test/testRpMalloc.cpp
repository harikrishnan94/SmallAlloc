/**
 * File: /testRpMalloc.cpp
 * Project: test
 * Created Date: Monday, October 30th 2017, 1:40:39 pm
 * Author: Harikrishnan
 */


#include "BuddyManager/BuddyManager.h"
#include "SlabAllocator.h"
#include "test/catch.hpp"
#include "rpmalloc/rpmalloc.h"

#include <cstdlib>
#include <random>
#include <iostream>
#include <unordered_set>

TEST_CASE("RpMallocTest", "[allocator]")
{
	using namespace std;
	using random_gen = ranlux24_base;
	using namespace SmallAlloc::SlabAllocator;
	using namespace SmallAlloc::BuddyManager;

	rpmalloc_initialize();
	rpmalloc_thread_initialize();

	constexpr auto SlabAllocSize = 128;
	constexpr auto SlabPageSize = 4 * 1024;

	std::random_device r;
	std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
	random_gen rand_op(seed);
	unordered_set<void *> ptr_set;

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
			auto mem = rpmalloc(SlabAllocSize);

			if (mem == nullptr)
				continue;

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
				rpfree(mem);
			}
		}
	}

	for (auto ptr : ptr_set)
		rpfree(ptr);

	rpmalloc_thread_finalize();
	rpmalloc_finalize();
}