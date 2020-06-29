#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "stout/borrowed_ptr.h"

using std::atomic;
using std::function;
using std::string;
using std::thread;
using std::unique_ptr;
using std::vector;

using stout::borrow;
using stout::borrowed_ptr;

using testing::_;
using testing::MockFunction;


TEST(BorrowTest, Borrow)
{
  string s = "hello world";

  MockFunction<void(string*)> mock;

  EXPECT_CALL(mock, Call(&s))
    .Times(1);

  borrowed_ptr<string> borrowed = borrow(&s, mock.AsStdFunction());
}


TEST(BorrowTest, ConstBorrow)
{
  string s = "hello world";

  MockFunction<void(const string*)> mock;

  EXPECT_CALL(mock, Call(&s))
    .Times(1);

  borrowed_ptr<const string> borrowed = borrow<const string>(&s, mock.AsStdFunction());
}


TEST(BorrowTest, UniqueBorrow)
{
  MockFunction<void(string*)> mock;

  EXPECT_CALL(mock, Call(_))
    .WillOnce([](auto* s) {
      delete s;
    });

  unique_ptr<string, function<void(string*)>> s(
      new string("hello world"),
      mock.AsStdFunction());

  borrowed_ptr<string> borrowed = borrow(std::move(s));
}


TEST(BorrowTest, UniqueWithFunctionBorrow)
{
  MockFunction<void(unique_ptr<string>&&)> mock;

  EXPECT_CALL(mock, Call(_))
    .Times(1);

  unique_ptr<string> s(new string("hello world"));

  borrowed_ptr<string> borrowed = borrow(std::move(s), mock.AsStdFunction());
}


TEST(BorrowTest, MultipleBorrows)
{
  string s = "hello world";

  MockFunction<void(string*)> mock;

  EXPECT_CALL(mock, Call(&s))
    .Times(0);

  vector<borrowed_ptr<string>> borrows = borrow(4, &s, mock.AsStdFunction());

  vector<thread> threads;

  atomic<bool> wait(true);

  while (!borrows.empty()) {
    threads.push_back(thread([&wait, borrowed = std::move(borrows.back())]() {
      while (wait.load()) {}
      // ... destructor will invoke borrowed.relinquish().
    }));

    borrows.pop_back();
  }

  EXPECT_CALL(mock, Call(&s))
    .Times(1);

  wait.store(false);

  for (auto&& thread : threads) {
    thread.join();
  }
}


TEST(BorrowTest, MultipleConstBorrows)
{
  string s = "hello world";

  MockFunction<void(const string*)> mock;

  EXPECT_CALL(mock, Call(&s))
    .Times(0);

  vector<borrowed_ptr<const string>> borrows = borrow<const string>(4, &s, mock.AsStdFunction());

  vector<thread> threads;

  atomic<bool> wait(true);

  while (!borrows.empty()) {
    threads.push_back(thread([&wait, borrowed = std::move(borrows.back())]() {
      while (wait.load()) {}
      // ... destructor will invoke borrowed.relinquish().
    }));

    borrows.pop_back();
  }

  EXPECT_CALL(mock, Call(&s))
    .Times(1);

  wait.store(false);

  for (auto&& thread : threads) {
    thread.join();
  }
}


TEST(BorrowTest, MultipleUniqueWithFunctionBorrows)
{
  unique_ptr<string> s(new string("hello world"));

  MockFunction<void(unique_ptr<string>&&)> mock;

  EXPECT_CALL(mock, Call(_))
    .Times(0);

  vector<borrowed_ptr<string>> borrows = borrow<string>(4, std::move(s), mock.AsStdFunction());

  vector<thread> threads;

  atomic<bool> wait(true);

  while (!borrows.empty()) {
    threads.push_back(thread([&wait, borrowed = std::move(borrows.back())]() {
      while (wait.load()) {}
      // ... destructor will invoke borrowed.relinquish().
    }));

    borrows.pop_back();
  }

  EXPECT_CALL(mock, Call(_))
    .Times(1);

  wait.store(false);

  for (auto&& thread : threads) {
    thread.join();
  }
}
