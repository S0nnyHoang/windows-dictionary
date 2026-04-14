#pragma once
// Tiny IUnknown smart pointer. We don't pull in WRL or wil so the binary stays slim.

#include <utility>

template <typename T>
class ComPtr {
public:
    ComPtr() noexcept = default;
    ComPtr(std::nullptr_t) noexcept {}
    ComPtr(const ComPtr& other) noexcept : p_(other.p_) { if (p_) p_->AddRef(); }
    ComPtr(ComPtr&& other) noexcept : p_(other.p_) { other.p_ = nullptr; }
    ~ComPtr() { Reset(); }

    ComPtr& operator=(const ComPtr& other) noexcept {
        if (this != &other) {
            Reset();
            p_ = other.p_;
            if (p_) p_->AddRef();
        }
        return *this;
    }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Reset();
            p_ = other.p_;
            other.p_ = nullptr;
        }
        return *this;
    }
    ComPtr& operator=(std::nullptr_t) noexcept { Reset(); return *this; }

    T* Get() const noexcept { return p_; }
    T* operator->() const noexcept { return p_; }
    T& operator*() const noexcept { return *p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }

    // For passing to functions that take T**.
    T** GetAddressOf() noexcept { return &p_; }
    T** ReleaseAndGetAddressOf() noexcept { Reset(); return &p_; }
    void** PutVoid() noexcept { Reset(); return reinterpret_cast<void**>(&p_); }

    void Reset() noexcept {
        if (p_) {
            p_->Release();
            p_ = nullptr;
        }
    }

    friend bool operator==(const ComPtr& p, std::nullptr_t) noexcept { return p.p_ == nullptr; }
    friend bool operator==(std::nullptr_t, const ComPtr& p) noexcept { return p.p_ == nullptr; }
    friend bool operator!=(const ComPtr& p, std::nullptr_t) noexcept { return p.p_ != nullptr; }
    friend bool operator!=(std::nullptr_t, const ComPtr& p) noexcept { return p.p_ != nullptr; }

    T* Detach() noexcept {
        T* tmp = p_;
        p_ = nullptr;
        return tmp;
    }

    void Attach(T* raw) noexcept {
        Reset();
        p_ = raw;
    }

private:
    T* p_ = nullptr;
};
