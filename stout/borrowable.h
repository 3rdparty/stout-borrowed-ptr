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
  bool watch(F&& f)
  {
    auto [state, count] = tally_.wait([](auto, auto) { return true; });

    do {
      if (state == State::Watching) {
        return false;
      } else if (count == 0) {
        f();
        return true;
      }

      assert(state == State::Borrowing);

    } while (!tally_.update(state, count, State::Watching, count + 1));

    watch_ = std::move(f);

    relinquish();

    return true;
  }

  borrowed_ptr<T> borrow()
  {
    auto state = State::Borrowing;
    if (tally_.increment(state)) {
      return borrowed_ptr<T>(this);
    } else {
      return borrowed_ptr<T>();
    }
  }

  T* get() { return &t_; }

  T* operator->() { return get(); }

  T& operator*() { return t_; }

private:
  template <typename>
  friend class borrowed_ptr;

  void relinquish()
  {
    auto [state, count] = tally_.decrement();

    if (state == State::Watching && count == 0) {
      // Move out 'watch_' in case it gets reset either in the
      // callback or because a concurrent call to 'borrow()' occurs
      // after we've updated the tally below.
      auto f = std::move(watch_);
      watch_ = std::function<void()>();

      tally_.update(state, State::Borrowing);

      // At this point a call to 'borrow()' may mean that there are
      // outstanding borrowed_ptr's when the watch callback gets
      // invoked and thus it's up to the users of this abstraction to
      // avoid making calls to 'borrow()' until after the watch
      // callback gets invoked if they want to guarantee that there
      // are no outstanding borrowed_ptr's.

      f();
    }
  }

  borrowed_ptr<T> reborrow()
  {
    auto [state, count] = tally_.wait([](auto, auto) { return true; });

    assert(count > 0);

    do {
      assert(state != State::Waiting);
    } while (!tally_.increment(state));

    return borrowed_ptr<T>(this);
  }

  T t_;

  enum class State : uint8_t {
    Borrowing,
    Watching,
    Waiting,
  };

  // NOTE: 'stateful_tally' ensures this is non-moveable (but still
  // copyable). What would it mean to be able to borrow a pointer to
  // something that might move!? If an implemenetation ever replaces
  // 'stateful_tally' with something else care will need to be taken
  // to ensure that 'borrowable' doesn't become moveable.
  stateful_tally<State> tally_;

  std::function<void()> watch_;
};

} // namespace stout {
