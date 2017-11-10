/**
 * File: /testBuddyManager.cpp
 * Project: test
 * Created Date: Friday, October 20th 2017, 2:11:35 pm
 * Author: Harikrishnan
 */


#include "BuddyManager/StandAloneBuddyManager.h"
#include "BuddyManager/BuddyManager.h"
#include "test/catch.hpp"
#include "test/testBase.h"

#include <memory>
#include <cstdlib>
#include <random>
#include <unordered_map>
#include <list>
#include <iostream>


TEST_CASE("StandAloneBuddyManager Test", "[allocator]")
{
	using namespace std;

	constexpr auto BuddyPageSize = 16 * 1024;
	constexpr auto MinAllocSize = 16;
	auto buddy_managed_chunk = new char[BuddyPageSize];
	using BuddyManager = SmallAlloc::BuddyManager::StandAloneBuddyManager<BuddyPageSize, MinAllocSize>;
	BuddyManager buddy_manager(buddy_managed_chunk);

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

using random_gen = std::ranlux24_base;

static void randomize_mem(random_gen &rand, void *ptr, size_t size)
{
	auto mem = reinterpret_cast<size_t *>(ptr);

	for (size_t i = 0; i < size / sizeof(size_t); i++)
		mem[i] = rand();
}

TEST_CASE("BuddyManager Test", "[allocator]")
{
	using namespace std;

	using BuddyManager = SmallAlloc::BuddyManager::BuddyManager;
	constexpr auto BuddyManagerAllocLimit = 28 * 1024 * 1024;
	constexpr auto AllocLimit = BuddyManagerAllocLimit - 2 * 1024 * 1024;
	std::random_device r;
	std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
	random_gen rand_mem(seed);

	BuddyManager buddy_manager(BuddyManagerAllocLimit, [&](auto align, auto size)
	{
		auto ptr = test_aligned_alloc(align, size);

		if (ptr)
			randomize_mem(rand_mem, ptr, size);

		return ptr;
	}, [](void *ptr,
		  auto size)
	{
		test_aligned_free(ptr);
	});

	auto BuddyPageSize = buddy_manager.get_page_size();
	auto MinAllocSize = buddy_manager.get_min_alloc_size();
	unordered_map<void *, size_t> ptr_set;

	auto do_alloc = [&ptr_set, &buddy_manager, &rand_mem](auto & alloc_size_vector)
	{
		int i = 0;

		for (auto alloc_size : alloc_size_vector)
		{
			auto mem = buddy_manager.alloc(alloc_size);

			if (!mem)
				(void) mem;

			REQUIRE(mem != nullptr);
			REQUIRE(ptr_set.count(mem) == 0);

			if (mem)
			{
				ptr_set.insert({mem, alloc_size});
				randomize_mem(rand_mem, mem, alloc_size);
			}

			i++;
		}
	};

	auto do_free = [&ptr_set, &buddy_manager](auto free_size = 0)
	{
		vector<pair<void *, size_t>> reclaim_ptrs{};

		for (auto &ptr_size_p : ptr_set)
		{
			if (free_size == 0 || free_size == ptr_size_p.second)
			{
				buddy_manager.free(ptr_size_p.first, ptr_size_p.second);
				reclaim_ptrs.push_back({ptr_size_p.first, ptr_size_p.second});
			}
		}

		for (auto &ptr_size_p : reclaim_ptrs)
			ptr_set.erase(ptr_size_p.first);
	};

	vector<size_t> alloc_size_vector{};

	alloc_size_vector.push_back(MinAllocSize);
	alloc_size_vector.push_back(MinAllocSize * 2);
	alloc_size_vector.push_back(MinAllocSize * 4);
	alloc_size_vector.push_back(MinAllocSize * 8);
	alloc_size_vector.push_back(MinAllocSize * 16);
	alloc_size_vector.push_back(BuddyPageSize / 4);

	buddy_manager.free(nullptr, MinAllocSize);
	buddy_manager.free(static_cast<void *>(&buddy_manager), BuddyPageSize + 1);
	buddy_manager.alloc(BuddyPageSize + 1);

	do_alloc(alloc_size_vector);
	do_free(0);

	alloc_size_vector.clear();
	alloc_size_vector.push_back(BuddyPageSize / 2);
	alloc_size_vector.push_back(BuddyPageSize / 4);
	alloc_size_vector.push_back(BuddyPageSize / 4);
	do_alloc(alloc_size_vector);
	do_free(BuddyPageSize / 2);

	alloc_size_vector.clear();

	for (int i = 0; i < AllocLimit / MinAllocSize; i++)
		alloc_size_vector.push_back(MinAllocSize);

	do_alloc(alloc_size_vector);
	do_free(0);

	alloc_size_vector.clear();
	alloc_size_vector.push_back(BuddyPageSize / 2);
	alloc_size_vector.push_back(BuddyPageSize / 4);
	alloc_size_vector.push_back(BuddyPageSize / 4);
	do_alloc(alloc_size_vector);

	BuddyManager dummy_buddy_manager1(BuddyManagerAllocLimit, [](auto align, auto size)
	{
		return nullptr;
	}, [](void *ptr,
		  auto size) {});

	REQUIRE(dummy_buddy_manager1.alloc(MinAllocSize) == nullptr);

	int i = 0;
	BuddyManager dummy_buddy_manager2(BuddyManagerAllocLimit, [&i](auto align, auto size)
	{
		void *ptr = nullptr;

		if (i == 0)
		{
			i++;
			return test_aligned_alloc(align, size);
		}

		return ptr;
	}, [](void *ptr,
		  auto size) {});

	REQUIRE(dummy_buddy_manager2.alloc(MinAllocSize) == nullptr);
}