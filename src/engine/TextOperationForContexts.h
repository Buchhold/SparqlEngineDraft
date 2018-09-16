// Copyright 2015, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Björn Buchhold (buchhold@informatik.uni-freiburg.de)
#pragma once

#include <list>
#include <utility>
#include <vector>

#include "./Operation.h"
#include "./QueryExecutionTree.h"

using std::list;

using std::pair;
using std::vector;

class TextOperationForContexts : public Operation {
 public:
  TextOperationForContexts(
      QueryExecutionContext* qec, const string& words,
      const vector<pair<std::shared_ptr<QueryExecutionTree>, size_t>>& subtrees,
      size_t textLimit);

  TextOperationForContexts(QueryExecutionContext* qec, const string& words,
                           size_t textLimit)
      : TextOperationForContexts(
            qec, words,
            vector<pair<std::shared_ptr<QueryExecutionTree>, size_t>>(),
            textLimit) {}

  virtual string asString(size_t indent = 0) const override;

  virtual size_t getResultWidth() const override;

  virtual vector<size_t> resultSortedOn() const override { return {}; }

  virtual void setTextLimit(size_t limit) override {
    _textLimit = limit;
    for (auto& st : _subtrees) {
      st.first->setTextLimit(limit);
    }
  }

  virtual size_t getSizeEstimate() override {
    if (_executionContext) {
      // TODO: return a better estimate!
    }
    return 10000;
  }

  virtual size_t getCostEstimate() override {
    size_t sum = 10000;
    for (auto& pair : _subtrees) {
      sum += pair.first->getCostEstimate();
    }
    return sum;
  }

  virtual float getMultiplicity(size_t col) override {
    // TODO: return a better estimate!
    return col;
  }

  virtual bool knownEmptyResult() override {
    // TODO: return a better estimate!
    return false;
  }

 private:
  string _words;
  vector<pair<std::shared_ptr<QueryExecutionTree>, size_t>> _subtrees;
  size_t _textLimit;

  virtual void computeResult(ResultTable* result) const override;
};
