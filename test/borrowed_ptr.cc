#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

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

  s.Watch(mock.AsStdFunction());
}


TEST(BorrowTest, ConstBorrow)
{
  Borrowable<const string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
    .Times(1);

  borrowed_ptr<const string> borrowed = s.Borrow();

  s.Watch(mock.AsStdFunction());
}


TEST(BorrowTest, Reborrow)
{
  Borrowable<string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
    .Times(1);

  borrowed_ptr<string> borrowed = s.Borrow();

  s.Watch(mock.AsStdFunction());

  borrowed_ptr<string> reborrow = borrowed.reborrow();

  EXPECT_TRUE(reborrow);
}


TEST(BorrowTest, Emplace)
{
  struct S
  {
    S(borrowed_ptr<int> i) : i_(std::move(i)) {}

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


TEST(BorrowTest, MultipleBorrows)
{
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


TEST(BorrowTest, MultipleConstBorrows)
{
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
