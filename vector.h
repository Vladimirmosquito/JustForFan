#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <stdexcept>
#include <algorithm>
template <typename T>
class RawMemory {
public:

    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        //rhs.Deallocate();
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};



template <typename T>
class Vector {
private:
    RawMemory<T> data_;
    size_t size_ = 0;

public:
    using iterator = T*;
    using const_iterator = const T*;
    Vector() = default;
    Vector(size_t size)
        : data_(size),
        size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_),
        size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other)
        :data_(std::move(other.data_)),
        size_(std::exchange(other.size_, 0))
    {
    }


    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }             // случай если размер нашего вектора больше чем копируемого, то надо удалить лишние                
            else if (rhs.size_ <= size_) {
                size_t i = 0;
                for (; i != rhs.size_; ++i) {
                    data_[i] = rhs.data_[i];
                }
                DestroyN(data_ + i, size_ - i);
                size_ = rhs.size_;

            }        // случай если размер нашего вектора меньше чем копируемого, то надо скопировать недостающее
            else if (rhs.size_ > size_) {
                size_t i = 0;
                for (; i != size_; ++i) {
                    data_[i] = rhs.data_[i];
                }
                std::uninitialized_copy_n(rhs.data_ + i, rhs.size_ - i, data_ + i);
                size_ = rhs.size_;
            }

        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_.Swap(rhs.data_);
        std::swap(size_, rhs.size_);
        return *this;
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_ + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) return;

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (Capacity() == 0) {
            RawMemory<T> new_data(1);
            new(new_data.GetAddress()) T(std::forward<Args>(args)...);
            data_.Swap(new_data);
            ++size_;
            return data_[size_ - 1];
        }
        if (Capacity() == size_) {
            RawMemory<T> new_data(Capacity() * 2);
            new(new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
            return data_[size_ - 1];
        }
        else {
            new(data_.GetAddress() + size_) T(std::forward<Args>(args)...);
            ++size_;
            return data_[size_ - 1];
        }
    }

    void Resize(size_t new_size) {
        if (new_size == size_) return;
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, Capacity() - size_);
            size_ = Capacity();
        }
        else {
            std::destroy_n(data_ + new_size, size_ - new_size);
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() {
        Destroy(data_ + size_ - 1);
        --size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t position = std::distance(cbegin(), pos);
        if (pos == cend()) {
            EmplaceBack(std::forward<Args>(args)...);
        }
        else if (Capacity() == 0) {
            RawMemory<T> new_data(1);
            new(new_data.GetAddress()) T(std::forward<Args>(args)...);
            data_.Swap(new_data);
            ++size_;
        }
        else if (size_ == Capacity()) {
            RawMemory<T> new_data(Capacity() * 2);
            new(new_data + position) T(std::forward<Args>(args)...);
            // переносим элементы которые находится перед pos
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), position, new_data.GetAddress());
                }
                else {
                    std::uninitialized_copy_n(data_.GetAddress(), position, new_data.GetAddress());
                }
            }
            catch (...) {
                std::destroy_at(new_data + position);
                throw;
            }
            // переносим элементы которые находятся после pos
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(iterator(pos), size_ - position, std::next(new_data.GetAddress() + position));
                }
                else {
                    std::uninitialized_copy_n(iterator(pos), size_ - position, std::next(new_data.GetAddress() + position));
                }
            }
            catch (...) {
                std::destroy(new_data.GetAddress(), new_data + position);
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;

        }
        else {
            T object(std::forward<Args>(args)...);
            new(end()) T(std::move(*(end() - 1)));
            std::move_backward(iterator(pos), end() - 1, end());
            *(iterator(pos)) = std::move(object);
            ++size_;
        }
        return iterator(begin() + position);
    }

    iterator Erase(const_iterator pos)  {
        std::move(std::next(iterator(pos)), end(), iterator(pos));
        Destroy(end() - 1);
        --size_;
        return iterator(pos);
    }

    iterator Insert(const_iterator pos, const T& value) {
        if (size_ != 0) {
            if (
                std::addressof(value) >= std::addressof(data_[0])
                && std::addressof(value) <= std::addressof(data_[size_ - 1])) {
                return Emplace(pos, T(value));
            }
        }
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:

    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

};