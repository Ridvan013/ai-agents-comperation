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
private:
	/**
	 * Raw, uninitialized storage for the elements. We manage lifetimes
	 * manually with placement new / explicit destructor calls so that the
	 * element type T is never required to be default constructible.
	 */
	T *data_;
	size_t size_;
	size_t capacity_;

	/**
	 * Allocate raw memory able to hold `n` objects of type T without
	 * constructing any of them.
	 */
	static T *allocate(size_t n)
	{
		if (n == 0) return nullptr;
		return static_cast<T *>(operator new(n * sizeof(T)));
	}

	/**
	 * Release raw memory previously obtained from allocate().
	 */
	static void deallocate(T *p)
	{
		operator delete(p);
	}

	/**
	 * Grow the underlying storage to (at least) `new_cap` elements,
	 * relocating existing elements by copy construction.
	 */
	void reserve(size_t new_cap)
	{
		if (new_cap <= capacity_) return;
		T *new_data = allocate(new_cap);
		for (size_t i = 0; i < size_; ++i) {
			new (new_data + i) T(data_[i]);
		}
		for (size_t i = 0; i < size_; ++i) {
			data_[i].~T();
		}
		deallocate(data_);
		data_ = new_data;
		capacity_ = new_cap;
	}

	/**
	 * Destroy all live elements without freeing the storage.
	 */
	void destroy_all()
	{
		for (size_t i = 0; i < size_; ++i) {
			data_[i].~T();
		}
	}

public:
	/**
	 * a type for actions of the elements of a vector, and a class named
	 * const_iterator with same interfaces.
	 * see RandomAccessIterator at CppReference for help.
	 */
	class const_iterator;
	class iterator
	{
	// The following code is written for the C++ type_traits library.
	// Type traits is a C++ feature for describing certain properties of a type.
	// For instance, for an iterator, iterator::value_type is the type that the
	// iterator points to.
	// STL algorithms and containers may use these type_traits (e.g. the following
	// typedef) to work properly. In particular, without the following code,
	// @code{std::sort(iter, iter1);} would not compile.
	// See these websites for more information:
	// https://en.cppreference.com/w/cpp/header/type_traits
	// About value_type: https://blog.csdn.net/u014299153/article/details/72419713
	// About iterator_category: https://en.cppreference.com/w/cpp/iterator
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;
		using iterator_category = std::output_iterator_tag;

	private:
		friend class vector;
		friend class const_iterator;
		// `origin_` identifies the owning vector so we can detect iterators
		// from different containers; `ptr_` is the referenced slot.
		const vector *origin_;
		T *ptr_;

		iterator(const vector *origin, T *ptr) : origin_(origin), ptr_(ptr) {}

	public:
		iterator() : origin_(nullptr), ptr_(nullptr) {}

		/**
		 * return a new iterator which pointer n-next elements
		 * as well as operator-
		 */
		iterator operator+(const int &n) const
		{
			return iterator(origin_, ptr_ + n);
		}
		iterator operator-(const int &n) const
		{
			return iterator(origin_, ptr_ - n);
		}
		// return the distance between two iterators,
		// if these two iterators point to different vectors, throw invaild_iterator.
		int operator-(const iterator &rhs) const
		{
			if (origin_ != rhs.origin_) throw invalid_iterator();
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
		 * a operator to check whether two iterators are same (pointing to the same memory address).
		 */
		bool operator==(const iterator &rhs) const
		{
			return ptr_ == rhs.ptr_;
		}
		bool operator==(const const_iterator &rhs) const
		{
			return ptr_ == rhs.ptr_;
		}
		/**
		 * some other operator for iterator.
		 */
		bool operator!=(const iterator &rhs) const
		{
			return ptr_ != rhs.ptr_;
		}
		bool operator!=(const const_iterator &rhs) const
		{
			return ptr_ != rhs.ptr_;
		}
	};
	/**
	 * has same function as iterator, just for a const object.
	 */
	class const_iterator
	{
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;
		using iterator_category = std::output_iterator_tag;

	private:
		friend class vector;
		friend class iterator;
		const vector *origin_;
		const T *ptr_;

		const_iterator(const vector *origin, const T *ptr) : origin_(origin), ptr_(ptr) {}

	public:
		const_iterator() : origin_(nullptr), ptr_(nullptr) {}
		const_iterator(const iterator &other) : origin_(other.origin_), ptr_(other.ptr_) {}

		const_iterator operator+(const int &n) const
		{
			return const_iterator(origin_, ptr_ + n);
		}
		const_iterator operator-(const int &n) const
		{
			return const_iterator(origin_, ptr_ - n);
		}
		int operator-(const const_iterator &rhs) const
		{
			if (origin_ != rhs.origin_) throw invalid_iterator();
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
		bool operator==(const iterator &rhs) const
		{
			return ptr_ == rhs.ptr_;
		}
		bool operator==(const const_iterator &rhs) const
		{
			return ptr_ == rhs.ptr_;
		}
		bool operator!=(const iterator &rhs) const
		{
			return ptr_ != rhs.ptr_;
		}
		bool operator!=(const const_iterator &rhs) const
		{
			return ptr_ != rhs.ptr_;
		}
	};
	/**
	 * Constructs
	 * At least two: default constructor, copy constructor
	 */
	vector() : data_(nullptr), size_(0), capacity_(0) {}
	vector(const vector &other) : data_(nullptr), size_(0), capacity_(0)
	{
		if (other.size_ > 0) {
			data_ = allocate(other.size_);
			capacity_ = other.size_;
			for (size_t i = 0; i < other.size_; ++i) {
				new (data_ + i) T(other.data_[i]);
			}
			size_ = other.size_;
		}
	}
	/**
	 * Destructor
	 */
	~vector()
	{
		destroy_all();
		deallocate(data_);
	}
	/**
	 * Assignment operator
	 */
	vector &operator=(const vector &other)
	{
		if (this == &other) return *this;
		destroy_all();
		deallocate(data_);
		data_ = nullptr;
		size_ = 0;
		capacity_ = 0;
		if (other.size_ > 0) {
			data_ = allocate(other.size_);
			capacity_ = other.size_;
			for (size_t i = 0; i < other.size_; ++i) {
				new (data_ + i) T(other.data_[i]);
			}
			size_ = other.size_;
		}
		return *this;
	}
	/**
	 * assigns specified element with bounds checking
	 * throw index_out_of_bound if pos is not in [0, size)
	 */
	T & at(const size_t &pos)
	{
		if (pos >= size_) throw index_out_of_bound();
		return data_[pos];
	}
	const T & at(const size_t &pos) const
	{
		if (pos >= size_) throw index_out_of_bound();
		return data_[pos];
	}
	/**
	 * assigns specified element with bounds checking
	 * throw index_out_of_bound if pos is not in [0, size)
	 * !!! Pay attentions
	 *   In STL this operator does not check the boundary but I want you to do.
	 */
	T & operator[](const size_t &pos)
	{
		if (pos >= size_) throw index_out_of_bound();
		return data_[pos];
	}
	const T & operator[](const size_t &pos) const
	{
		if (pos >= size_) throw index_out_of_bound();
		return data_[pos];
	}
	/**
	 * access the first element.
	 * throw container_is_empty if size == 0
	 */
	const T & front() const
	{
		if (size_ == 0) throw container_is_empty();
		return data_[0];
	}
	/**
	 * access the last element.
	 * throw container_is_empty if size == 0
	 */
	const T & back() const
	{
		if (size_ == 0) throw container_is_empty();
		return data_[size_ - 1];
	}
	/**
	 * returns an iterator to the beginning.
	 */
	iterator begin()
	{
		return iterator(this, data_);
	}
	const_iterator begin() const
	{
		return const_iterator(this, data_);
	}
	const_iterator cbegin() const
	{
		return const_iterator(this, data_);
	}
	/**
	 * returns an iterator to the end.
	 */
	iterator end()
	{
		return iterator(this, data_ + size_);
	}
	const_iterator end() const
	{
		return const_iterator(this, data_ + size_);
	}
	const_iterator cend() const
	{
		return const_iterator(this, data_ + size_);
	}
	/**
	 * checks whether the container is empty
	 */
	bool empty() const
	{
		return size_ == 0;
	}
	/**
	 * returns the number of elements
	 */
	size_t size() const
	{
		return size_;
	}
	/**
	 * clears the contents
	 */
	void clear()
	{
		destroy_all();
		size_ = 0;
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
	 * throw index_out_of_bound if ind > size (in this situation ind can be size because after inserting the size will increase 1.)
	 */
	iterator insert(const size_t &ind, const T &value)
	{
		if (ind > size_) throw index_out_of_bound();
		if (size_ == capacity_) {
			reserve(capacity_ == 0 ? 1 : capacity_ * 2);
		}
		if (ind == size_) {
			new (data_ + size_) T(value);
		} else {
			// Construct the new last slot from the current last element, then
			// shift the tail one position to the right via assignment.
			new (data_ + size_) T(data_[size_ - 1]);
			for (size_t i = size_ - 1; i > ind; --i) {
				data_[i] = data_[i - 1];
			}
			data_[ind] = value;
		}
		++size_;
		return iterator(this, data_ + ind);
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
		if (ind >= size_) throw index_out_of_bound();
		for (size_t i = ind; i + 1 < size_; ++i) {
			data_[i] = data_[i + 1];
		}
		data_[size_ - 1].~T();
		--size_;
		return iterator(this, data_ + ind);
	}
	/**
	 * adds an element to the end.
	 */
	void push_back(const T &value)
	{
		if (size_ == capacity_) {
			reserve(capacity_ == 0 ? 1 : capacity_ * 2);
		}
		new (data_ + size_) T(value);
		++size_;
	}
	/**
	 * remove the last element from the end.
	 * throw container_is_empty if size() == 0
	 */
	void pop_back()
	{
		if (size_ == 0) throw container_is_empty();
		data_[size_ - 1].~T();
		--size_;
	}
};


}

#endif
