/**
 * File: /PointerHashMap.h
 * Project: Utility
 * Created Date: Sunday, October 22nd 2017, 10:19:40 am
 * Author: Harikrishnan
 */


#ifndef POINTERHASHMAP_H
#define POINTERHASHMAP_H

#include <memory>
#include <cstddef>
#include <string>
#include <iterator>

namespace SmallAlloc
{
namespace utility
{

class PointerHashMap
{
public:
	using pointer_t = void *;
	using index_t = uint64_t;
	using count_t = uint64_t;

	constexpr static double DEFAULT_LOAD_FACTOR = 0.75;
	constexpr static uint64_t DEFAULT_NUM_BUCKETS = 8;

	PointerHashMap();
	PointerHashMap(double load_factor, count_t num_buckets);

	bool insert(pointer_t key, pointer_t val);
	pointer_t find(pointer_t key);
	bool erase(pointer_t key);
	count_t size();
	std::string dump();

	using Cell = std::pair<pointer_t, pointer_t>;

	using iterator = Cell *;
	using const_iterator = const Cell *;

	iterator begin()
	{
		return &m_buckets[0];
	}

	iterator end()
	{
		return &m_buckets[m_num_buckets];
	}

private:

	std::unique_ptr<Cell[]> m_buckets;
	count_t m_num_buckets;
	uint64_t m_element_count;
	double m_load_factor;

	void resize();
	index_t get_ideal_bucket(pointer_t key);
	index_t get_next_bucket(index_t bucket);
	index_t get_bucket(pointer_t key);
	bool is_between(index_t bucket, index_t start_bucket, index_t end_bucket);
};

}
}

#endif /* POINTERHASHMAP_H */
