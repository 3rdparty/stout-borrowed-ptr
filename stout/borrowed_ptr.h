#pragma once

#include <functional>

#include "stout/stateful-tally.h"

////////////////////////////////////////////////////////////////////////

namespace stout {

////////////////////////////////////////////////////////////////////////

// Forward dependency.
template <typename T>
class borrowed_ptr;

////////////////////////////////////////////////////////////////////////

// NOTE: currently this implementation of Borrowable does an atomic
// backoff instead of blocking the thread when the destructor waits
// for all borrows to be relinquished. This will be much less
// efficient (and hold up a CPU) if the borrowers take a while to
// relinquish. However, since Borrowable will mostly be used in
// cirumstances where the tally is definitely back to 0 when we wait
// no backoff will occur. For circumstances where Borrowable is being
// used to wait until work is completed consider using a Notification
// to be notified when the work is complete and then Borrowable should
// destruct without any atomic backoff (because any workers/threads
// will have relinquished).
class TypeErasedBorrowable {
 public:
  template <typename F>
  bool Watch(F&& f) {
    auto [state, count] = tally_.Wait([](auto, size_t) { return true; });

    do {
      if (state == State::Watching) {
        return false;
      } else if (count == 0) {
        f();
        return true;
      }

      assert(state == State::Borrowing);

    } while (!tally_.Update(state, count, State::Watching, count + 1));

    watch_ = std::move(f);

    Relinquish();

    return true;
  }

  size_t borrows() {
    return tally_.count();
  }

  void Relinquish() {
    auto [state, count] = tally_.Decrement();

    if (state == State::Watching && count == 0) {
      // Move out 'watch_' in case it gets reset either in the
      // callback or because a concurrent call to 'borrow()' occurs
      // after we've updated the tally below.
      auto f = std::move(watch_);
      watch_ = std::function<void()>();

      tally_.Update(state, State::Borrowing);

      // At this point a call to 'borrow()' may mean that there are
      // outstanding borrowed_ptr's when the watch callback gets
      // invoked and thus it's up to the users of this abstraction to
      // avoid making calls to 'borrow()' until after the watch
      // callback gets invoked if they want to guarantee that there
      // are no outstanding borrowed_ptr's.

      f();
    }
  }

 protected:
  TypeErasedBorrowable()
    : tally_(State::Borrowing) {}

  virtual ~TypeErasedBorrowable() {
    auto state = State::Borrowing;
    if (!tally_.Update(state, State::Destructing)) {
      std::abort(); // Invalid state encountered.
    } else {
      // NOTE: it's possible that we'll block forever if exceptions
      // were thrown and destruction was not successful.
      // if (!std::uncaught_exceptions() > 0) {
      tally_.Wait([](auto /* state */, size_t count) {
        return count == 0;
      });
      // }
    }
  }

  enum class State : uint8_t {
    Borrowing,
    Watching,
    Destructing,
  };

  // NOTE: 'stateful_tally' ensures this is non-moveable (but still
  // copyable). What would it mean to be able to borrow a pointer to
  // something that might move!? If an implemenetation ever replaces
  // 'stateful_tally' with something else care will need to be taken
  // to ensure that 'Borrowable' doesn't become moveable.
  StatefulTally<State> tally_;

  std::function<void()> watch_;

 private:
  // Only 'borrowed_ptr' can reborrow!
  template <typename>
  friend class borrowed_ptr;

  void Reborrow() {
    auto [state, count] = tally_.Wait([](auto, size_t) { return true; });

    assert(count > 0);

    do {
      assert(state != State::Destructing);
    } while (!tally_.Increment(state));
  }
};

////////////////////////////////////////////////////////////////////////

template <typename T>
class Borrowable : public TypeErasedBorrowable {
 public:
  // Helper type that is callable and handles ensuring 'this' is
  // borrowed until the callback is destructed.
  template <typename F>
  class Callable final {
   public:
    Callable(F f, borrowed_ptr<T> t)
      : f_(std::move(f)),
        t_(std::move(t)) {}

    Callable(Callable&& that) = default;

    template <typename... Args>
    decltype(auto) operator()(Args&&... args) const& {
      return f_(std::forward<Args>(args)...);
    }

    template <typename... Args>
    decltype(auto) operator()(Args&&... args) && {
      return std::move(f_)(std::forward<Args>(args)...);
    }

   private:
    F f_;
    borrowed_ptr<T> t_;
  };

  template <
      typename... Args,
      std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
  Borrowable(Args&&... args)
    : t_(std::forward<Args>(args)...) {}

  Borrowable(const Borrowable& that)
    : t_(that.t_) {}

  Borrowable(Borrowable&& that)
    : t_([&]() {
        // We need to wait until all borrows have been relinquished so
        // any memory associated with 'that' can be safely released.
        that.tally_.Wait([](auto /* state */, size_t count) {
          return count == 0;
        });
        return std::move(that.t_);
      }()) {}

  borrowed_ptr<T> Borrow() {
    auto state = State::Borrowing;
    if (tally_.Increment(state)) {
      return borrowed_ptr<T>(this, &t_);
    } else {
      return borrowed_ptr<T>();
    }
  }

  template <typename F>
  Callable<F> Borrow(F&& f) {
    auto state = State::Borrowing;
    if (tally_.Increment(state)) {
      return Callable<F>(std::forward<F>(f), borrowed_ptr<T>(this, &t_));
    } else {
      // TODO(benh): make semantics here consistent with 'Borrow()',
      // likely this means we should always abort (or throw an exception).
      std::abort();
    }
  }

  T* get() {
    return &t_;
  }

  const T* get() const {
    return &t_;
  }

  T* operator->() {
    return get();
  }

  const T* operator->() const {
    return get();
  }

  T& operator*() {
    return t_;
  }

  const T& operator*() const {
    return t_;
  }

 private:
  T t_;
};

////////////////////////////////////////////////////////////////////////

template <typename T>
class borrowed_ptr final {
 public:
  borrowed_ptr() {}

  borrowed_ptr(borrowed_ptr&& that) {
    std::swap(borrowable_, that.borrowable_);
    std::swap(t_, that.t_);
  }

  ~borrowed_ptr() {
    relinquish();
  }

  explicit operator bool() const {
    return borrowable_ != nullptr;
  }

  template <
      typename U,
      std::enable_if_t<
          std::conjunction_v<
              std::negation<std::is_pointer<U>>,
              std::negation<std::is_reference<U>>,
              std::is_convertible<T*, U*>>,
          int> = 0>
  operator borrowed_ptr<U>() const {
    if (borrowable_ != nullptr) {
      borrowable_->Reborrow();
      return borrowed_ptr<U>(borrowable_, t_);
    } else {
      return borrowed_ptr<U>();
    }
  }

  borrowed_ptr reborrow() const {
    if (borrowable_ != nullptr) {
      borrowable_->Reborrow();
      return borrowed_ptr<T>(borrowable_, t_);
    } else {
      return borrowed_ptr<T>();
    }
  }

  void relinquish() {
    if (borrowable_ != nullptr) {
      borrowable_->Relinquish();
      borrowable_ = nullptr;
      t_ = nullptr;
    }
  }

  T* get() const {
    return t_;
  }

  T* operator->() const {
    return get();
  }

  T& operator*() const {
    // NOTE: just like with 'std::unique_ptr' the behavior is
    // undefined if 'get() == nullptr'.
    return *get();
  }

  // TODO(benh): operator[]

  template <typename H>
  friend H AbslHashValue(H h, const borrowed_ptr& that) {
    return H::combine(std::move(h), that.t_);
  }

 private:
  template <typename>
  friend class borrowed_ptr;

  template <typename>
  friend class Borrowable;

  borrowed_ptr(TypeErasedBorrowable* borrowable, T* t)
    : borrowable_(borrowable),
      t_(t) {}

  TypeErasedBorrowable* borrowable_ = nullptr;
  T* t_ = nullptr;
};

////////////////////////////////////////////////////////////////////////

} // namespace stout

////////////////////////////////////////////////////////////////////////
