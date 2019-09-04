// Copyright 2011, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Björn Buchhold (buchhold@informatik.uni-freiburg.de)

#include <gtest/gtest.h>
#include <string>
#include "../src/util/LRUCache.h"

using std::string;

namespace ad_utility {
// _____________________________________________________________________________
TEST(LRUCacheTest, testSimpleMapUsage) {
  LRUCache<string, string> cache(5);
  cache.insert("1", "x");
  cache.insert("2", "xx");
  cache.insert("3", "xxx");
  cache.insert("4", "xxxx");
  cache.insert("5", "xxxxx");

  ASSERT_EQ(*cache["1"], "x");
  ASSERT_EQ(*cache["2"], "xx");
  ASSERT_EQ(*cache["3"], "xxx");
  ASSERT_EQ(*cache["4"], "xxxx");
  ASSERT_EQ(*cache["5"], "xxxxx");
  // Non-existing elements must yield shared_ptr<const Value>(nullptr)
  // this bool converts to false
  ASSERT_FALSE(cache["non-existant"]);
}
// _____________________________________________________________________________
TEST(LRUCacheTest, testSimpleMapUsageWithDrop) {
  LRUCache<string, string> cache(3);
  cache.insert("1", "x");
  cache.insert("2", "xx");
  cache.insert("3", "xxx");
  cache.insert("4", "xxxx");
  cache.insert("5", "xxxxx");

  ASSERT_FALSE(cache["2"]);         // second oldest dropped
  ASSERT_FALSE(cache["1"]);         // oldest dropped
  ASSERT_EQ(*cache["5"], "xxxxx");  // not dropped
  ASSERT_EQ(*cache["4"], "xxxx");   // not dropped
  ASSERT_EQ(*cache["3"], "xxx");    // not dropped
  cache.insert("6", "xxxxxx");
  ASSERT_FALSE(cache["5"]);          // oldest access driooed
  ASSERT_EQ(*cache["3"], "xxx");     // not dropped
  ASSERT_EQ(*cache["4"], "xxxx");    // not dropped
  ASSERT_EQ(*cache["6"], "xxxxxx");  // not dropped
}
// _____________________________________________________________________________
TEST(LRUCacheTest, testTryEmplace) {
  LRUCache<string, string> cache(5);
  cache.insert("1", "x");
  cache.insert("2", "xx");
  // tryEmplace returns a pair of shared_ptr where the first is non-const and
  // only non-null if it was freshly inserted. That in turn converts to false
  // in bool context
  ASSERT_FALSE(cache.tryEmplace("2", "foo").first);
  ASSERT_EQ(*cache.tryEmplace("4", "bar").first, "bar");
  auto emplaced = cache.tryEmplace("5", "foo").first;
  ASSERT_TRUE(emplaced);
  *emplaced += "bar";
  ASSERT_EQ(*cache["5"], "foobar");
}
// _____________________________________________________________________________
TEST(LRUCacheTest, testTryEmplaceWithDrop) {
  LRUCache<string, string> cache(3);
  cache.insert("1", "x");
  cache.insert("2", "xx");
  ASSERT_FALSE(cache.tryEmplace("2", "foo").first);
  ASSERT_EQ(*cache.tryEmplace("3", "bar").first, "bar");
  auto emplaced = cache.tryEmplace("4", "foo").first;
  ASSERT_TRUE(emplaced);
  *emplaced += "bar";
  ASSERT_EQ(*cache["4"], "foobar");
  ASSERT_FALSE(cache["1"]);      // Dropped oldest
  ASSERT_EQ(*cache["2"], "xx");  // Kept second oldest
}
// _____________________________________________________________________________
TEST(LRUCacheTest, testTryEmplacePinnedSimple) {
  LRUCache<string, string> cache(3);
  ASSERT_EQ(*cache.tryEmplacePinned("pinned", "bar").first, "bar");
  cache.insert("1", "x");
  cache.insert("2", "xx");
  cache.insert("3", "xxx");
  cache.insert("4", "xxxx");
  ASSERT_EQ(*cache["pinned"], "bar");  // pinned still there
  ASSERT_FALSE(cache["1"]);            // oldest already gone
  ASSERT_EQ(*cache["2"], "xx");  // not dropped because pinned not in capacity
  ASSERT_EQ(*cache["pinned"], "bar");  // pinned still there
}
// _____________________________________________________________________________
TEST(LRUCacheTest, testTryEmplacePinnedExisting) {
  LRUCache<string, string> cache(2);
  cache.insert("1", "x");
  cache.insert("2", "xx");
  // tryEmplacePinned on an existing element doesn't emplace but it does pin
  ASSERT_FALSE(cache.tryEmplacePinned("2", "yy").first);
  cache.insert("3", "xxx");
  cache.insert("4", "xxxx");
  cache.insert("5", "xxxxx");
  ASSERT_FALSE(cache["1"]);         // oldest already gone
  ASSERT_EQ(*cache["2"], "xx");     // pinned still there
  ASSERT_FALSE(cache["3"]);         // third oldest not pinned and gone
  ASSERT_EQ(*cache["4"], "xxxx");   // second newest still there
  ASSERT_EQ(*cache["5"], "xxxxx");  // newest still there
}

// _____________________________________________________________________________
TEST(LRUCacheTest, testTryEmplacePinnedClear) {
  LRUCache<string, string> cache(3);
  ASSERT_EQ(*cache.tryEmplacePinned("pinned", "bar").first, "bar");
  cache.insert("1", "x");
  cache.insert("2", "xx");
  ASSERT_EQ(*cache["1"], "x");         // there
  ASSERT_EQ(*cache["2"], "xx");        // there
  ASSERT_EQ(*cache["pinned"], "bar");  // there
  cache.clear();
  ASSERT_FALSE(cache["1"]);            // gone
  ASSERT_FALSE(cache["2"]);            // gone
  ASSERT_EQ(*cache["pinned"], "bar");  // still there
}

// _____________________________________________________________________________
TEST(LRUCacheTest, testIncreasingCapacity) {
  LRUCache<string, string> cache(5);
  cache.insert("1", "x");
  cache.insert("2", "x");
  cache.insert("3", "x");
  cache.insert("4", "x");
  cache.insert("5", "x");

  ASSERT_EQ(*cache["1"], "x");
  ASSERT_EQ(*cache["2"], "x");
  ASSERT_EQ(*cache["3"], "x");
  ASSERT_EQ(*cache["4"], "x");
  ASSERT_EQ(*cache["5"], "x");
  cache.setCapacity(10);
  ASSERT_EQ(*cache["3"], "x");
  cache.insert("3", "xxxx");
  ASSERT_EQ(*cache["3"], "xxxx");
  ASSERT_EQ(*cache["5"], "x");
  cache.insert("0", "xxxx");
  ASSERT_EQ(*cache["0"], "xxxx");
  ASSERT_EQ(*cache["4"], "x");
  ASSERT_EQ(*cache["5"], "x");
}

// _____________________________________________________________________________
TEST(LRUCacheTest, testDecreasingCapacity) {
  LRUCache<string, string> cache(10);
  cache.insert("1", "x");
  cache.insert("2", "xx");
  cache.insert("3", "xxx");
  cache.insert("4", "xxxx");
  cache.insert("5", "xxxxx");
  ASSERT_EQ(*cache["1"], "x");
  ASSERT_EQ(*cache["2"], "xx");
  ASSERT_EQ(*cache["3"], "xxx");
  ASSERT_EQ(*cache["4"], "xxxx");
  ASSERT_EQ(*cache["5"], "xxxxx");
  cache.insert("9", "xxxxxxxxx");
  cache.setCapacity(2);
  ASSERT_EQ(*cache["9"], "xxxxxxxxx");  // freshly inserted
  ASSERT_EQ(*cache["5"], "xxxxx");      // second leat recently used
  ASSERT_FALSE(cache["1"]);
  ASSERT_FALSE(cache["2"]);
  ASSERT_FALSE(cache["3"]);
  ASSERT_FALSE(cache["4"]);
}
}  // namespace ad_utility

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
