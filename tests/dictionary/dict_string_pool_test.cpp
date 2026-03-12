#include "dictionary/string_pool.h"

#include <gtest/gtest.h>

namespace suzume {
namespace dictionary {
namespace {

TEST(DictStringPoolTest, AddAndGetString) {
  DictStringPool pool;
  uint32_t idx = pool.add("hello");
  EXPECT_EQ(pool.get(idx), "hello");
}

TEST(DictStringPoolTest, AddMultipleStrings) {
  DictStringPool pool;
  uint32_t id0 = pool.add("alpha");
  uint32_t id1 = pool.add("beta");
  uint32_t id2 = pool.add("gamma");
  EXPECT_EQ(id0, 0u);
  EXPECT_EQ(id1, 1u);
  EXPECT_EQ(id2, 2u);
  EXPECT_EQ(pool.get(id0), "alpha");
  EXPECT_EQ(pool.get(id1), "beta");
  EXPECT_EQ(pool.get(id2), "gamma");
}

TEST(DictStringPoolTest, AddUtf8String) {
  DictStringPool pool;
  uint32_t idx = pool.add("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");  // test
  EXPECT_EQ(pool.get(idx), "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
}

TEST(DictStringPoolTest, GetInvalidIdReturnsEmpty) {
  DictStringPool pool;
  EXPECT_TRUE(pool.get(0).empty());
  EXPECT_TRUE(pool.get(999).empty());
}

TEST(DictStringPoolTest, GetInvalidIdAfterAdd) {
  DictStringPool pool;
  pool.add("item");
  EXPECT_TRUE(pool.get(1).empty());
  EXPECT_TRUE(pool.get(100).empty());
}

TEST(DictStringPoolTest, SizeInitiallyZero) {
  DictStringPool pool;
  EXPECT_EQ(pool.size(), 0u);
}

TEST(DictStringPoolTest, SizeIncrementsOnAdd) {
  DictStringPool pool;
  pool.add("one");
  EXPECT_EQ(pool.size(), 1u);
  pool.add("two");
  EXPECT_EQ(pool.size(), 2u);
}

TEST(DictStringPoolTest, MemoryUsagePositive) {
  DictStringPool pool;
  pool.add("some data");
  EXPECT_GT(pool.memoryUsage(), 0u);
}

TEST(DictStringPoolTest, ClearResetsPool) {
  DictStringPool pool;
  pool.add("first");
  pool.add("second");
  pool.clear();
  EXPECT_EQ(pool.size(), 0u);
  EXPECT_TRUE(pool.get(0).empty());
}

TEST(DictStringPoolTest, SerializeAndLoadRoundtrip) {
  DictStringPool pool;
  pool.add("alpha");
  pool.add("beta");
  pool.add("\xe6\x9d\xb1\xe4\xba\xac");  // Tokyo

  auto data = pool.serialize();
  EXPECT_FALSE(data.empty());

  DictStringPool loaded;
  bool result = loaded.loadFromMemory(data.data(), data.size());
  EXPECT_TRUE(result);
  EXPECT_EQ(loaded.size(), 3u);
  EXPECT_EQ(loaded.get(0), "alpha");
  EXPECT_EQ(loaded.get(1), "beta");
  EXPECT_EQ(loaded.get(2), "\xe6\x9d\xb1\xe4\xba\xac");
}

TEST(DictStringPoolTest, SerializeEmptyPool) {
  DictStringPool pool;
  auto data = pool.serialize();

  DictStringPool loaded;
  bool result = loaded.loadFromMemory(data.data(), data.size());
  EXPECT_TRUE(result);
  EXPECT_EQ(loaded.size(), 0u);
}

TEST(DictStringPoolTest, LoadFromMemoryTooSmall) {
  DictStringPool pool;
  char tiny[4] = {0};
  EXPECT_FALSE(pool.loadFromMemory(tiny, sizeof(tiny)));
}

TEST(DictStringPoolTest, LoadFromMemoryTruncatedData) {
  DictStringPool original;
  original.add("hello");
  original.add("world");

  auto data = original.serialize();
  // Truncate to header only (8 bytes for count + data_size)
  DictStringPool pool;
  EXPECT_FALSE(pool.loadFromMemory(data.data(), 8));
}

TEST(DictStringPoolTest, LoadFromMemoryZeroSize) {
  DictStringPool pool;
  EXPECT_FALSE(pool.loadFromMemory(nullptr, 0));
}

TEST(DictStringPoolTest, AddEmptyString) {
  DictStringPool pool;
  uint32_t idx = pool.add("");
  EXPECT_TRUE(pool.get(idx).empty());
  EXPECT_EQ(pool.size(), 1u);
}

}  // namespace
}  // namespace dictionary
}  // namespace suzume
