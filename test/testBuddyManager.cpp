/**
 * File: /testBuddyManager.cpp
 * Project: test
 * Created Date: Friday, October 20th 2017, 2:11:35 pm
 * Author: Harikrishnan
 */


#include "BuddyManager.h"
#include "catch.hpp"

#include <memory>
#include <cstdlib>
#include <unordered_map>
#include <list>
#include <iostream>


TEST_CASE("BuddyManager Test", "[allocator]")
{
	using namespace std;

	constexpr auto BuddyPageSize = 16 * 1024;
	constexpr auto MinAllocSize = 16;
	auto buddy_managed_chunk = new char[BuddyPageSize];
	SmallAlloc::BuddyManager<BuddyPageSize, MinAllocSize> buddy_manager(buddy_managed_chunk);

	unordered_map<void *, size_t> ptr_set;

	auto do_alloc = [&ptr_set, &buddy_manager](auto & alloc_size_vector)
	{
		for (auto alloc_size : alloc_size_vector)
		{
			auto mem = buddy_manager.alloc(alloc_size);

			REQUIRE(ptr_set.count(mem) == 0);

			if (mem)
				ptr_set.insert({mem, alloc_size});
		}
	};

	auto do_free = [&ptr_set, &buddy_manager](auto free_size = 0)
	{
		bool reclaim = false;
		vector<pair<void *, size_t>> reclaim_ptrs{};

		for (auto &ptr_size_p : ptr_set)
		{
			if (free_size == 0 || free_size == ptr_size_p.second)
			{
				reclaim = buddy_manager.free(ptr_size_p.first, ptr_size_p.second);
				reclaim_ptrs.push_back({ptr_size_p.first, ptr_size_p.second});

				if (reclaim)
				{
					REQUIRE(ptr_set.size() == reclaim_ptrs.size());
					break;
				}
			}
		}

		for (auto &ptr_size_p : reclaim_ptrs)
			ptr_set.erase(ptr_size_p.first);

		return reclaim;
	};

	vector<size_t> alloc_size_vector{};

	alloc_size_vector.push_back(MinAllocSize);
	alloc_size_vector.push_back(MinAllocSize * 2);
	alloc_size_vector.push_back(MinAllocSize * 4);
	alloc_size_vector.push_back(MinAllocSize * 8);
	alloc_size_vector.push_back(MinAllocSize * 16);
	alloc_size_vector.push_back(BuddyPageSize / 4);

	do_alloc(alloc_size_vector);
	REQUIRE(do_free(0) == true);

	alloc_size_vector.clear();
	alloc_size_vector.push_back(BuddyPageSize / 2);
	alloc_size_vector.push_back(BuddyPageSize / 4);
	alloc_size_vector.push_back(BuddyPageSize / 4);
	do_alloc(alloc_size_vector);
	REQUIRE(do_free(BuddyPageSize / 2) == false);

	alloc_size_vector.clear();

	for (int i = 0; i < BuddyPageSize / MinAllocSize; i++)
		alloc_size_vector.push_back(MinAllocSize);

	do_alloc(alloc_size_vector);
	REQUIRE(do_free(0) == true);
}