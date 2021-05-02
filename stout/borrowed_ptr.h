#pragma once

#include <functional>

namespace stout {

template <typename T>
class borrowable;

template <typename T>
class borrowed_ptr
{
public:
  borrowed_ptr() {}

  borrowed_ptr(const borrowed_ptr& that)
    : borrowed_ptr(that.reborrow()) {}

  borrowed_ptr(borrowed_ptr&& that)
  {
    std::swap(borrowable_, that.borrowable_);
  }

  ~borrowed_ptr()
  {
    relinquish();
  }

  template <typename H>
  friend H AbslHashValue(H h, const borrowed_ptr& that)
  {
    return H::combine(std::move(h), that.t_);
  }

  borrowed_ptr reborrow() const
  {
    if (borrowable_ != nullptr) {
      return borrowable_->reborrow();
    } else {
      return borrowed_ptr<T>();
    }
  }

  void relinquish()
  {
    if (borrowable_ != nullptr) {
      borrowable_->relinquish();
      borrowable_ = nullptr;
    }
  }

  T* get() const
  {
    if (borrowable_ != nullptr) {
      return borrowable_->get();
    } else {
      return nullptr;
    }
  }

  T* operator->() const
  {
    assert(borrowable_ != nullptr);
    return borrowable_->operator->();
  }

  T& operator*() const
  {
    assert(borrowable_ != nullptr);
    return borrowable_->operator*();
  }

  operator bool() const
  {
    return borrowable_ != nullptr;
  }

  // TODO(benh): operator[]

private:
  template <typename>
  friend class borrowable;

  borrowed_ptr(borrowable<T>* borrowable)
    : borrowable_(borrowable) {}

  borrowable<T>* borrowable_ = nullptr;
};

} // namespace stout {
