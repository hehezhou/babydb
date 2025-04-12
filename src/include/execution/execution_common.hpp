#pragma once

#include "common/typedefs.hpp"

namespace babydb {

class Index;
class ExecutionContext;
class Transaction;
class WriteTableGuard;

//! Insert (or cover) a tuple to a table
void InsertRow(WriteTableGuard &write_guard, Tuple &&tuple, Index *index, const data_t &key, ExecutionContext &exec_ctx);

}