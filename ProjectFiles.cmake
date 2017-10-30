# Copyright (c) <year> <author> (<email>)
# Distributed under the MIT License.
# See accompanying file LICENSE.md or copy at http://opensource.org/licenses/MIT

# Set project source files.
set(SRC
  "${SRC_PATH}/PointerHashMap.cpp"
  "${SRC_PATH}/SmallAlloc.cpp"
  "${SRC_PATH}/Heap.cpp"
  "${SRC_PATH}/BuddyManager.cpp"
  "${SRC_PATH}/SlabAllocator.cpp"
  "${SRC_PATH}/rpmalloc/rpmalloc.c"
)

# Set project main file.
# Not Required
# set(MAIN_SRC
#   "${SRC_PATH}/main.cpp"
# )

# Set project test source files.
set(TEST_SRC
  "${TEST_SRC_PATH}/testBase.cpp"
  "${TEST_SRC_PATH}/testUtility.cpp"
  "${TEST_SRC_PATH}/testBuddyManager.cpp"
  "${TEST_SRC_PATH}/testSlabAllocator.cpp"
)
