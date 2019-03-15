// Copyright 2015, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Björn Buchhold (buchhold@informatik.uni-freiburg.de)

#include "./Sort.h"
#include <sstream>
#include "../util/kxsort.h"
#include "CallFixedSize.h"
#include "QueryExecutionTree.h"

using std::string;

// _____________________________________________________________________________
size_t Sort::getResultWidth() const { return _subtree->getResultWidth(); }

// _____________________________________________________________________________
Sort::Sort(QueryExecutionContext* qec,
           std::shared_ptr<QueryExecutionTree> subtree, size_t sortCol)
    : Operation(qec), _subtree(subtree), _sortCol(sortCol) {}

// _____________________________________________________________________________
string Sort::asString(size_t indent) const {
  std::ostringstream os;
  for (size_t i = 0; i < indent; ++i) {
    os << " ";
  }
  os << "SORT on column:" << _sortCol << "\n" << _subtree->asString(indent);
  return os.str();
}

template <int WIDTH>
void Sort::sort(IdTable* dynTable, size_t sortCol,
                ResultTable::ResultType sortType) {
  switch (sortType) {
    case ResultTable::ResultType::KB:
    case ResultTable::ResultType::VERBATIM:
    case ResultTable::ResultType::LOCAL_VOCAB: {
      IdTableStatic<WIDTH> table = dynTable->moveToStatic<WIDTH>();
      kx::radix_sort<WIDTH>(table, sortCol);
      *dynTable = table.moveToDynamic();
    } break;
    default:
      Engine::sort<WIDTH>(dynTable, sortCol);
  }
}

// _____________________________________________________________________________
void Sort::computeResult(ResultTable* result) {
  LOG(DEBUG) << "Getting sub-result for Sort result computation..." << endl;
  shared_ptr<const ResultTable> subRes = _subtree->getResult();

  std::string orderByVars = "";
  for (auto p : _subtree->getVariableColumnMap()) {
    if (p.second == _sortCol) {
      orderByVars = "ASC(" + p.first + ")";
      break;
    }
  }
  RuntimeInformation& runtimeInfo = getRuntimeInfo();
  runtimeInfo.setDescriptor("Sort on " + orderByVars);
  runtimeInfo.addChild(_subtree->getRootOperation()->getRuntimeInfo());

  LOG(DEBUG) << "Sort result computation..." << endl;
  result->_data.setCols(subRes->_data.cols());
  result->_resultTypes.insert(result->_resultTypes.end(),
                              subRes->_resultTypes.begin(),
                              subRes->_resultTypes.end());
  result->_localVocab = subRes->_localVocab;
  result->_data.insert(result->_data.end(), subRes->_data.begin(),
                       subRes->_data.end());
  int width = result->_data.cols();
  //  CALL_FIXED_SIZE_1(width, Engine::sort, &result->_data, _sortCol);
  CALL_FIXED_SIZE_1(width, Sort::sort, &result->_data, _sortCol,
                    result->getResultType(_sortCol));
  result->_sortedBy = resultSortedOn();

  LOG(DEBUG) << "Sort result computation done." << endl;
}
