#include "core/string_pool.h"

#include <gtest/gtest.h>

namespace suzume {
namespace core {
namespace {

TEST(StringPoolTest, InternNewStringReturnsView) {
  StringPool pool;
  auto view = pool.intern("hello");
  EXPECT_EQ(view, "hello");
}

TEST(StringPoolTest, InternDuplicateReturnsSameView) {
  StringPool pool;
  auto view1 = pool.intern("test");
  auto view2 = pool.intern("test");
  EXPECT_EQ(view1.data(), view2.data());
  EXPECT_EQ(view1, view2);
}

TEST(StringPoolTest, InternDifferentStringsReturnsDifferentViews) {
  StringPool pool;
  auto view1 = pool.intern("alpha");
  auto view2 = pool.intern("beta");
  EXPECT_NE(view1.data(), view2.data());
  EXPECT_EQ(view1, "alpha");
  EXPECT_EQ(view2, "beta");
}

TEST(StringPoolTest, InternUtf8String) {
  StringPool pool;
  auto view = pool.intern("\xe6\x9d\xb1\xe4\xba\xac");  // Tokyo
  EXPECT_EQ(view, "\xe6\x9d\xb1\xe4\xba\xac");
}

TEST(StringPoolTest, SizeInitiallyZero) {
  StringPool pool;
  EXPECT_EQ(pool.size(), 0u);
}

TEST(StringPoolTest, SizeIncrementsOnNewString) {
  StringPool pool;
  pool.intern("one");
  EXPECT_EQ(pool.size(), 1u);
  pool.intern("two");
  EXPECT_EQ(pool.size(), 2u);
}

TEST(StringPoolTest, SizeDoesNotIncrementOnDuplicate) {
  StringPool pool;
  pool.intern("same");
  pool.intern("same");
  EXPECT_EQ(pool.size(), 1u);
}

TEST(StringPoolTest, MemoryUsagePositiveAfterAdding) {
  StringPool pool;
  pool.intern("some string data");
  EXPECT_GT(pool.memoryUsage(), 0u);
}

TEST(StringPoolTest, MemoryUsageZeroWhenEmpty) {
  StringPool pool;
  EXPECT_EQ(pool.memoryUsage(), 0u);
}

TEST(StringPoolTest, ClearResetsSizeToZero) {
  StringPool pool;
  pool.intern("first");
  pool.intern("second");
  EXPECT_EQ(pool.size(), 2u);
  pool.clear();
  EXPECT_EQ(pool.size(), 0u);
}

TEST(StringPoolTest, ClearAllowsReIntern) {
  StringPool pool;
  pool.intern("word");
  pool.clear();
  auto view = pool.intern("word");
  EXPECT_EQ(view, "word");
  EXPECT_EQ(pool.size(), 1u);
}

TEST(StringPoolTest, InternEmptyString) {
  StringPool pool;
  auto view = pool.intern("");
  EXPECT_TRUE(view.empty());
  EXPECT_EQ(pool.size(), 1u);
}

}  // namespace
}  // namespace core
}  // namespace suzume
