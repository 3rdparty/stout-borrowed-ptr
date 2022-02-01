#include "stout/borrowed_ptr.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stout/borrowable.h"

using std::atomic;
using std::function;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;

using stout::Borrowable;
using stout::borrowed_ptr;

using testing::_;
using testing::MockFunction;

TEST(BorrowTest, Borrow) {
  Borrowable<string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
      .Times(1);

  borrowed_ptr<string> borrowed = s.Borrow();

  // NOTE: after a move we only expect a single relinquish!
  borrowed_ptr<string> moved = std::move(borrowed);

  EXPECT_EQ("hello world", *moved);

  s.Watch(mock.AsStdFunction());
}


TEST(BorrowTest, ConstBorrow) {
  Borrowable<const string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
      .Times(1);

  borrowed_ptr<const string> borrowed = s.Borrow();

  s.Watch(mock.AsStdFunction());
}


TEST(BorrowTest, Reborrow) {
  Borrowable<string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
      .Times(1);

  borrowed_ptr<string> borrowed = s.Borrow();

  s.Watch(mock.AsStdFunction());

  borrowed_ptr<string> reborrow = borrowed.reborrow();

  EXPECT_TRUE(reborrow);
}


TEST(BorrowTest, Emplace) {
  struct S {
    S(borrowed_ptr<int> i)
      : i_(std::move(i)) {}

    S(const S& that)
      : i_(that.i_.reborrow()) {}

    borrowed_ptr<int> i_;
  };

  Borrowable<int> i(42);

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
      .Times(1);

  vector<Borrowable<S>> vector;

  vector.emplace_back(i.Borrow());

  i.Watch(mock.AsStdFunction());
}


TEST(BorrowTest, MultipleBorrows) {
  Borrowable<string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
      .Times(0);

  vector<borrowed_ptr<string>> borrows;

  borrows.push_back(s.Borrow());
  borrows.push_back(s.Borrow());
  borrows.push_back(s.Borrow());
  borrows.push_back(s.Borrow());

  s.Watch(mock.AsStdFunction());

  vector<thread> threads;

  atomic<bool> wait(true);

  while (!borrows.empty()) {
    threads.push_back(thread([&wait, borrowed = std::move(borrows.back())]() {
      while (wait.load()) {}
      // ... destructor will invoke borrowed.relinquish().
    }));

    borrows.pop_back();
  }

  EXPECT_CALL(mock, Call())
      .Times(1);

  wait.store(false);

  for (auto&& thread : threads) {
    thread.join();
  }
}


TEST(BorrowTest, MultipleConstBorrows) {
  Borrowable<const string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
      .Times(0);

  vector<borrowed_ptr<const string>> borrows;

  borrows.push_back(s.Borrow());
  borrows.push_back(s.Borrow());
  borrows.push_back(s.Borrow());
  borrows.push_back(s.Borrow());

  s.Watch(mock.AsStdFunction());

  vector<thread> threads;

  atomic<bool> wait(true);

  while (!borrows.empty()) {
    threads.push_back(thread([&wait, borrowed = std::move(borrows.back())]() {
      while (wait.load()) {}
      // ... destructor will invoke borrowed.relinquish().
    }));

    borrows.pop_back();
  }

  EXPECT_CALL(mock, Call())
      .Times(1);

  wait.store(false);

  for (auto&& thread : threads) {
    thread.join();
  }
}


TEST(BorrowTest, BorrowedPtrUpcast) {
  struct Base {
    int i = 42;
  };

  struct Derived : public Base {};

  Borrowable<Derived> derived;

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
      .Times(1);

  borrowed_ptr<Base> base = derived.Borrow();

  base->i++;

  EXPECT_EQ(43, base->i);

  derived.Watch(mock.AsStdFunction());
}


TEST(BorrowTest, BorrowedPtrConstUpcast) {
  struct Base {
    int i = 42;
  };

  struct Derived : public Base {};

  Borrowable<Derived> derived;

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
      .Times(1);

  borrowed_ptr<const Base> base = derived.Borrow();

  EXPECT_EQ(42, base->i);

  derived.Watch(mock.AsStdFunction());
}

TEST(BorrowTest, ConstBorrowedPtrUpcast) {
  struct Base {
    int i = 42;
  };

  struct Derived : public Base {};

  Borrowable<const Derived> derived;

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
      .Times(1);

  borrowed_ptr<const Base> base = derived.Borrow();

  EXPECT_EQ(42, base->i);

  derived.Watch(mock.AsStdFunction());
}
