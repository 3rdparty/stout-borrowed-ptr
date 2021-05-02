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

using stout::borrowable;
using stout::borrowed_ptr;

using testing::_;
using testing::MockFunction;


TEST(BorrowTest, Borrow)
{
  borrowable<string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
    .Times(1);

  borrowed_ptr<string> borrowed = s.borrow();

  s.watch(mock.AsStdFunction());
}


TEST(BorrowTest, ConstBorrow)
{
  borrowable<const string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
    .Times(1);

  borrowed_ptr<const string> borrowed = s.borrow();

  s.watch(mock.AsStdFunction());
}


TEST(BorrowTest, Reborrow)
{
  borrowable<string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
    .Times(1);

  borrowed_ptr<string> borrowed = s.borrow();

  s.watch(mock.AsStdFunction());

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

  borrowable<int> i(42);

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
    .Times(1);

  vector<borrowable<S>> vector;

  vector.emplace_back(i.borrow());

  i.watch(mock.AsStdFunction());
}


TEST(BorrowTest, MultipleBorrows)
{
  borrowable<string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
    .Times(0);

  vector<borrowed_ptr<string>> borrows;

  borrows.push_back(s.borrow());
  borrows.push_back(s.borrow());
  borrows.push_back(s.borrow());
  borrows.push_back(s.borrow());

  s.watch(mock.AsStdFunction());

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
  borrowable<const string> s("hello world");

  MockFunction<void()> mock;

  EXPECT_CALL(mock, Call())
    .Times(0);

  vector<borrowed_ptr<const string>> borrows;

  borrows.push_back(s.borrow());
  borrows.push_back(s.borrow());
  borrows.push_back(s.borrow());
  borrows.push_back(s.borrow());

  s.watch(mock.AsStdFunction());

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
