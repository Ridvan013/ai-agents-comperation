#ifndef SJTU_VECTOR_HPP
#define SJTU_VECTOR_HPP

#include <cstddef>
#include <stdexcept>
#include <iostream>

namespace sjtu {

template<typename T>
class vector {
private:
    T* data;
    size_t sz;
    size_t cap;

    void reallocate(size_t new_cap) {
        T* new_data = reinterpret_cast<T*>(new char[new_cap * sizeof(T)]);
        for (size_t i = 0; i < sz; ++i) {
            new (new_data + i) T(std::move(data[i]));
            data[i].~T();
        }
        delete[] reinterpret_cast<char*>(data);
        data = new_data;
        cap = new_cap;
    }

public:
    vector() : data(nullptr), sz(0), cap(0) {}
    
    vector(const vector& other) : sz(other.sz), cap(other.cap) {
        data = reinterpret_cast<T*>(new char[cap * sizeof(T)]);
        for (size_t i = 0; i < sz; ++i) {
            new (data + i) T(other.data[i]);
        }
    }
    
    vector(vector&& other) noexcept : data(other.data), sz(other.sz), cap(other.cap) {
        other.data = nullptr;
        other.sz = 0;
        other.cap = 0;
    }
    
    vector& operator=(const vector& other) {
        if (this != &other) {
            clear();
            delete[] reinterpret_cast<char*>(data);
            sz = other.sz;
            cap = other.cap;
            data = reinterpret_cast<T*>(new char[cap * sizeof(T)]);
            for (size_t i = 0; i < sz; ++i) {
                new (data + i) T(other.data[i]);
            }
        }
        return *this;
    }
    
    vector& operator=(vector&& other) noexcept {
        if (this != &other) {
            clear();
            delete[] reinterpret_cast<char*>(data);
            data = other.data;
            sz = other.sz;
            cap = other.cap;
            other.data = nullptr;
            other.sz = 0;
            other.cap = 0;
        }
        return *this;
    }

    ~vector() {
        clear();
        delete[] reinterpret_cast<char*>(data);
    }

    void push_back(const T& value) {
        if (sz == cap) {
            reallocate(cap == 0 ? 1 : cap * 2);
        }
        new (data + sz) T(value);
        ++sz;
    }

    void push_back(T&& value) {
        if (sz == cap) {
            reallocate(cap == 0 ? 1 : cap * 2);
        }
        new (data + sz) T(std::move(value));
        ++sz;
    }

    void pop_back() {
        if (sz > 0) {
            --sz;
            data[sz].~T();
        }
    }

    T& operator[](size_t index) {
        return data[index];
    }

    const T& operator[](size_t index) const {
        return data[index];
    }

    size_t size() const { return sz; }
    bool empty() const { return sz == 0; }
    
    void clear() {
        for (size_t i = 0; i < sz; ++i) {
            data[i].~T();
        }
        sz = 0;
    }
    
    void resize(size_t new_size) {
        if (new_size > cap) {
            reallocate(new_size);
        }
        if (new_size > sz) {
            for (size_t i = sz; i < new_size; ++i) {
                new (data + i) T();
            }
        } else {
            for (size_t i = new_size; i < sz; ++i) {
                data[i].~T();
            }
        }
        sz = new_size;
    }

    T* begin() { return data; }
    T* end() { return data + sz; }
    const T* begin() const { return data; }
    const T* end() const { return data + sz; }
};

} // namespace sjtu

#endif // SJTU_VECTOR_HPP
