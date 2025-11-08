#pragma once

#include <cassert>
#include <initializer_list>
#include <algorithm>
#include <stdexcept>
#include <compare>
#include "array_ptr.h"

struct ReserveProxyObj {
    explicit ReserveProxyObj(size_t capacity_to_reserve) : capacity(capacity_to_reserve) {
    }
    size_t capacity;
};

ReserveProxyObj Reserve(size_t capacity_to_reserve) {
    return ReserveProxyObj(capacity_to_reserve);
}

template <typename Type>
class SimpleVector {
public:
    using Iterator = Type*;
    using ConstIterator = const Type*;

    SimpleVector() noexcept = default;

    explicit SimpleVector(size_t size)
        : items_(size)
        , size_(size)
        , capacity_(size) {
        std::generate(begin(), end(), []{ 
            return Type{}; 
            });
    }

    SimpleVector(size_t size, const Type& value)
        : items_(size)  
        , size_(size)
        , capacity_(size) {
        std::fill(begin(), end(), value);
    }

    SimpleVector(std::initializer_list<Type> init)
        : items_(init.size())
        , size_(init.size())
        , capacity_(init.size()) {
        std::copy(init.begin(), init.end(), this->begin());
    }

    SimpleVector(ReserveProxyObj reserved)
        : items_(reserved.capacity)
        , size_(0)
        , capacity_(reserved.capacity) {
    }

    SimpleVector(const SimpleVector& other)
        : items_(other.size_)
        , size_(other.size_)
        , capacity_(other.size_) {
        std::copy(other.begin(), other.end(), this->begin());
    }

    SimpleVector(SimpleVector&& other)
        : items_(std::move(other.items_))
        , size_(std::exchange(other.size_, 0))
        , capacity_(std::exchange(other.capacity_, 0)) { 
        }

    SimpleVector& operator=(const SimpleVector& other) {
        if (&other != this) {
            if (other.IsEmpty()) {
                Clear();
                capacity_ = 0;
                items_ = ArrayPtr<Type>();
            } else {
                SimpleVector other_copy(other);
                swap(other_copy);
            }
        }
        return *this;
    }

    SimpleVector& operator=(SimpleVector&& other) {
        if (this != &other) {
            swap(other);
        }
        return *this;
    }

    void PushBack(const Type& item) {
        if (IsFull()) {
            IncCapacity();
        }
        items_[size_++] = item;
    }

    void PushBack(Type&& item) {
        if (IsFull()) {
            IncCapacity();
        }
        items_[size_++] = std::move(item);
    }

    Iterator Insert(ConstIterator pos, const Type& value) {
        assert(begin() <= pos && pos <= end());
        size_t offset = std::distance(cbegin(), pos);

        if (IsFull()) {
            IncCapacity();
        }

        ++size_;
        Iterator  it = begin() + offset;
        std::copy_backward(it, end() - 1, end());
        *it = value;
        return it;
    }

    Iterator Insert(ConstIterator pos, Type&& value) {
        assert(begin() <= pos && pos <= end());
        size_t offset = std::distance(cbegin(), pos);

        if (IsFull()) {
            IncCapacity();
        }

        ++size_;
        Iterator  it = begin() + offset;
        std::copy_backward(std::make_move_iterator(it), std::make_move_iterator(end() - 1), end());
        *it = std::move(value);
        return it;
    }

    void PopBack() noexcept {
        if (!IsEmpty()) {
            --size_;
        }
    }

    Iterator Erase(ConstIterator pos) {
        assert(begin() <= pos && pos < end());
        Iterator erase_pos = const_cast<Iterator>(pos);
        std::move(erase_pos + 1, end(), erase_pos);
        --size_;
        return erase_pos;
    }

    void swap(SimpleVector& other) noexcept {
        items_.swap(other.items_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    size_t GetSize() const noexcept {
        return size_;
    }

    size_t GetCapacity() const noexcept {
        return capacity_;
    }

    bool IsEmpty() const noexcept {
        return size_ == 0;
    }

    bool IsFull() const noexcept {
        return size_ == capacity_;
    }

    Type& operator[](size_t index) noexcept {
        assert(index < size_);
        return items_[index];
    }

    const Type& operator[](size_t index) const noexcept {
        assert(index < size_);
        return items_[index];
    }

    Type& At(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return items_[index];
    }

    const Type& At(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return items_[index];
    }

    void Clear() noexcept {
        size_ = 0;
    }

    void Resize(size_t new_size) {
        if (new_size > capacity_) {
            const size_t new_capacity = NewCapacity(new_size);
            SimpleVector<Type> buffer(new_capacity);
            std::move(begin(), end(), buffer.begin());
            swap(buffer);
        }
        else if (new_size > size_) {
            assert(new_size <= capacity_);
            std::generate(end(), begin() + new_size, [] {
                return Type{};
                });
        }
        size_ = new_size;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity > capacity_) {
            size_t size_save = size_;
            Resize(new_capacity);
            size_ = size_save;
            capacity_ = new_capacity;
        }
    };

    Iterator begin() noexcept {
        return items_.GetRawPtr();
    }

    Iterator end() noexcept {
        return items_.GetRawPtr() + size_;
    }

    ConstIterator begin() const noexcept {
        return items_.GetRawPtr();
    }

    ConstIterator end() const noexcept {
        return items_.GetRawPtr() + size_;
    }

    ConstIterator cbegin() const noexcept {
        return items_.GetRawPtr();
    }

    ConstIterator cend() const noexcept {
        return items_.GetRawPtr() + size_;
    }

private:
    void IncCapacity() {
        SimpleVector<Type> buffer(NewCapacity());
        buffer.size_ = size_;
        std::move(begin(), end(), buffer.begin());
        swap(buffer);
    }

    size_t NewCapacity(const size_t new_size = 1) {
        return std::max(capacity_ * 2, new_size);
    }

private:
    ArrayPtr<Type> items_;
    size_t size_ = 0;
    size_t capacity_ = 0;
};

template <typename Type>
inline std::strong_ordering operator<=>(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return std::lexicographical_compare_three_way(
        lhs.cbegin(), lhs.cend(),
        rhs.cbegin(), rhs.cend());
}

template <typename Type>
inline bool operator==(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return (lhs <=> rhs) == std::strong_ordering::equal;
}

template <typename Type>
inline bool operator!=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return (lhs <=> rhs) != std::strong_ordering::equal;
}

template <typename Type>
inline bool operator<(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return (lhs <=> rhs) == std::strong_ordering::less;
}

template <typename Type>
inline bool operator<=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return (lhs <=> rhs) != std::strong_ordering::greater;
}

template <typename Type>
inline bool operator>(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return (lhs <=> rhs) == std::strong_ordering::greater;
}

template <typename Type>
inline bool operator>=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return (lhs <=> rhs) != std::strong_ordering::less;
}
