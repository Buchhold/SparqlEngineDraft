// Copyright 2011, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Björn Buchhold (buchhold@informatik.uni-freiburg.de)

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../global/Constants.h"
#include "../index/Index.h"
#include "../util/LRUCache.h"
#include "../util/Log.h"
#include "./Engine.h"
#include "./ResultTable.h"
#include "QueryPlanningCostFactors.h"
#include "RuntimeInformation.h"

using std::shared_ptr;
using std::string;
using std::vector;

struct CacheValue {
  CacheValue() : resTable(std::make_shared<ResultTable>()), runtimeInfo() {}
  std::shared_ptr<ResultTable> resTable;
  RuntimeInformation runtimeInfo;
};

/**
 * This only needs to be approximately correct the size of _runtimeInfo
 * is often negligible compared to the resTable so we only add its
 * empty size as part of the CacheValue not its heap memory
 */
namespace ad_utility {
template <>
inline size_t memorySizeOf(const CacheValue& cv) {
  return cv.resTable->rows() * cv.resTable->cols() * sizeof(Id) +
         sizeof(CacheValue);
}
}  // namespace ad_utility

typedef ad_utility::LRUCache<string, CacheValue> SubtreeCache;

// Execution context for queries.
// Holds references to index and engine, implements caching.
class QueryExecutionContext {
 public:
  QueryExecutionContext(const Index& index, const Engine& engine)
      : _subtreeCache(CACHE_SIZE_BYTES),
        _index(index),
        _engine(engine),
        _costFactors() {}

  SubtreeCache& getQueryTreeCache() { return _subtreeCache; }

  const Engine& getEngine() const { return _engine; }

  const Index& getIndex() const { return _index; }

  void clearCache() { _subtreeCache.clear(); }

  void readCostFactorsFromTSVFile(const string& fileName) {
    _costFactors.readFromFile(fileName);
  }

  float getCostFactor(const string& key) const {
    return _costFactors.getCostFactor(key);
  };

 private:
  SubtreeCache _subtreeCache;
  const Index& _index;
  const Engine& _engine;
  QueryPlanningCostFactors _costFactors;
};
