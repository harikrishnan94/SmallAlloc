/**
 * File: /testHeap.cpp
 * Project: test
 * Created Date: Friday, November 3rd 2017, 3:44:41 am
 * Author: Harikrishnan
 */



#include "Heap.h"
#include "test/catch.hpp"
#include "test/testBase.h"

#include <cstdlib>
#include <random>
#include <iostream>
#include <unordered_set>

using random_gen = std::ranlux24_base;

TEST_CASE("HeapTest", "[allocator]")
{
	using namespace std;

	constexpr uint32_t AllocSize = 64;
	constexpr size_t AllocLimit = 64LL * 1024 * 1024 * 1024;

	std::random_device r;
	std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
	random_gen rand_op(seed);
	unordered_set<void *> ptr_set;

	SmallAlloc::Heap heap(AllocLimit);

	constexpr auto Low = 1, High = 100;
	std::uniform_int_distribution<int> uniform_dist(Low, High);

	enum
	{
		ALLOC,
		FREE,
		REMOTE_FREE
	};

	auto alloc_type = [](auto val)
	{
		if (val > 60)
			return ALLOC;

		if (val < 30)
			return FREE;

		return REMOTE_FREE;
	};
	auto gen_op = [&rand_op, &uniform_dist]()
	{
		return uniform_dist(rand_op);
	};

	constexpr auto MinAllocSize = 40, MaxAllocSize = 8144;
	random_gen rand_size(seed);
	std::uniform_int_distribution<int> size_dist(MinAllocSize, MaxAllocSize);
	auto gen_size = [&rand_size, &size_dist]()
	{
		return size_dist(rand_size);
	};

	int testIterations = 100 * 1000;

	for (int i = 0; i < testIterations; i++)
	{
		switch (alloc_type(gen_op()))
		{
			case ALLOC:
			{
				auto alloc_size = gen_size();
				auto mem = heap.alloc(alloc_size);

				if (mem)
				{
					memset(mem, 0x7F, alloc_size);
					*reinterpret_cast<size_t *>(mem) = alloc_size;
				}

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
					auto alloc_size = *reinterpret_cast<size_t *>(mem);
					ptr_set.erase(mem);
					heap.free(mem, alloc_size);
				}

				break;
			}

			case REMOTE_FREE:
			{
				auto iter = ptr_set.begin();

				if (iter != ptr_set.end())
				{
					auto mem = *iter;
					auto alloc_size = *reinterpret_cast<size_t *>(mem);
					ptr_set.erase(mem);
					heap.remote_free(mem, alloc_size);
				}
			}
		}
	}

	for (auto mem : ptr_set)
		heap.free(mem, *reinterpret_cast<size_t *>(mem));
}