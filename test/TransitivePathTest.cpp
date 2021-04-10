// Copyright 2018, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Florian Kramer (florian.kramer@mail.uni-freiburg.de)

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "../src/engine/TransitivePath.h"
#include "../src/global/Id.h"

TEST(TransitivePathTest, computeTransitivePath) {
  IdTable sub(2);
  sub.push_back({0, 2});
  sub.push_back({2, 4});
  sub.push_back({4, 7});
  sub.push_back({0, 7});
  sub.push_back({3, 3});
  sub.push_back({7, 0});
  // Disconnected component.
  sub.push_back({10, 11});

  IdTable result(2);

  IdTable expected(2);
  expected.push_back({0, 2});
  expected.push_back({0, 4});
  expected.push_back({0, 7});
  expected.push_back({0, 0});
  expected.push_back({2, 4});
  expected.push_back({2, 7});
  expected.push_back({2, 0});
  expected.push_back({2, 2});
  expected.push_back({4, 7});
  expected.push_back({4, 0});
  expected.push_back({4, 2});
  expected.push_back({4, 4});
  expected.push_back({3, 3});
  expected.push_back({7, 0});
  expected.push_back({7, 2});
  expected.push_back({7, 4});
  expected.push_back({7, 7});
  expected.push_back({10, 11});

  TransitivePath T(nullptr, nullptr, false, false, 0, 0, 0, 0, "bim"s, "bam"s, 0, 0);

  T.computeTransitivePath<2>(&result, sub, true, true, 0, 1, 0, 0,
                                           1,
                                           std::numeric_limits<size_t>::max());
  ASSERT_EQ(expected, result);

  result.clear();
  expected.clear();
  expected.push_back({0, 2});
  expected.push_back({0, 4});
  expected.push_back({0, 7});
  expected.push_back({0, 0});
  expected.push_back({2, 4});
  expected.push_back({2, 7});
  expected.push_back({4, 7});
  expected.push_back({4, 0});
  expected.push_back({3, 3});
  expected.push_back({7, 0});
  expected.push_back({7, 2});
  expected.push_back({7, 7});
  expected.push_back({10, 11});

  T.computeTransitivePath<2>(&result, sub, true, true, 0, 1, 0, 0,
                                           1, 2);
  ASSERT_EQ(expected, result);

  result.clear();
  expected.clear();
  expected.push_back({7, 0});
  expected.push_back({7, 2});
  expected.push_back({7, 7});

  T.computeTransitivePath<2>(&result, sub, false, true, 0, 1, 7,
                                           0, 1, 2);
  ASSERT_EQ(expected, result);

  result.clear();
  expected.clear();
  expected.push_back({0, 2});
  expected.push_back({7, 2});

  T.computeTransitivePath<2>(&result, sub, true, false, 0, 1, 0,
                                           2, 1, 2);
  ASSERT_EQ(expected, result);
}
