#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace stout {

template <typename T>
class borrowed_ptr
{
public:
  borrowed_ptr() {}

  template <typename U, typename F>
  borrowed_ptr(U* u, F&& f)
    : t_(u, std::forward<F>(f)) {}

  // NOTE: requires 'Deleter' to be copyable (for now).
  template <typename U, typename Deleter, typename F>
  borrowed_ptr(std::unique_ptr<U, Deleter>&& u, F&& f)
    : borrowed_ptr(
        u.release(),
        [deleter = u.get_deleter(), f = std::forward<F>(f)](auto* u) {
          f(std::unique_ptr<U, Deleter>(u, deleter));
        }) {}

  // NOTE: requires 'Deleter' to be copyable (for now).
  template <typename U, typename Deleter>
  borrowed_ptr(std::unique_ptr<U, Deleter>&& u)
    : borrowed_ptr(std::move(u), [](auto&&) {}) {}

  template <typename H>
  friend H AbslHashValue(H h, const borrowed_ptr& that)
  {
    return H::combine(std::move(h), that.t_);
  }

  void relinquish()
  {
    t_.reset();
  }

  T* get() const
  {
    return t_.get();
  }

  T* operator->() const
  {
    return t_.operator->();
  }

  T& operator*() const
  {
    return t_.operator*();
  }

  operator bool() const
  {
    return t_.operator bool();
  }

  // TODO(benh): operator[]

private:
  std::unique_ptr<T, std::function<void(T*)>> t_;
};


template <typename T, typename F>
borrowed_ptr<T> borrow(T* t, F&& f)
{
  return borrowed_ptr<T>(t, std::forward<F>(f));
}


template <typename T, typename Deleter, typename F>
borrowed_ptr<T> borrow(std::unique_ptr<T, Deleter>&& t, F&& f)
{
  return borrowed_ptr<T>(std::move(t), std::forward<F>(f));
}


template <typename T, typename Deleter>
borrowed_ptr<T> borrow(std::unique_ptr<T, Deleter>&& t)
{
  return borrowed_ptr<T>(std::move(t));
}


template <typename T, typename F>
std::vector<borrowed_ptr<T>> borrow(size_t n, T* t, F&& f)
{
  auto relinquish = [n = new std::atomic<size_t>(n), f = std::forward<F>(f)](T* t) {
    if (n->fetch_sub(1) == 1) {
      f(t);
      delete n;
    }
  };

  std::vector<borrowed_ptr<T>> result;

  result.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    result.push_back(borrow(t, relinquish));
  }

  return result;
}


template <typename T, typename Deleter, typename F>
std::vector<borrowed_ptr<T>> borrow(size_t n, std::unique_ptr<T, Deleter>&& t, F&& f)
{
  return borrow(
      n,
      t.release(),
      [deleter = t.get_deleter(), f = std::forward<F>(f)](auto* t) {
        f(std::unique_ptr<T, Deleter>(t, deleter));
      });
}


template <typename T, typename Deleter>
std::vector<borrowed_ptr<T>> borrow(size_t n, std::unique_ptr<T>&& t)
{
  return borrow(n, std::move(t), [](auto&&) {});
}

} // namespace stout {
