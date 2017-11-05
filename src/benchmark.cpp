/**
 * File: /benchmark.cpp
 * Project: src
 * Created Date: Friday, November 3rd 2017, 4:59:45 am
 * Author: Harikrishnan
 */


#include "Heap.h"
#include "rpmalloc/rpmalloc.h"
#include "BenchMark.h"

#include <random>
#include <vector>
#include <unordered_set>
#include <chrono>
#include <ctime>
#include <dlfcn.h>
#include <sys/mman.h>

#define JEMALLOC(s)	malloc(s)
#define JEFREE(p)	free(p)

#define RPMALLOC(s)	rpmalloc(s)
#define RPFREE(p)	rpfree(p)

#define LIBCMALLOC(s)	sys_malloc(s)
#define LIBCFREE(p)		sys_free(p)

#define SMALLOC(s)	heap.alloc(s)
#define SFREE(p, s)	heap.free(p, s)

enum
{
	ALLOC,
	FREE,
	REMOTE_FREE
};

auto alloc_type(int val)
{
	if (val > 60)
		return ALLOC;

	return FREE;
};

auto gen_op(std::ranlux24_base &rand_op, std::uniform_int_distribution<int> &uniform_dist)
{
	return uniform_dist(rand_op);
};

auto gen_size(std::ranlux24_base &rand_size, std::uniform_int_distribution<int> &size_dist)
{
	return size_dist(rand_size);
};

enum AllocatorType
{
	SMALLOC_ALLOCATOR,
	JEMALLOC_ALLOCATOR,
	RPMALLOC_ALLOCATOR,
	LIBC_ALLOCATOR
};

#define MALLOC_TYPE(x)	reinterpret_cast<void *(*)(size_t)>(x)
#define FREE_TYPE(x)	reinterpret_cast<void (*)(void *)>(x)

static void BM_SMalloc(benchmark::State& state, AllocatorType allocator,
					   const std::vector<int> &op_vec, const std::vector<size_t> &alloc_size_vec,
					   const std::vector<int> &free_ind_vec, const std::vector<int> &unfreed_ind_vec)
{
	constexpr size_t AllocLimit = 64LL * 1024 * 1024 * 1024;
	SmallAlloc::Heap heap(AllocLimit);

	rpmalloc_initialize();
	rpmalloc_thread_initialize();

	void *(*sys_malloc)(size_t) = malloc;
	void (*sys_free)(void *) = free;

	void* handle = dlopen("libSystem.B.dylib", RTLD_LAZY);

	if (handle)
	{
		sys_malloc = MALLOC_TYPE(dlsym(handle, "malloc"));
		sys_free = FREE_TYPE(dlsym(handle, "free"));
		dlclose(handle);
	}

	if (!handle || !sys_malloc || !sys_free)
	{
		sys_malloc = malloc;
		sys_free = free;
	}

	while (sys_malloc && sys_malloc == malloc)
	{
		sys_malloc = reinterpret_cast<void *(*)(size_t)>(dlsym(RTLD_NEXT, "malloc"));
		sys_free = reinterpret_cast<void (*)(void *)>(dlsym(RTLD_NEXT, "free"));
	}

	if (sys_malloc == nullptr)
	{
		sys_malloc = malloc;
		sys_free = free;
	}

	auto mem_alloc = [&](auto size, auto allocator)
	{
		void *mem;

		switch (allocator)
		{
			case SMALLOC_ALLOCATOR:
				mem = SMALLOC(size);
				break;

			case JEMALLOC_ALLOCATOR:
				mem = JEMALLOC(size);
				break;

			case RPMALLOC_ALLOCATOR:
				mem = RPMALLOC(size);
				break;

			case LIBC_ALLOCATOR:
				mem = LIBCMALLOC(size);
				break;

			default:
				return mem;
		}

		return mem;
	};
	auto mem_dealloc = [&](auto ptr, auto size, auto allocator)
	{
		switch (allocator)
		{
			case SMALLOC_ALLOCATOR:
				return SFREE(ptr, size);

			case JEMALLOC_ALLOCATOR:
				return JEFREE(ptr);

			case RPMALLOC_ALLOCATOR:
				return RPFREE(ptr);

			case LIBC_ALLOCATOR:
				return LIBCFREE(ptr);
		}
	};

	auto alloc_count = 0;
	auto free_count = 0;
	auto i = 0;

	std::vector<void *> alloc_ptrs;
	alloc_ptrs.reserve(alloc_size_vec.size());

	for (auto _ : state)
	{
		if (i == op_vec.size())
		{
			i = 0;

			for (auto ind : unfreed_ind_vec)
				mem_dealloc(alloc_ptrs[ind], alloc_size_vec[ind], allocator);

			alloc_ptrs.clear();
			free_count = alloc_count = 0;
		}

		switch (op_vec[i++])
		{
			case ALLOC:
			{
				alloc_ptrs.push_back(mem_alloc(alloc_size_vec[alloc_count++], allocator));
				break;
			}

			case FREE:
			{
				auto ind = free_ind_vec[free_count++];
				mem_dealloc(alloc_ptrs[ind], alloc_size_vec[ind], allocator);
				break;
			}

			default:
				break;
		}
	}

	rpmalloc_thread_finalize();
	rpmalloc_finalize();
}

static void generate_bench_args(std::vector<int> &op_vec, std::vector<size_t> &alloc_size_vec,
								std::vector<int> &free_ind_vec, std::vector<int> &unfreed_ind_vec,
								int num_operations)
{
	std::random_device r;
	std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
	std::mt19937_64 rnd(seed);

	constexpr auto MinAllocSize = 40, MaxAllocSize = 8144;

	std::uniform_int_distribution<int> op_dist{1, 100};
	std::uniform_int_distribution<int> op_size{MinAllocSize, MaxAllocSize};
	std::uniform_int_distribution<int> op_ind;
	std::unordered_set<int> free_ind_set;

	op_vec.reserve(num_operations);

	auto gen_op = [&op_dist, &rnd]()
	{
		auto op_val = op_dist(rnd);

		if (op_val > 30)
			return ALLOC;

		return FREE;
	};

	for (int i = 0; i < num_operations; i++)
	{
		if (gen_op() == ALLOC)
		{
			auto alloc_size = op_size(rnd);
			op_vec.emplace_back(ALLOC);
			alloc_size_vec.emplace_back(alloc_size);
		}
		else
		{
			if (alloc_size_vec.size() <= free_ind_vec.size())
			{
				i--;
				continue;
			}

			while (true)
			{
				auto free_ind = op_ind(rnd) % alloc_size_vec.size();

				if (!free_ind_set.count(free_ind))
				{
					free_ind_vec.emplace_back(free_ind);
					free_ind_set.insert(free_ind);
					break;
				}
			}

			op_vec.push_back(FREE);
		}
	}

	unfreed_ind_vec.reserve(alloc_size_vec.size() - free_ind_vec.size());

	for (auto ind = 0; ind < alloc_size_vec.size(); ind++)
	{
		if (free_ind_set.count(ind) == 0)
			unfreed_ind_vec.push_back(ind);
	}
}

int main(int argc, char** argv)
{
	std::vector<int> op_vec;
	std::vector<size_t> alloc_size_vec;
	std::vector<int> free_ind_vec;
	std::vector<int> unfreed_ind_vec;

	generate_bench_args(op_vec, alloc_size_vec, free_ind_vec, unfreed_ind_vec, 1 * 1024 * 1024);

	benchmark::RegisterBenchmark("SmallAllocTest", BM_SMalloc, SMALLOC_ALLOCATOR, op_vec,
								 alloc_size_vec, free_ind_vec, unfreed_ind_vec);
	// benchmark::RegisterBenchmark("LibcMallocTest", BM_SMalloc, LIBC_ALLOCATOR, op_vec,
	// 							 alloc_size_vec, free_ind_vec, unfreed_ind_vec);
	// benchmark::RegisterBenchmark("RpMallocTest", BM_SMalloc, RPMALLOC_ALLOCATOR, op_vec,
	// 							 alloc_size_vec, free_ind_vec, unfreed_ind_vec);
	// benchmark::RegisterBenchmark("JeMallocTest", BM_SMalloc, JEMALLOC_ALLOCATOR, op_vec,
	// 							 alloc_size_vec, free_ind_vec, unfreed_ind_vec);

	benchmark::Initialize(&argc, argv);
	benchmark::RunSpecifiedBenchmarks();
}
