/**
 * File: /IList.h
 * Project: SmallAlloc
 * Created Date: Tuesday, October 17th 2017, 7:33:03 pm
 * Author: Harikrishnan
 */


#ifndef FREELIST_H
#define FREELIST_H

#include <cstddef>
#include <cassert>
#include <atomic>
#include <type_traits>
#include <emmintrin.h>

namespace SmallAlloc
{

namespace utility
{
class FreeList
{
public:

	struct Node
	{
		Node *next;

		Node *get_next()
		{
			return next;
		}
	};

	FreeList() : m_head(nullptr)
	{
		static_assert(std::is_pod<Node>::value == true, "FreeList::Node cannot laid as POD");
	}

	bool empty()
	{
		return m_head == nullptr;
	}

	Node *peek()
	{
		return m_head;
	}

	void push(Node *node)
	{
		node->next = m_head;
		m_head = node;
	}

	Node *pop()
	{
		auto head = m_head;

		if (head)
		{
			m_head = m_head->next;
			return head;
		}

		return nullptr;
	}

	Node *popAll()
	{
		auto head = m_head;

		if (head)
		{
			m_head = nullptr;
			return head;
		}

		return nullptr;
	}

private:
	Node *m_head;
};


class ForwardList
{
public:

	struct Node
	{
		Node *next;
		Node *prev;

		Node *get_next()
		{
			return next;
		}

		Node *get_prev()
		{
			return prev;
		}
	};

	ForwardList() : m_head(nullptr)
	{
		static_assert(std::is_pod<Node>::value == true, "ForwardList::Node cannot laid as POD");
	}

	bool empty()
	{
		return m_head == nullptr;
	}

	Node *peek()
	{
		return m_head;
	}

	void remove(Node *node)
	{
		if (m_head == node)
		{
			pop();
			return;
		}

		node->prev->next = node->next;

		if (node->next)
			node->next->prev = node->prev;
	}

	void push(Node *node)
	{
		node->prev = nullptr;
		node->next = m_head;

		if (m_head)
			node->next->prev = node;

		m_head = node;
	}

	Node *pop()
	{
		auto head = m_head;

		if (head)
		{
			m_head = m_head->next;

			if (m_head)
				m_head->prev = nullptr;

			return head;
		}

		return nullptr;
	}

	Node *popAll()
	{
		auto head = m_head;

		if (head)
		{
			m_head = nullptr;
			return head;
		}

		return nullptr;
	}

private:
	Node *m_head;
};


class FreeListAtomic
{
public:

	using Node = FreeList::Node;

	FreeListAtomic() : m_head(nullptr)
	{
		static_assert(std::is_pod<Node>::value == true, "FreelistAtomic::Node cannot laid as POD");
	}

	bool empty()
	{
		return m_head.load(std::memory_order_acquire) == nullptr;
	}

	Node *peek()
	{
		return m_head.load(std::memory_order_acquire);
	}

	void push(Node *node)
	{
		while (true)
		{
			auto head = m_head.load(std::memory_order_acquire);
			node->next = head;

			if (m_head.compare_exchange_weak(head, node, std::memory_order_release))
				break;

			_mm_pause();
		}
	}

	Node *pop()
	{
		while (true)
		{
			auto head = m_head.load(std::memory_order_acquire);

			if (head == nullptr)
				return nullptr;

			auto next = head->next;

			if (m_head.compare_exchange_weak(head, next, std::memory_order_release))
				return head;

			_mm_pause();
		}
	}

	Node *popAll()
	{
		while (true)
		{
			auto head = m_head.load(std::memory_order_acquire);

			if (head == nullptr)
				return nullptr;

			if (m_head.compare_exchange_weak(head, nullptr, std::memory_order_release))
				return head;

			_mm_pause();
		}
	}

private:
	std::atomic<Node *> m_head;
};


class List
{
public:

	struct Node
	{
		union
		{
			Node *prev;
			Node *last;
		};

		union
		{
			Node *next;
			Node *first;
		};

		Node *get_next()
		{
			return next;
		}

		Node *get_prev()
		{
			return prev;
		}
	};

	List() : m_head()
	{
		static_assert(std::is_pod<Node>::value == true, "List::Node cannot laid as POD");

		m_head.first = &m_head;
		m_head.last = &m_head;
	}

	bool empty()
	{
		return m_head.first == &m_head;
	}

	Node *front()
	{
		return m_head.first;
	}

	Node *back()
	{
		return m_head.last;
	}

	void push_back(Node *node)
	{
		node->prev = back();
		node->next = &m_head;
		node->prev->next = node;
		m_head.last = node;
	}

	void push_front(Node *node)
	{
		node->next = front();
		node->prev = &m_head;
		node->next->prev = node;
		m_head.first = node;
	}

	Node *pop_front()
	{
		auto head = front();
		remove(head);

		return head;
	}

	void remove(Node *node)
	{
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}

	Node *pop_back()
	{
		auto tail = back();
		remove(tail);

		return tail;
	}

	void move_front(Node *node)
	{
		remove(node);
		push_front(node);
	}

	void move_back(Node *node)
	{
		remove(node);
		push_back(node);
	}

private:
	Node m_head;
};

}
}

#endif /* FREELIST_H */
