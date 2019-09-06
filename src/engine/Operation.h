// Copyright 2015, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Björn Buchhold (buchhold@informatik.uni-freiburg.de)
#pragma once

#include <iomanip>
#include <iostream>
#include <memory>
#include <utility>

#include "../util/Exception.h"
#include "../util/Log.h"
#include "../util/Timer.h"
#include "QueryExecutionContext.h"
#include "ResultTable.h"
#include "RuntimeInformation.h"

using std::endl;
using std::pair;
using std::shared_ptr;

class Operation {
 public:
  // Default Constructor.
  Operation() : _executionContext(nullptr), _hasComputedSortColumns(false) {}

  // Typical Constructor.
  explicit Operation(QueryExecutionContext* executionContext)
      : _executionContext(executionContext), _hasComputedSortColumns(false) {}

  // Destructor.
  virtual ~Operation() {
    // Do NOT delete _executionContext, since
    // there is no ownership.
  }

  // Get the result for the subtree rooted at this element.
  // Use existing results if they are already available, otherwise
  // trigger computation.
  shared_ptr<const ResultTable> getResult() {
    ad_utility::Timer timer;
    timer.start();
    auto& cache = _executionContext->getQueryTreeCache();
    const string cacheKey = asString();
    LOG(TRACE) << "Check cache for Operation result" << endl;
    LOG(TRACE) << "Using key: \n" << cacheKey << endl;
    auto [newResult, existingResult] = cache.tryEmplace(cacheKey);

    if (newResult) {
      LOG(TRACE) << "Not in the cache, need to compute result" << endl;
      // Passing the raw pointer here is ok as the result shared_ptr remains
      // in scope
      try {
        computeResult(newResult->_resTable.get());
      } catch (const ad_semsearch::AbortException& e) {
        // A child Operation was aborted, abort this Operation
        // as well. The child already printed
        abort(newResult, false);
        // Continue unwinding the stack
        throw;
      } catch (const std::exception& e) {
        // We are in the innermost level of the exception, so print
        abort(newResult, true);
        // Rethrow as QUERY_ABORTED allowing us to print the Operation
        // only at innermost failure of a recursive call
        throw ad_semsearch::AbortException(e);
      } catch (...) {
        // We are in the innermost level of the exception, so print
        abort(newResult, true);
        LOG(ERROR) << "WEIRD_EXCEPTION not inheriting from std::exception"
                   << endl;
        // Rethrow as QUERY_ABORTED allowing us to print the Operation
        // only at innermost failure of a recursive call
        throw ad_semsearch::AbortException("WEIRD_EXCEPTION");
      }
      timer.stop();
      _runtimeInfo.setRows(newResult->_resTable->size());
      _runtimeInfo.setCols(getResultWidth());
      _runtimeInfo.setDescriptor(getDescriptor());
      _runtimeInfo.setColumnNames(getVariableColumns());

      _runtimeInfo.setTime(timer.msecs());
      _runtimeInfo.setWasCached(false);
      // cache the runtime information for the execution as well
      newResult->_runtimeInfo = _runtimeInfo;
      // Only now we can let other threads access the result
      // and runtime information
      newResult->_resTable->finish();
      return newResult->_resTable;
    }

    existingResult->_resTable->awaitFinished();
    if (existingResult->_resTable->status() == ResultTable::ABORTED) {
      LOG(ERROR) << "Operation aborted while awaiting result" << endl;
      AD_THROW(ad_semsearch::Exception::BAD_QUERY,
               "Operation aborted while awaiting result");
    }
    timer.stop();
    _runtimeInfo = existingResult->_runtimeInfo;
    // We need to update column names and descriptor as we may have cached with
    // different variable names
    _runtimeInfo.setDescriptor(getDescriptor());
    _runtimeInfo.setColumnNames(getVariableColumns());
    _runtimeInfo.setTime(timer.msecs());
    _runtimeInfo.setWasCached(true);
    _runtimeInfo.addDetail("original_total_time",
                           existingResult->_runtimeInfo.getTime());
    _runtimeInfo.addDetail("original_operation_time",
                           existingResult->_runtimeInfo.getOperationTime());
    return existingResult->_resTable;
  }

  /**
   * Abort this Operation.  Removes the Operation's result from the cache so
   * that it can be retried. The result must be owned meaning only the
   * computing thread can abort an Operation. Retrying may succeed for example
   * when memory pressure was lowered in the meantime.  When print is true the
   * Operation is printed to the ERROR LOG
   */
  void abort(const shared_ptr<CacheValue>& cachedResult, bool print) {
    const std::string opString = asString();
    if (print) {
      LOG(ERROR) << "Aborted Operation:" << endl;
      LOG(ERROR) << opString << endl;
    }
    // Remove Operation from cache so we may retry it later. Anyone with a live
    // pointer will be waiting and register the abort.
    _executionContext->getQueryTreeCache().erase(opString);
    cachedResult->_resTable->abort();
  }

  /**
   * @return A list of columns on which the result of this operation is sorted.
   */
  const vector<size_t>& getResultSortedOn() {
    if (!_hasComputedSortColumns) {
      _hasComputedSortColumns = true;
      _resultSortedColumns = resultSortedOn();
    }
    return _resultSortedColumns;
  }

  const Index& getIndex() const { return _executionContext->getIndex(); }

  const Engine& getEngine() const { return _executionContext->getEngine(); }

  // Get a unique, not ambiguous string representation for a subtree.
  // This should possible act like an ID for each subtree.
  virtual string asString(size_t indent = 0) const = 0;

  // Gets a very short (one line without line ending) descriptor string for
  // this Operation.  This string is used in the RuntimeInformation
  virtual string getDescriptor() const = 0;
  virtual size_t getResultWidth() const = 0;
  virtual void setTextLimit(size_t limit) = 0;
  virtual size_t getCostEstimate() = 0;
  virtual size_t getSizeEstimate() = 0;
  virtual float getMultiplicity(size_t col) = 0;
  virtual bool knownEmptyResult() = 0;
  virtual ad_utility::HashMap<string, size_t> getVariableColumns() const = 0;

  RuntimeInformation& getRuntimeInfo() { return _runtimeInfo; }

 protected:
  // The QueryExecutionContext for this particular element.
  // No ownership.
  QueryExecutionContext* _executionContext;

  /**
   * @brief Allows for updating of the sorted columns of an operation. This
   *        has to be used by an operation if it's sort columns change during
   *        the operations lifetime.
   */
  void setResultSortedOn(const vector<size_t>& sortedColumns) {
    _resultSortedColumns = sortedColumns;
  }

  /**
   * @brief Compute and return the columns on which the result will be sorted
   * @return The columns on which the result will be sorted.
   */
  virtual vector<size_t> resultSortedOn() const = 0;

 private:
  //! Compute the result of the query-subtree rooted at this element..
  //! Computes both, an EntityList and a HitList.
  virtual void computeResult(ResultTable* result) = 0;

  vector<size_t> _resultSortedColumns;
  RuntimeInformation _runtimeInfo;

  bool _hasComputedSortColumns;
};
