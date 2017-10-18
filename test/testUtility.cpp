/**
 * File: /testUtility.cpp
 * Project: test
 * Created Date: Wednesday, October 18th 2017, 8:15:32 am
 * Author: Harikrishnan
 */


#include "Utility.h"
#include "catch.hpp"

TEST_CASE("Freelist Test", "[utility]")
{
	using namespace SmallAlloc;

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
	using namespace SmallAlloc;

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