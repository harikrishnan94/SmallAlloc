/**
 * File: /testUtility.cpp
 * Project: test
 * Created Date: Wednesday, October 18th 2017, 8:15:32 am
 * Author: Harikrishnan
 */


#include "Utility.h"
#include "catch.hpp"

#include <emmintrin.h>
#include <thread>
#include <memory>

using namespace SmallAlloc;

TEST_CASE("Freelist Test", "[utility]")
{
	struct FLNode : public FreeList::Node
	{
		long val;

		FLNode() : val(0)
		{}

		FLNode(long v) : val(v)
		{}
	};

	FreeList fl;

	FLNode n1{1};
	FLNode n2{2};
	FLNode n3{3};

	fl.push(&n1);
	fl.push(&n2);
	fl.push(&n3);

	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 3);
	REQUIRE(fl.empty() == false);
	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 2);

	fl.push(&n3);
	fl.push(&n2);

	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 2);
	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 3);
	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 1);
	REQUIRE(fl.empty() == true);

	FLNode n4{4};
	FLNode n5{5};

	fl.push(&n4);
	fl.push(&n5);

	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 5);
	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 4);
	REQUIRE(fl.empty() == true);
}

TEST_CASE("List Test", "[utility]")
{
	struct ListNode : public List::Node
	{
		long val;

		ListNode() : val(0)
		{}

		ListNode(long v) : val(v)
		{}
	};

	List list;

	ListNode n1{1};
	ListNode n2{2};
	ListNode n3{3};

	list.push_front(&n1);
	REQUIRE(static_cast<ListNode *>(list.front())->val == 1);
	REQUIRE(static_cast<ListNode *>(list.back())->val == 1);

	list.push_front(&n2);
	REQUIRE(static_cast<ListNode *>(list.front())->val == 2);
	REQUIRE(static_cast<ListNode *>(list.back())->val == 1);

	list.push_back(&n3);
	REQUIRE(static_cast<ListNode *>(list.front())->val == 2);
	REQUIRE(static_cast<ListNode *>(list.back())->val == 3);
	REQUIRE(list.empty() == false);

	REQUIRE(static_cast<ListNode *>(list.pop_front())->val == 2);
	REQUIRE(static_cast<ListNode *>(list.pop_back())->val == 3);
	REQUIRE(static_cast<ListNode *>(list.pop_back())->val == 1);
	REQUIRE(list.empty() == true);

	ListNode n4{4};
	ListNode n5{5};

	list.push_back(&n4);
	list.push_back(&n5);
	list.move_front(&n5);
	REQUIRE(static_cast<ListNode *>(list.front())->val == 5);
	list.move_front(&n4);
	REQUIRE(static_cast<ListNode *>(list.front())->val == 4);
	list.move_back(&n4);
	REQUIRE(static_cast<ListNode *>(list.back())->val == 4);
	list.move_back(&n5);
	REQUIRE(static_cast<ListNode *>(list.back())->val == 5);
}

TEST_CASE("Forward List Test", "[utility]")
{
	struct ListNode : public ForwardList::Node
	{
		long val;

		ListNode() : val(0)
		{}

		ListNode(long v) : val(v)
		{}
	};

	ForwardList list;

	ListNode n1{1};
	ListNode n2{2};
	ListNode n3{3};

	list.push(&n1);
	list.push(&n2);
	list.push(&n3);
	REQUIRE(!list.empty());

	REQUIRE(static_cast<ListNode *>(list.pop())->val == 3);
	REQUIRE(static_cast<ListNode *>(list.pop())->val == 2);
	REQUIRE(static_cast<ListNode *>(list.pop())->val == 1);
	REQUIRE(list.empty());

	ListNode n4{4};
	ListNode n5{5};

	list.push(&n4);
	list.push(&n5);
	REQUIRE(!list.empty());
	list.remove(&n4);
	REQUIRE(!list.empty());
	REQUIRE(static_cast<ListNode *>(list.pop())->val == 5);
	REQUIRE(list.empty());
}

TEST_CASE("FreelistAtomic Test", "[utility]")
{
	using namespace SmallAlloc;

	struct FLNode : public FreeListAtomic::Node
	{
		long val;

		FLNode() : val(0)
		{}

		FLNode(long v) : val(v)
		{}
	};

	FreeListAtomic fl;

	FLNode n1{1};
	FLNode n2{2};
	FLNode n3{3};

	fl.push(&n1);
	fl.push(&n2);
	fl.push(&n3);

	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 3);
	REQUIRE(fl.empty() == false);
	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 2);

	fl.push(&n3);
	fl.push(&n2);

	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 2);
	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 3);
	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 1);
	REQUIRE(fl.empty() == true);

	FLNode n4{4};
	FLNode n5{5};

	fl.push(&n4);
	fl.push(&n5);

	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 5);
	REQUIRE(static_cast<FLNode *>(fl.pop())->val == 4);
	REQUIRE(fl.empty() == true);

	constexpr int MAX_THREADS = 8;
	std::vector<std::thread> workers;
	std::atomic_bool quit(0);
	uint64_t popcount = 0;
	uint32_t node_count = 100 * 1000;
	uint64_t expected_popcount = uint64_t{node_count} * (MAX_THREADS - 1);

	std::unique_ptr<std::vector<FLNode>[]> node_vectors = std::make_unique<std::vector<FLNode>[]>
														  (MAX_THREADS - 1);

	auto thread_safety_test_pusher = [&](std::vector<FLNode>& nodes)
	{
		for (FLNode &node : nodes)
			fl.push(&node);
	};

	auto thread_safety_test_popper = [&]()
	{
		popcount = 0;

		while (!quit.load())
		{
			if (fl.pop())
			{
				popcount++;
			}
			else
			{
				for (int i = 0; i < 100; i++)
					_mm_pause();
			}
		}

		while (fl.pop())
			popcount++;
	};

	for (int i = 0; i < MAX_THREADS - 1; i++)
	{
		auto &nodes = node_vectors[i];
		nodes.reserve(node_count);

		for (uint32_t j = 0; j < node_count; j++)
			nodes.push_back(FLNode(j * long(i)));
	}

	for (int i = 0; i < MAX_THREADS; i++)
	{
		if (i == MAX_THREADS - 1)
		{
			workers.push_back(std::thread(thread_safety_test_popper));
		}
		else
		{
			workers.push_back(std::thread(thread_safety_test_pusher, std::ref(node_vectors[i])));
		}
	}

	for (int i = 0; i < MAX_THREADS - 1; i++)
		workers[i].join();

	quit.store(true);
	workers[MAX_THREADS - 1].join();

	REQUIRE(popcount == expected_popcount);
}
