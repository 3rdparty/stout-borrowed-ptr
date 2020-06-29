#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "stout/borrowed_ptr.h"

using std::function;
using std::string;
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

  borrows.pop_back();
  borrows.pop_back();
  borrows.pop_back();

  borrowed_ptr<string> borrowed = std::move(borrows.back());

  borrows.pop_back();

  EXPECT_CALL(mock, Call(&s))
    .Times(1);
}
