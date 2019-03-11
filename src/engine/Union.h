// Copyright 2018, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Florian Kramer (florian.kramer@mail.uni-freiburg.de)
#pragma once

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../parser/ParsedQuery.h"
#include "../util/HashMap.h"
#include "Operation.h"
#include "QueryExecutionTree.h"

class Union : public Operation {
 public:
  Union(QueryExecutionContext* qec, std::shared_ptr<QueryExecutionTree> t1,
        std::shared_ptr<QueryExecutionTree> t2);

  virtual string asString(size_t indent = 0) const override;

  virtual size_t getResultWidth() const override;

  virtual vector<size_t> resultSortedOn() const override;

  ad_utility::HashMap<string, size_t> getVariableColumns() const;

  virtual void setTextLimit(size_t limit) override;

  virtual bool knownEmptyResult() override;

  virtual float getMultiplicity(size_t col) override;

  virtual size_t getSizeEstimate() override;

  virtual size_t getCostEstimate() override;

  const static size_t NO_COLUMN;

  static void computeUnion(
      ResultTable* result, shared_ptr<const ResultTable> subRes1,
      shared_ptr<const ResultTable> subRes2,
      const std::vector<std::array<size_t, 2>>& columnOrigins);

   virtual std::vector<QueryExecutionTree*> getChildTrees() override{
     return {_subtrees[0].get(), _subtrees[1].get()};
   }
 private:
  virtual void computeResult(ResultTable* result) override;

  /**
   * @brief This method is used to convert runtime information about the
   *        column count of _subtrees[0] to compile time information
   */
  template <typename Res>
  static void computeUnion(
      vector<Res>* res, shared_ptr<const ResultTable> subRes1,
      shared_ptr<const ResultTable> subRes2,
      const std::vector<std::array<size_t, 2>>& columnOrigins);

  /**
   * @brief This method is used to convert runtime information about the
   *        column count of _subtrees[1] to compile time information
   */
  template <typename Res, typename L>
  static void computeUnion(
      vector<Res>* res, const vector<L>* left,
      shared_ptr<const ResultTable> subRes2,
      const std::vector<std::array<size_t, 2>>& columnOrigins);

  // The method is declared here to make it unit testable
  template <typename Res, typename L, typename R>
  static void computeUnion(
      vector<Res>* res, const vector<L>* left, const vector<R>* right,
      const std::vector<std::array<size_t, 2>>& columnOrigins);

  /**
   * @brief This stores the input column from each of the two subtrees or
   * NO_COLUMN if the subtree does not have a matching column for each result
   * column.
   */
  std::vector<std::array<size_t, 2>> _columnOrigins;
  std::shared_ptr<QueryExecutionTree> _subtrees[2];
};
