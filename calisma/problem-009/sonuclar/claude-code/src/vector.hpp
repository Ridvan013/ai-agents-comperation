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
 *
 * Memory is managed manually with raw storage (operator new / operator delete)
 * and placement new, so that element types without a default constructor are
 * supported and no spurious constructions/destructions happen.
 */
template<typename T>
class vector
{
private:
	T *data_;       // raw storage holding the sz_ constructed elements
	size_t sz_;     // number of constructed elements
	size_t cap_;    // number of elements the storage can hold

	// allocate raw, uninitialized storage for n elements
	static T *allocate(size_t n)
	{
		if (n == 0) return nullptr;
		return static_cast<T *>(::operator new(n * sizeof(T)));
	}

	// free raw storage (does not run destructors)
	static void deallocate(T *p)
	{
		::operator delete(p);
	}

	// grow storage to hold at least newCap elements, preserving contents.
	void reserve(size_t newCap)
	{
		if (newCap <= cap_) return;
		T *nd = allocate(newCap);
		// copy-construct existing elements into the new buffer, then destroy
		// the old ones. If a copy throws, clean up what we already built.
		size_t i = 0;
		try {
			for (; i < sz_; ++i) {
				new (nd + i) T(data_[i]);
			}
		} catch (...) {
			for (size_t j = 0; j < i; ++j) nd[j].~T();
			deallocate(nd);
			throw;
		}
		for (size_t j = 0; j < sz_; ++j) data_[j].~T();
		deallocate(data_);
		data_ = nd;
		cap_ = newCap;
	}

	// capacity to use when growth is required
	size_t nextCapacity() const
	{
		return cap_ == 0 ? 1 : cap_ * 2;
	}

public:
	/**
	 * a random-access iterator over a vector.
	 */
	class const_iterator;
	class iterator
	{
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;
		using iterator_category = std::output_iterator_tag;

		friend class vector;
		friend class const_iterator;

	private:
		T *ptr_;                    // element currently pointed to
		const vector *container_;   // owning container (identity)

	public:
		iterator() : ptr_(nullptr), container_(nullptr) {}
		iterator(T *ptr, const vector *container) : ptr_(ptr), container_(container) {}

		/**
		 * return a new iterator which points n elements forward,
		 * as well as operator-
		 */
		iterator operator+(const int &n) const
		{
			return iterator(ptr_ + n, container_);
		}
		iterator operator-(const int &n) const
		{
			return iterator(ptr_ - n, container_);
		}
		// return the distance between two iterators,
		// if these two iterators point to different vectors, throw invalid_iterator.
		int operator-(const iterator &rhs) const
		{
			if (container_ != rhs.container_) throw invalid_iterator();
			return static_cast<int>(ptr_ - rhs.ptr_);
		}
		iterator& operator+=(const int &n)
		{
			ptr_ += n;
			return *this;
		}
		iterator& operator-=(const int &n)
		{
			ptr_ -= n;
			return *this;
		}
		/**
		 * iter++
		 */
		iterator operator++(int)
		{
			iterator tmp = *this;
			++ptr_;
			return tmp;
		}
		/**
		 * ++iter
		 */
		iterator& operator++()
		{
			++ptr_;
			return *this;
		}
		/**
		 * iter--
		 */
		iterator operator--(int)
		{
			iterator tmp = *this;
			--ptr_;
			return tmp;
		}
		/**
		 * --iter
		 */
		iterator& operator--()
		{
			--ptr_;
			return *this;
		}
		/**
		 * *it
		 */
		T& operator*() const
		{
			return *ptr_;
		}
		T* operator->() const
		{
			return ptr_;
		}
		/**
		 * check whether two iterators point to the same memory address.
		 */
		bool operator==(const iterator &rhs) const
		{
			return ptr_ == rhs.ptr_;
		}
		bool operator==(const const_iterator &rhs) const;
		/**
		 * some other operator for iterator.
		 */
		bool operator!=(const iterator &rhs) const
		{
			return ptr_ != rhs.ptr_;
		}
		bool operator!=(const const_iterator &rhs) const;
	};
	/**
	 * has same function as iterator, just for a const object.
	 */
	class const_iterator
	{
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = const T*;
		using reference = const T&;
		using iterator_category = std::output_iterator_tag;

		friend class vector;
		friend class iterator;

	private:
		const T *ptr_;
		const vector *container_;

	public:
		const_iterator() : ptr_(nullptr), container_(nullptr) {}
		const_iterator(const T *ptr, const vector *container) : ptr_(ptr), container_(container) {}
		// allow implicit conversion from a (mutable) iterator
		const_iterator(const iterator &it) : ptr_(it.ptr_), container_(it.container_) {}

		const_iterator operator+(const int &n) const
		{
			return const_iterator(ptr_ + n, container_);
		}
		const_iterator operator-(const int &n) const
		{
			return const_iterator(ptr_ - n, container_);
		}
		int operator-(const const_iterator &rhs) const
		{
			if (container_ != rhs.container_) throw invalid_iterator();
			return static_cast<int>(ptr_ - rhs.ptr_);
		}
		const_iterator& operator+=(const int &n)
		{
			ptr_ += n;
			return *this;
		}
		const_iterator& operator-=(const int &n)
		{
			ptr_ -= n;
			return *this;
		}
		const_iterator operator++(int)
		{
			const_iterator tmp = *this;
			++ptr_;
			return tmp;
		}
		const_iterator& operator++()
		{
			++ptr_;
			return *this;
		}
		const_iterator operator--(int)
		{
			const_iterator tmp = *this;
			--ptr_;
			return tmp;
		}
		const_iterator& operator--()
		{
			--ptr_;
			return *this;
		}
		const T& operator*() const
		{
			return *ptr_;
		}
		const T* operator->() const
		{
			return ptr_;
		}
		bool operator==(const const_iterator &rhs) const
		{
			return ptr_ == rhs.ptr_;
		}
		bool operator==(const iterator &rhs) const
		{
			return ptr_ == rhs.ptr_;
		}
		bool operator!=(const const_iterator &rhs) const
		{
			return ptr_ != rhs.ptr_;
		}
		bool operator!=(const iterator &rhs) const
		{
			return ptr_ != rhs.ptr_;
		}
	};
	/**
	 * Constructs
	 */
	vector() : data_(nullptr), sz_(0), cap_(0) {}
	vector(const vector &other) : data_(nullptr), sz_(0), cap_(0)
	{
		if (other.sz_ == 0) return;
		data_ = allocate(other.sz_);
		cap_ = other.sz_;
		try {
			for (; sz_ < other.sz_; ++sz_) {
				new (data_ + sz_) T(other.data_[sz_]);
			}
		} catch (...) {
			for (size_t j = 0; j < sz_; ++j) data_[j].~T();
			deallocate(data_);
			data_ = nullptr;
			sz_ = cap_ = 0;
			throw;
		}
	}
	/**
	 * Destructor
	 */
	~vector()
	{
		for (size_t i = 0; i < sz_; ++i) data_[i].~T();
		deallocate(data_);
	}
	/**
	 * Assignment operator
	 */
	vector &operator=(const vector &other)
	{
		if (this == &other) return *this;
		// build the new contents first, so a throw leaves *this unchanged
		T *nd = allocate(other.sz_);
		size_t i = 0;
		try {
			for (; i < other.sz_; ++i) {
				new (nd + i) T(other.data_[i]);
			}
		} catch (...) {
			for (size_t j = 0; j < i; ++j) nd[j].~T();
			deallocate(nd);
			throw;
		}
		for (size_t j = 0; j < sz_; ++j) data_[j].~T();
		deallocate(data_);
		data_ = nd;
		sz_ = other.sz_;
		cap_ = other.sz_;
		return *this;
	}
	/**
	 * access specified element with bounds checking
	 * throw index_out_of_bound if pos is not in [0, size)
	 */
	T & at(const size_t &pos)
	{
		if (pos >= sz_) throw index_out_of_bound();
		return data_[pos];
	}
	const T & at(const size_t &pos) const
	{
		if (pos >= sz_) throw index_out_of_bound();
		return data_[pos];
	}
	/**
	 * access specified element with bounds checking
	 * throw index_out_of_bound if pos is not in [0, size)
	 */
	T & operator[](const size_t &pos)
	{
		if (pos >= sz_) throw index_out_of_bound();
		return data_[pos];
	}
	const T & operator[](const size_t &pos) const
	{
		if (pos >= sz_) throw index_out_of_bound();
		return data_[pos];
	}
	/**
	 * access the first element.
	 * throw container_is_empty if size == 0
	 */
	const T & front() const
	{
		if (sz_ == 0) throw container_is_empty();
		return data_[0];
	}
	/**
	 * access the last element.
	 * throw container_is_empty if size == 0
	 */
	const T & back() const
	{
		if (sz_ == 0) throw container_is_empty();
		return data_[sz_ - 1];
	}
	/**
	 * returns an iterator to the beginning.
	 */
	iterator begin()
	{
		return iterator(data_, this);
	}
	const_iterator begin() const
	{
		return const_iterator(data_, this);
	}
	const_iterator cbegin() const
	{
		return const_iterator(data_, this);
	}
	/**
	 * returns an iterator to the end.
	 */
	iterator end()
	{
		return iterator(data_ + sz_, this);
	}
	const_iterator end() const
	{
		return const_iterator(data_ + sz_, this);
	}
	const_iterator cend() const
	{
		return const_iterator(data_ + sz_, this);
	}
	/**
	 * checks whether the container is empty
	 */
	bool empty() const
	{
		return sz_ == 0;
	}
	/**
	 * returns the number of elements
	 */
	size_t size() const
	{
		return sz_;
	}
	/**
	 * clears the contents
	 */
	void clear()
	{
		for (size_t i = 0; i < sz_; ++i) data_[i].~T();
		sz_ = 0;
	}
	/**
	 * inserts value before pos
	 * returns an iterator pointing to the inserted value.
	 */
	iterator insert(iterator pos, const T &value)
	{
		size_t ind = static_cast<size_t>(pos.ptr_ - data_);
		return insert(ind, value);
	}
	/**
	 * inserts value at index ind.
	 * after inserting, this->at(ind) == value
	 * returns an iterator pointing to the inserted value.
	 * throw index_out_of_bound if ind > size
	 */
	iterator insert(const size_t &ind, const T &value)
	{
		if (ind > sz_) throw index_out_of_bound();
		if (sz_ == cap_) reserve(nextCapacity());
		if (ind == sz_) {
			// append: construct in place
			new (data_ + sz_) T(value);
		} else {
			// make room by constructing a new last element from the old last,
			// then shift the tail one slot to the right via assignment.
			new (data_ + sz_) T(data_[sz_ - 1]);
			for (size_t i = sz_ - 1; i > ind; --i) {
				data_[i] = data_[i - 1];
			}
			data_[ind] = value;
		}
		++sz_;
		return iterator(data_ + ind, this);
	}
	/**
	 * removes the element at pos.
	 * return an iterator pointing to the following element.
	 * If the iterator pos refers the last element, the end() iterator is returned.
	 */
	iterator erase(iterator pos)
	{
		size_t ind = static_cast<size_t>(pos.ptr_ - data_);
		return erase(ind);
	}
	/**
	 * removes the element with index ind.
	 * return an iterator pointing to the following element.
	 * throw index_out_of_bound if ind >= size
	 */
	iterator erase(const size_t &ind)
	{
		if (ind >= sz_) throw index_out_of_bound();
		for (size_t i = ind; i + 1 < sz_; ++i) {
			data_[i] = data_[i + 1];
		}
		data_[sz_ - 1].~T();
		--sz_;
		return iterator(data_ + ind, this);
	}
	/**
	 * adds an element to the end.
	 */
	void push_back(const T &value)
	{
		if (sz_ == cap_) reserve(nextCapacity());
		new (data_ + sz_) T(value);
		++sz_;
	}
	/**
	 * remove the last element from the end.
	 * throw container_is_empty if size() == 0
	 */
	void pop_back()
	{
		if (sz_ == 0) throw container_is_empty();
		data_[sz_ - 1].~T();
		--sz_;
	}
};

// Cross-type iterator comparisons, defined out of line because they need the
// full definition of const_iterator, which follows iterator.
template<typename T>
bool vector<T>::iterator::operator==(const const_iterator &rhs) const
{
	return ptr_ == rhs.ptr_;
}

template<typename T>
bool vector<T>::iterator::operator!=(const const_iterator &rhs) const
{
	return ptr_ != rhs.ptr_;
}

}

#endif
