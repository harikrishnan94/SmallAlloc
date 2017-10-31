#define CATCH_CONFIG_MAIN

#include "test/catch.hpp"

void *aligned_alloc(size_t align, size_t size)
{
#ifdef _WIN32
	return _aligned_malloc(size, align);
#else
	void *ptr = nullptr;
	posix_memalign(&ptr, align, size);
	return ptr;
#endif // _WIN32
}

void aligned_free(void *ptr)
{
#ifdef _WIN32
	_aligned_free(ptr);
#else
	free(ptr);
#endif // _WIN32
}