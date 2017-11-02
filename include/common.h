/**
 * File: /include/common.h
 * Project: SmallAlloc
 * Created Date: Sunday, October 29th 2017, 4:49:25 pm
 * Author: Harikrishnan
 */


#ifndef COMMON_H
#define COMMON_H

#include <cstddef>
#include <jemalloc/jemalloc.h>

namespace SmallAlloc
{

using Size = size_t;
using SizeClass = size_t;
using Count = size_t;

}

#endif /* COMMON_H */
