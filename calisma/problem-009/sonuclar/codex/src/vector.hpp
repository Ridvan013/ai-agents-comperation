#ifndef SJTU_VECTOR_HPP
#define SJTU_VECTOR_HPP

#include "exceptions.hpp"

#include <cstddef>
#include <iterator>
#include <new>

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
	T *data_;
	size_t size_;
	size_t capacity_;

	static T *allocate_storage(size_t capacity)
	{
		if (capacity == 0) {
			return nullptr;
		}
		return static_cast<T *>(::operator new(sizeof(T) * capacity));
	}

	static void destroy_range(T *data, size_t count)
	{
		for (size_t i = 0; i < count; ++i) {
			(data + i)->~T();
		}
	}

	static size_t next_capacity(size_t current, size_t required)
	{
		size_t candidate = current == 0 ? 1 : current;
		while (candidate < required) {
			candidate <<= 1;
		}
		return candidate;
	}

	void release_storage()
	{
		destroy_range(data_, size_);
		::operator delete(data_);
		data_ = nullptr;
		size_ = 0;
		capacity_ = 0;
	}

	void reallocate(size_t new_capacity)
	{
		T *new_data = allocate_storage(new_capacity);
		size_t constructed = 0;
		try {
			for (; constructed < size_; ++constructed) {
				new (new_data + constructed) T(data_[constructed]);
			}
		} catch (...) {
			destroy_range(new_data, constructed);
			::operator delete(new_data);
			throw;
		}
		destroy_range(data_, size_);
		::operator delete(data_);
		data_ = new_data;
		capacity_ = new_capacity;
	}

	void ensure_capacity(size_t required)
	{
		if (required <= capacity_) {
			return;
		}
		reallocate(next_capacity(capacity_, required));
	}

	void copy_from(const vector &other)
	{
		data_ = nullptr;
		size_ = 0;
		capacity_ = 0;
		if (other.size_ == 0) {
			return;
		}
		data_ = allocate_storage(other.size_);
		capacity_ = other.size_;
		size_t constructed = 0;
		try {
			for (; constructed < other.size_; ++constructed) {
				new (data_ + constructed) T(other.data_[constructed]);
			}
		} catch (...) {
			destroy_range(data_, constructed);
			::operator delete(data_);
			data_ = nullptr;
			capacity_ = 0;
			throw;
		}
		size_ = other.size_;
	}

	void swap(vector &other)
	{
		T *tmp_data = data_;
		data_ = other.data_;
		other.data_ = tmp_data;

		size_t tmp_size = size_;
		size_ = other.size_;
		other.size_ = tmp_size;

		size_t tmp_capacity = capacity_;
		capacity_ = other.capacity_;
		other.capacity_ = tmp_capacity;
	}

	T *finish_pointer()
	{
		return size_ == 0 ? data_ : data_ + size_;
	}

	const T *finish_pointer() const
	{
		return size_ == 0 ? data_ : data_ + size_;
	}

public:
	/**
	 * TODO
	 * a type for actions of the elements of a vector, and you should write
	 *   a class named const_iterator with same interfaces.
	 */
	/**
	 * you can see RandomAccessIterator at CppReference for help.
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
		using pointer = T *;
		using reference = T &;
		using iterator_category = std::random_access_iterator_tag;

	private:
		vector *owner_;
		T *current_;

		void validate_dereference() const
		{
			if (owner_ == nullptr || owner_->size_ == 0 || current_ == nullptr || current_ < owner_->data_ || current_ >= owner_->finish_pointer()) {
				throw invalid_iterator();
			}
		}

		friend class vector;
		friend class const_iterator;

	public:
		iterator(vector *owner = nullptr, T *current = nullptr) : owner_(owner), current_(current) {}

		/**
		 * return a new iterator which pointer n-next elements
		 * as well as operator-
		 */
		iterator operator+(const int &n) const
		{
			return iterator(owner_, current_ + n);
		}
		iterator operator-(const int &n) const
		{
			return iterator(owner_, current_ - n);
		}
		// return the distance between two iterators,
		// if these two iterators point to different vectors, throw invaild_iterator.
		int operator-(const iterator &rhs) const
		{
			if (owner_ != rhs.owner_) {
				throw invalid_iterator();
			}
			return static_cast<int>(current_ - rhs.current_);
		}
		int operator-(const const_iterator &rhs) const
		{
			if (owner_ != rhs.owner_) {
				throw invalid_iterator();
			}
			return static_cast<int>(current_ - rhs.current_);
		}
		iterator &operator+=(const int &n)
		{
			current_ += n;
			return *this;
		}
		iterator &operator-=(const int &n)
		{
			current_ -= n;
			return *this;
		}
		/**
		 * TODO iter++
		 */
		iterator operator++(int)
		{
			iterator tmp(*this);
			++current_;
			return tmp;
		}
		/**
		 * TODO ++iter
		 */
		iterator &operator++()
		{
			++current_;
			return *this;
		}
		/**
		 * TODO iter--
		 */
		iterator operator--(int)
		{
			iterator tmp(*this);
			--current_;
			return tmp;
		}
		/**
		 * TODO --iter
		 */
		iterator &operator--()
		{
			--current_;
			return *this;
		}
		/**
		 * TODO *it
		 */
		T &operator*() const
		{
			validate_dereference();
			return *current_;
		}
		/**
		 * a operator to check whether two iterators are same (pointing to the same memory address).
		 */
		bool operator==(const iterator &rhs) const
		{
			return owner_ == rhs.owner_ && current_ == rhs.current_;
		}
		bool operator==(const const_iterator &rhs) const
		{
			return owner_ == rhs.owner_ && current_ == rhs.current_;
		}
		/**
		 * some other operator for iterator.
		 */
		bool operator!=(const iterator &rhs) const
		{
			return !(*this == rhs);
		}
		bool operator!=(const const_iterator &rhs) const
		{
			return !(*this == rhs);
		}
	};
	/**
	 * TODO
	 * has same function as iterator, just for a const object.
	 */
	class const_iterator
	{
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = const T *;
		using reference = const T &;
		using iterator_category = std::random_access_iterator_tag;

	private:
		const vector *owner_;
		const T *current_;

		void validate_dereference() const
		{
			if (owner_ == nullptr || owner_->size_ == 0 || current_ == nullptr || current_ < owner_->data_ || current_ >= owner_->finish_pointer()) {
				throw invalid_iterator();
			}
		}

		friend class vector;
		friend class iterator;

	public:
		const_iterator(const vector *owner = nullptr, const T *current = nullptr) : owner_(owner), current_(current) {}
		const_iterator(const iterator &other) : owner_(other.owner_), current_(other.current_) {}

		const_iterator operator+(const int &n) const
		{
			return const_iterator(owner_, current_ + n);
		}
		const_iterator operator-(const int &n) const
		{
			return const_iterator(owner_, current_ - n);
		}
		int operator-(const iterator &rhs) const
		{
			if (owner_ != rhs.owner_) {
				throw invalid_iterator();
			}
			return static_cast<int>(current_ - rhs.current_);
		}
		int operator-(const const_iterator &rhs) const
		{
			if (owner_ != rhs.owner_) {
				throw invalid_iterator();
			}
			return static_cast<int>(current_ - rhs.current_);
		}
		const_iterator &operator+=(const int &n)
		{
			current_ += n;
			return *this;
		}
		const_iterator &operator-=(const int &n)
		{
			current_ -= n;
			return *this;
		}
		const_iterator operator++(int)
		{
			const_iterator tmp(*this);
			++current_;
			return tmp;
		}
		const_iterator &operator++()
		{
			++current_;
			return *this;
		}
		const_iterator operator--(int)
		{
			const_iterator tmp(*this);
			--current_;
			return tmp;
		}
		const_iterator &operator--()
		{
			--current_;
			return *this;
		}
		const T &operator*() const
		{
			validate_dereference();
			return *current_;
		}
		bool operator==(const iterator &rhs) const
		{
			return owner_ == rhs.owner_ && current_ == rhs.current_;
		}
		bool operator==(const const_iterator &rhs) const
		{
			return owner_ == rhs.owner_ && current_ == rhs.current_;
		}
		bool operator!=(const iterator &rhs) const
		{
			return !(*this == rhs);
		}
		bool operator!=(const const_iterator &rhs) const
		{
			return !(*this == rhs);
		}
	};
	/**
	 * TODO Constructs
	 * At least two: default constructor, copy constructor
	 */
	vector() : data_(nullptr), size_(0), capacity_(0) {}
	vector(const vector &other)
	{
		copy_from(other);
	}
	/**
	 * TODO Destructor
	 */
	~vector()
	{
		release_storage();
	}
	/**
	 * TODO Assignment operator
	 */
	vector &operator=(const vector &other)
	{
		if (this == &other) {
			return *this;
		}
		vector tmp(other);
		swap(tmp);
		return *this;
	}
	/**
	 * assigns specified element with bounds checking
	 * throw index_out_of_bound if pos is not in [0, size)
	 */
	T &at(const size_t &pos)
	{
		if (pos >= size_) {
			throw index_out_of_bound();
		}
		return data_[pos];
	}
	const T &at(const size_t &pos) const
	{
		if (pos >= size_) {
			throw index_out_of_bound();
		}
		return data_[pos];
	}
	/**
	 * assigns specified element with bounds checking
	 * throw index_out_of_bound if pos is not in [0, size)
	 * !!! Pay attentions
	 *   In STL this operator does not check the boundary but I want you to do.
	 */
	T &operator[](const size_t &pos)
	{
		return at(pos);
	}
	const T &operator[](const size_t &pos) const
	{
		return at(pos);
	}
	/**
	 * access the first element.
	 * throw container_is_empty if size == 0
	 */
	const T &front() const
	{
		if (size_ == 0) {
			throw container_is_empty();
		}
		return data_[0];
	}
	/**
	 * access the last element.
	 * throw container_is_empty if size == 0
	 */
	const T &back() const
	{
		if (size_ == 0) {
			throw container_is_empty();
		}
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
		return iterator(this, finish_pointer());
	}
	const_iterator end() const
	{
		return const_iterator(this, finish_pointer());
	}
	const_iterator cend() const
	{
		return const_iterator(this, finish_pointer());
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
		destroy_range(data_, size_);
		size_ = 0;
	}
	/**
	 * inserts value before pos
	 * returns an iterator pointing to the inserted value.
	 */
	iterator insert(iterator pos, const T &value)
	{
		if (pos.owner_ != this) {
			throw invalid_iterator();
		}
		if (size_ == 0) {
			if (pos.current_ != data_) {
				throw invalid_iterator();
			}
			return insert(static_cast<size_t>(0), value);
		} else if (pos.current_ < data_ || pos.current_ > finish_pointer()) {
			throw invalid_iterator();
		}
		return insert(static_cast<size_t>(pos.current_ - data_), value);
	}
	/**
	 * inserts value at index ind.
	 * after inserting, this->at(ind) == value
	 * returns an iterator pointing to the inserted value.
	 * throw index_out_of_bound if ind > size (in this situation ind can be size because after inserting the size will increase 1.)
	 */
	iterator insert(const size_t &ind, const T &value)
	{
		if (ind > size_) {
			throw index_out_of_bound();
		}
		T value_copy(value);
		if (size_ == capacity_) {
			size_t new_capacity = next_capacity(capacity_, size_ + 1);
			T *new_data = allocate_storage(new_capacity);
			size_t constructed = 0;
			try {
				for (; constructed < ind; ++constructed) {
					new (new_data + constructed) T(data_[constructed]);
				}
				new (new_data + constructed) T(value_copy);
				++constructed;
				for (size_t i = ind; i < size_; ++i, ++constructed) {
					new (new_data + constructed) T(data_[i]);
				}
			} catch (...) {
				destroy_range(new_data, constructed);
				::operator delete(new_data);
				throw;
			}
			destroy_range(data_, size_);
			::operator delete(data_);
			data_ = new_data;
			capacity_ = new_capacity;
			++size_;
			return iterator(this, data_ + ind);
		}

		if (ind == size_) {
			new (data_ + size_) T(value_copy);
			++size_;
			return iterator(this, data_ + ind);
		}

		new (data_ + size_) T(data_[size_ - 1]);
		++size_;
		for (size_t i = size_ - 1; i > ind; --i) {
			data_[i] = data_[i - 1];
		}
		data_[ind] = value_copy;
		return iterator(this, data_ + ind);
	}
	/**
	 * removes the element at pos.
	 * return an iterator pointing to the following element.
	 * If the iterator pos refers the last element, the end() iterator is returned.
	 */
	iterator erase(iterator pos)
	{
		if (pos.owner_ != this || size_ == 0 || pos.current_ < data_ || pos.current_ >= finish_pointer()) {
			throw invalid_iterator();
		}
		return erase(static_cast<size_t>(pos.current_ - data_));
	}
	/**
	 * removes the element with index ind.
	 * return an iterator pointing to the following element.
	 * throw index_out_of_bound if ind >= size
	 */
	iterator erase(const size_t &ind)
	{
		if (ind >= size_) {
			throw index_out_of_bound();
		}
		for (size_t i = ind; i + 1 < size_; ++i) {
			data_[i] = data_[i + 1];
		}
		(data_ + size_ - 1)->~T();
		--size_;
		return iterator(this, data_ + ind);
	}
	/**
	 * adds an element to the end.
	 */
	void push_back(const T &value)
	{
		insert(size_, value);
	}
	/**
	 * remove the last element from the end.
	 * throw container_is_empty if size() == 0
	 */
	void pop_back()
	{
		if (size_ == 0) {
			throw container_is_empty();
		}
		(data_ + size_ - 1)->~T();
		--size_;
	}
};

}

#endif
