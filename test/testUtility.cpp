/**
 * File: /testUtility.cpp
 * Project: test
 * Created Date: Wednesday, October 18th 2017, 8:15:32 am
 * Author: Harikrishnan
 */


#include "Utility.h"
#include "catch.hpp"

TEST_CASE("Factorials are computed", "[factorial]")
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
