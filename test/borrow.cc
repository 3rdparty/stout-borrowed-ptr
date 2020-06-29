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


TEST(BorrowTest, FromT)
{
  string s = "hello world";

  MockFunction<void(string*)> mock;

  EXPECT_CALL(mock, Call(&s))
    .Times(1);

  borrowed_ptr<string> borrowed = borrow(&s, mock.AsStdFunction());
}


TEST(BorrowTest, FromUnique)
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


TEST(BorrowTest, Multiple)
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
