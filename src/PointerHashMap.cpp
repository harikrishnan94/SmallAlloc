/**
 * File: /PointerHashMap.cpp
 * Project: src
 * Created Date: Sunday, October 22nd 2017, 10:41:08 am
 * Author: Harikrishnan
 */


#include "Utility/PointerHashMap.h"

#include <stdexcept>
#include <limits>
#include <cassert>

using namespace SmallAlloc::utility;

using pointer_t = PointerHashMap::pointer_t;
using index_t = PointerHashMap::index_t;
using count_t = PointerHashMap::count_t;


static index_t hash(pointer_t key)
{
	return std::hash<pointer_t> {}(key);
}

PointerHashMap::PointerHashMap() : PointerHashMap(DEFAULT_LOAD_FACTOR, DEFAULT_NUM_BUCKETS)
{}

PointerHashMap::PointerHashMap(double load_factor, count_t num_buckets)
	: m_buckets(std::make_unique<Cell[]>(num_buckets)), m_num_buckets(num_buckets),
	  m_element_count(0), m_load_factor(load_factor)
{
	if (m_load_factor > 0.99)
		throw std::invalid_argument("load_factor should be <= .99");

	std::memset(m_buckets.get(), 0, m_num_buckets * sizeof(Cell));
}

count_t PointerHashMap::size()
{
	return m_element_count;
}

index_t PointerHashMap::get_ideal_bucket(pointer_t key)
{
	return hash(key) & (m_num_buckets - 1);
}

index_t PointerHashMap::get_next_bucket(index_t bucket)
{
	return (bucket + 1) & (m_num_buckets - 1);
}

index_t PointerHashMap::get_bucket(pointer_t key)
{
	auto bucket = get_ideal_bucket(key);
	auto buckets = m_buckets.get();

	while (buckets[bucket].first && buckets[bucket].first != key)
		bucket = get_next_bucket(bucket);

	return bucket;
}

bool PointerHashMap::is_between(index_t bucket, index_t start_bucket, index_t end_bucket)
{
#define CIRCULAR_OFFSET(a, b) ((b) >= (a) ? (b) - (a) : m_num_buckets + ((b) - (a)))
	return CIRCULAR_OFFSET(start_bucket, bucket) < CIRCULAR_OFFSET(start_bucket, end_bucket);
#undef CIRCULAR_OFFSET
}

pointer_t PointerHashMap::find(pointer_t key)
{
	auto &bucket = m_buckets[get_bucket(key)];
	return key && bucket.first ? bucket.second : nullptr;
}

bool PointerHashMap::insert(pointer_t key, pointer_t val)
{
	if ((m_element_count + 1) > m_num_buckets * m_load_factor)
		resize();

	if (key == nullptr)
		return false;

	auto buckets = m_buckets.get();
	auto bucket = get_bucket(key);

	if (buckets[bucket].first != nullptr)
	{
		assert(buckets[bucket].first == key);
		return false;
	}

	m_element_count++;
	buckets[bucket] = {key, val};
	return true;
}

bool PointerHashMap::erase(pointer_t key)
{
	if (key == nullptr)
		return false;

	auto bucket = get_bucket(key);
	auto buckets = m_buckets.get();

	if (buckets[bucket].first != key)
		return false;

	for (auto neighbour = get_next_bucket(bucket);
			buckets[neighbour].first;
			neighbour = get_next_bucket(neighbour))
	{
		auto ideal = get_ideal_bucket(buckets[neighbour].first);

		if (is_between(bucket, ideal, neighbour))
		{
			std::swap(buckets[bucket], buckets[neighbour]);
			bucket = neighbour;
		}
	}

	buckets[bucket] = {nullptr,  nullptr};
	m_element_count--;
	return true;
}

void PointerHashMap::resize()
{
	auto old_buckets = std::move(m_buckets);
	auto old_num_buckets = m_num_buckets;

	if (m_num_buckets == std::numeric_limits<count_t>::max())
		throw "Cannot resize PointerHashMap: maximum size limit reached";

	m_element_count = 0;
	m_num_buckets *= 2;
	m_buckets = std::make_unique<Cell[]>(m_num_buckets);
	auto buckets = old_buckets.get();

	for (auto bucket = 0; bucket < old_num_buckets; bucket++)
		insert(buckets[bucket].first, buckets[bucket].second);
}

std::string PointerHashMap::dump()
{
	auto buckets = m_buckets.get();
	auto num_buckets = m_num_buckets;
	std::string str;

	for (auto bucket = 0; bucket < num_buckets; bucket++)
	{
		str.append(std::to_string(bucket));
		str.append(" --> ");
		str.append(std::to_string(reinterpret_cast<uintptr_t>(buckets[bucket].first)));
		str.append(", ");
		str.append(std::to_string(reinterpret_cast<uintptr_t>(buckets[bucket].second)));
		str.append("\n");
	}

	return str;
}