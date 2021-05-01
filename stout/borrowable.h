#pragma once

#include "stout/borrowed_ptr.h"
#include "stout/stateful-tally.h"

namespace stout {

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
template <typename T>
class borrowable
{
public:
  borrowable(T t)
    : t_(std::move(t)), tally_(State::Borrowing) {}

  template <typename... Args>
  borrowable(Args&&... args)
    : t_(std::forward<Args>(args)...), tally_(State::Borrowing) {}

  borrowable(const borrowable& that)
    : t_(that.t_), tally_(State::Borrowing) {}

  ~borrowable()
  {
    auto state = State::Borrowing;
    if (!tally_.update(state, State::Waiting)) {
      std::abort(); // Invalid state encountered.
    } else {
      // // NOTE: it's possible that we'll block forever if exceptions
      // // were thrown and destruction was not successful.
      // if (!std::uncaught_exceptions() > 0) {
        tally_.wait([](auto /* state */, auto count) {
          return count == 0;
        });
      // }
    }
  }

  template <typename F>
  borrowed_ptr<T> borrow(F f)
  {
    auto state = State::Borrowing;
    if (tally_.increment(state)) {
      return stout::borrow(&t_, [this, f = std::move(f)](auto* t) {
        tally_.decrement();
        f(t);
      });
    }
    return borrowed_ptr<T>();
  }

  borrowed_ptr<T> borrow()
  {
    return borrow([](auto*) {});
  }

  T* get() { return &t_; }

  T* operator->() { return get(); }

  T& operator*() { return t_; }

private:
  T t_;

  enum class State : uint8_t {
    Borrowing,
    Waiting,
  };

  stateful_tally<State> tally_;
};

} // namespace stout {
