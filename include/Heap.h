#ifndef HEAP_H
#define HEAP_H

#include <memory>

namespace SmallAlloc
{

class Heap
{
public:
	explicit Heap(size_t alloc_limit = 0);
	~Heap();

	Heap(const Heap &heap_rhs) = delete;
	Heap(Heap &&heap_rhs);

	void *alloc(size_t size);
	void free(void *ptr, size_t ptr_size);

private:
	class HeapImpl;
	std::unique_ptr<HeapImpl> impl;
};

}
#endif /* HEAP_H */
