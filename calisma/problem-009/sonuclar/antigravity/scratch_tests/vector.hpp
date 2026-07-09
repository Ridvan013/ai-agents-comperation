#ifndef SJTU_VECTOR_HPP
#define SJTU_VECTOR_HPP

#include "exceptions.hpp"

#include <climits>
#include <cstddef>

namespace sjtu
{
/**
 * a data container like std::vector
 * store data in a successive memory and support random access.
 */
template<typename T>
class vector
{
public:
	/**
	 * you can see RandomAccessIterator at CppReference for help.
	 */
	class const_iterator;
	class iterator
	{
		friend class vector;
		friend class const_iterator;
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;
		using iterator_category = std::output_iterator_tag;

	private:
		vector<T>* vec;
		size_t index;

	public:
		iterator(vector<T>* v = nullptr, size_t idx = 0) : vec(v), index(idx) {}

		iterator operator+(const int &n) const
		{
			return iterator(vec, index + n);
		}
		iterator operator-(const int &n) const
		{
			return iterator(vec, index - n);
		}
		int operator-(const iterator &rhs) const
		{
			if (vec != rhs.vec) throw invalid_iterator();
			return index - rhs.index;
		}
		iterator& operator+=(const int &n)
		{
			index += n;
			return *this;
		}
		iterator& operator-=(const int &n)
		{
			index -= n;
			return *this;
		}
		iterator operator++(int) {
			iterator temp = *this;
			index++;
			return temp;
		}
		iterator& operator++() {
			index++;
			return *this;
		}
		iterator operator--(int) {
			iterator temp = *this;
			index--;
			return temp;
		}
		iterator& operator--() {
			index--;
			return *this;
		}
		T& operator*() const {
			return vec->data[index];
		}
		T* operator->() const {
			return &(vec->data[index]);
		}
		bool operator==(const iterator &rhs) const {
			return vec == rhs.vec && index == rhs.index;
		}
		bool operator==(const const_iterator &rhs) const {
			return vec == rhs.vec && index == rhs.index;
		}
		bool operator!=(const iterator &rhs) const {
			return !(*this == rhs);
		}
		bool operator!=(const const_iterator &rhs) const {
			return !(*this == rhs);
		}
	};
	
	class const_iterator
	{
		friend class vector;
		friend class iterator;
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;
		using iterator_category = std::output_iterator_tag;

	private:
		const vector<T>* vec;
		size_t index;
		
	public:
		const_iterator(const vector<T>* v = nullptr, size_t idx = 0) : vec(v), index(idx) {}
		const_iterator(const iterator &other) : vec(other.vec), index(other.index) {}

		const_iterator operator+(const int &n) const
		{
			return const_iterator(vec, index + n);
		}
		const_iterator operator-(const int &n) const
		{
			return const_iterator(vec, index - n);
		}
		int operator-(const const_iterator &rhs) const
		{
			if (vec != rhs.vec) throw invalid_iterator();
			return index - rhs.index;
		}
		const_iterator& operator+=(const int &n)
		{
			index += n;
			return *this;
		}
		const_iterator& operator-=(const int &n)
		{
			index -= n;
			return *this;
		}
		const_iterator operator++(int) {
			const_iterator temp = *this;
			index++;
			return temp;
		}
		const_iterator& operator++() {
			index++;
			return *this;
		}
		const_iterator operator--(int) {
			const_iterator temp = *this;
			index--;
			return temp;
		}
		const_iterator& operator--() {
			index--;
			return *this;
		}
		const T& operator*() const {
			return vec->data[index];
		}
		const T* operator->() const {
			return &(vec->data[index]);
		}
		bool operator==(const iterator &rhs) const {
			return vec == rhs.vec && index == rhs.index;
		}
		bool operator==(const const_iterator &rhs) const {
			return vec == rhs.vec && index == rhs.index;
		}
		bool operator!=(const iterator &rhs) const {
			return !(*this == rhs);
		}
		bool operator!=(const const_iterator &rhs) const {
			return !(*this == rhs);
		}
	};

private:
	T *data;
	size_t current_length;
	size_t max_capacity;

	void reserve(size_t new_cap) {
		if (new_cap <= max_capacity) return;
		T *new_data = reinterpret_cast<T*>(operator new(sizeof(T) * new_cap));
		size_t i = 0;
		try {
			for (; i < current_length; ++i) {
				new (new_data + i) T(data[i]);
			}
		} catch (...) {
			for (size_t j = 0; j < i; ++j) {
				new_data[j].~T();
			}
			operator delete(new_data);
			throw;
		}
		for (size_t j = 0; j < current_length; ++j) {
			data[j].~T();
		}
		if (data) operator delete(data);
		data = new_data;
		max_capacity = new_cap;
	}

public:
	vector() {
		data = nullptr;
		current_length = 0;
		max_capacity = 0;
	}
	vector(const vector &other) {
		max_capacity = other.max_capacity;
		current_length = other.current_length;
		if (max_capacity > 0) {
			data = reinterpret_cast<T*>(operator new(sizeof(T) * max_capacity));
			for (size_t i = 0; i < current_length; ++i) {
				new (data + i) T(other.data[i]);
			}
		} else {
			data = nullptr;
		}
	}
	~vector() {
		for (size_t i = 0; i < current_length; ++i) {
			data[i].~T();
		}
		if (data) {
			operator delete(data);
		}
	}
	vector &operator=(const vector &other) {
		if (this == &other) return *this;
		T *new_data = nullptr;
		if (other.max_capacity > 0) {
			new_data = reinterpret_cast<T*>(operator new(sizeof(T) * other.max_capacity));
			size_t i = 0;
			try {
				for (; i < other.current_length; ++i) {
					new (new_data + i) T(other.data[i]);
				}
			} catch (...) {
				for (size_t j = 0; j < i; ++j) {
					new_data[j].~T();
				}
				operator delete(new_data);
				throw;
			}
		}
		for (size_t i = 0; i < current_length; ++i) {
			data[i].~T();
		}
		if (data) {
			operator delete(data);
		}
		data = new_data;
		max_capacity = other.max_capacity;
		current_length = other.current_length;
		return *this;
	}
	
	T & at(const size_t &pos) {
		if (pos >= current_length) throw index_out_of_bound();
		return data[pos];
	}
	const T & at(const size_t &pos) const {
		if (pos >= current_length) throw index_out_of_bound();
		return data[pos];
	}
	T & operator[](const size_t &pos) {
		if (pos >= current_length) throw index_out_of_bound();
		return data[pos];
	}
	const T & operator[](const size_t &pos) const {
		if (pos >= current_length) throw index_out_of_bound();
		return data[pos];
	}
	const T & front() const {
		if (current_length == 0) throw container_is_empty();
		return data[0];
	}
	const T & back() const {
		if (current_length == 0) throw container_is_empty();
		return data[current_length - 1];
	}
	iterator begin() {
		return iterator(this, 0);
	}
	const_iterator begin() const {
		return const_iterator(this, 0);
	}
	const_iterator cbegin() const {
		return const_iterator(this, 0);
	}
	iterator end() {
		return iterator(this, current_length);
	}
	const_iterator end() const {
		return const_iterator(this, current_length);
	}
	const_iterator cend() const {
		return const_iterator(this, current_length);
	}
	bool empty() const {
		return current_length == 0;
	}
	size_t size() const {
		return current_length;
	}
	void clear() {
		for (size_t i = 0; i < current_length; ++i) {
			data[i].~T();
		}
		current_length = 0;
	}
	iterator insert(iterator pos, const T &value) {
		return insert(pos.index, value);
	}
	iterator insert(const size_t &ind, const T &value) {
		if (ind > current_length) throw index_out_of_bound();
		if (current_length == max_capacity) {
			reserve(max_capacity == 0 ? 1 : max_capacity * 2);
		}
		if (ind == current_length) {
			new (data + current_length) T(value);
		} else {
			new (data + current_length) T(data[current_length - 1]);
			for (size_t i = current_length - 1; i > ind; --i) {
				data[i] = data[i - 1];
			}
			data[ind] = value;
		}
		current_length++;
		return iterator(this, ind);
	}
	iterator erase(iterator pos) {
		return erase(pos.index);
	}
	iterator erase(const size_t &ind) {
		if (ind >= current_length) throw index_out_of_bound();
		for (size_t i = ind; i < current_length - 1; ++i) {
			data[i] = data[i + 1];
		}
		data[current_length - 1].~T();
		current_length--;
		return iterator(this, ind);
	}
	void push_back(const T &value) {
		if (current_length == max_capacity) {
			reserve(max_capacity == 0 ? 1 : max_capacity * 2);
		}
		new (data + current_length) T(value);
		current_length++;
	}
	void pop_back() {
		if (current_length == 0) throw container_is_empty();
		data[current_length - 1].~T();
		current_length--;
	}
};

}

#endif
