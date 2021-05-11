//
// Created by johannes on 19.04.20.
//

#ifndef QLEVER_BIND_H
#define QLEVER_BIND_H

#include "../parser/ParsedQuery.h"
#include "Operation.h"

/// BIND operation, currently only supports a very limited subset of expressions
class Bind : public Operation {
 public:
  Bind(QueryExecutionContext* qec, std::shared_ptr<QueryExecutionTree> subtree,
       GraphPatternOperation::Bind b)
      : Operation(qec), _subtree(std::move(subtree)), _bind(std::move(b)) {}

  // For the documentation of the overridden members, see Operation.h
  [[nodiscard]] string asString(size_t indent) const override;
  [[nodiscard]] string getDescriptor() const override;
  [[nodiscard]] size_t getResultWidth() const override;
  std::vector<QueryExecutionTree*> getChildren() override;
  void setTextLimit(size_t limit) override;
  size_t getCostEstimate() override;
  size_t getSizeEstimate() override;
  float getMultiplicity(size_t col) override;
  bool knownEmptyResult() override;
  [[nodiscard]] VariableColumnMap getVariableColumns() const override;

  // Returns the variable to which the expression will be bound
  [[nodiscard]] const SparqlVariable& targetVariable() const {
    return _bind._target;
  }

 protected:
  [[nodiscard]] vector<size_t> resultSortedOn() const override;

 private:
  std::shared_ptr<QueryExecutionTree> _subtree;
  GraphPatternOperation::Bind _bind;

  void computeResult(ResultTable* result) override;

  // Implementation for binding the sum of two columns: BIND (?x + ?y As ?z)
  template <int IN_WIDTH, int OUT_WIDTH>
  static void computeSumBind(IdTable* dynRes, const IdTable& inputDyn,
                             std::array<size_t, 2> columns,
                             array<ResultTable::ResultType, 2> inputTypes,
                             const Index& index);

  // Implementation for renaming a columns: BIND (?x As ?y)
  template <int IN_WIDTH, int OUT_WIDTH>
  static void computeRenameBind(IdTable* dynRes, const IdTable& inputDyn,
                                size_t column);

  // Bind a constant value from the knowledge base to the new columns : BIND (42
  // As ?x)
  template <int IN_WIDTH, int OUT_WIDTH>
  static void computeConstantBind(IdTable* dynRes, const IdTable& inputDyn,
                                  size_t targetVal);
};

#endif  // QLEVER_BIND_H
