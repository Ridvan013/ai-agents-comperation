#ifndef SJTU_UTILITY_HPP
#define SJTU_UTILITY_HPP

#include <utility>

namespace sjtu {

template<class T1, class T2>
struct pair {
    T1 first;
    T2 second;
    
    pair() : first(), second() {}
    pair(const T1& x, const T2& y) : first(x), second(y) {}
    pair(const pair& other) : first(other.first), second(other.second) {}
    pair(pair&& other) noexcept : first(std::move(other.first)), second(std::move(other.second)) {}
    
    pair& operator=(const pair& other) {
        if (this != &other) {
            first = other.first;
            second = other.second;
        }
        return *this;
    }
    
    pair& operator=(pair&& other) noexcept {
        if (this != &other) {
            first = std::move(other.first);
            second = std::move(other.second);
        }
        return *this;
    }

    bool operator==(const pair& other) const {
        return first == other.first && second == other.second;
    }
    bool operator!=(const pair& other) const {
        return !(*this == other);
    }
    bool operator<(const pair& other) const {
        if (first < other.first) return true;
        if (other.first < first) return false;
        return second < other.second;
    }
    bool operator<=(const pair& other) const {
        return !(other < *this);
    }
    bool operator>(const pair& other) const {
        return other < *this;
    }
    bool operator>=(const pair& other) const {
        return !(*this < other);
    }
};

template<class T>
void swap(T& a, T& b) {
    T temp = std::move(a);
    a = std::move(b);
    b = std::move(temp);
}

template<class RandomIt, class Compare>
void sort(RandomIt first, RandomIt last, Compare comp) {
    if (first >= last - 1) return;
    auto pivot = *(first + (last - first) / 2);
    RandomIt left = first;
    RandomIt right = last - 1;
    while (left <= right) {
        while (comp(*left, pivot)) ++left;
        while (comp(pivot, *right)) --right;
        if (left <= right) {
            swap(*left, *right);
            ++left;
            --right;
        }
    }
    if (first < right + 1) sort(first, right + 1, comp);
    if (left < last) sort(left, last, comp);
}

template<class RandomIt>
void sort(RandomIt first, RandomIt last) {
    sort(first, last, [](const auto& a, const auto& b) { return a < b; });
}

} // namespace sjtu

#endif // SJTU_UTILITY_HPP
