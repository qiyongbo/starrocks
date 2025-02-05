// This file is licensed under the Elastic License 2.0. Copyright 2021 StarRocks Limited.

#pragma once
#include <memory>
#include <type_traits>
namespace starrocks {
template <typename T>
// exclusive_ptr has the exclusive ownership of the underlying object, when copy
// ctor/assigment operator is invoked, the object underlying is moved into the
// target exclusive. exclusive_ptr is used in ownership capture in a lambda
// executed by another thread, exclusive_ptr is different from unique_ptr in two
// points:
// 1. unique_ptr also has the exclusive ownership of the underlying object, but
// copy ctor/assignment operator is deleted.
//
// 2. object captured by lambda is const modified, so methods of exclusive_ptr
// is also const modified and the underlying object returned is mutable.
// so exclusive_ptr is proper for lambda's ownership capture.
class exclusive_ptr {
    using type = T;
    using pointer_type = std::unique_ptr<type>;

public:
    constexpr exclusive_ptr() {}
    constexpr exclusive_ptr(nullptr_t) {}
    explicit exclusive_ptr(pointer_type&& v) : _value(v.release()) {}
    exclusive_ptr(const exclusive_ptr& other) : _value(other.release()) {}
    exclusive_ptr(exclusive_ptr&& other) : _value(other.release()) {}

    template <typename U>
    exclusive_ptr(exclusive_ptr<U>&& other) : _value(other.release()) {}

    template <typename U>
    exclusive_ptr(const exclusive_ptr<U>& other) : _value(other.release()) {}

    exclusive_ptr& operator=(const exclusive_ptr& other) {
        this->reset(other.release());
        return *this;
    }
    exclusive_ptr& operator=(exclusive_ptr&& other) {
        this->reset(other.release());
        return *this;
    }

    template <typename U>
    exclusive_ptr& operator=(exclusive_ptr<U>&& other) {
        this->reset(other.release());
        return *this;
    }
    template <typename U>
    exclusive_ptr& operator=(const exclusive_ptr<U>& other) {
        this->reset(other.release());
        return *this;
    }

    exclusive_ptr& operator=(nullptr_t) {
        this->_value = nullptr;
        return *this;
    }
    explicit operator bool() { return _value.get() != nullptr; }
    type* get() const { return _value.get(); }
    void reset() const { return _value.reset(); }
    void reset(type* ptr) const { return _value.reset(ptr); }
    type* release() const { return _value.release(); }
    type* operator->() const { return _value.get(); }
    type& operator*() const { return *_value; }

private:
    mutable pointer_type _value;
};

template <typename T, typename... Args>
static inline exclusive_ptr<T> make_exclusive(Args&&... args) {
    return exclusive_ptr<T>(std::make_unique<T>(std::forward<Args>(args)...));
}
} // namespace starrocks