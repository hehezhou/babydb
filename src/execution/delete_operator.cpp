#include "execution/delete_operator.hpp"

#include "execution/execution_common.hpp"
#include "storage/catalog.hpp"
#include "storage/index.hpp"
#include "storage/table.hpp"

namespace babydb {

DeleteOperator::DeleteOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator)
    : Operator(exec_ctx, {child_operator}, Schema{}), table_name_(child_operator->BindTableName()) {
    throw std::logic_error("Disallowed in Project 2");
}

void DeleteOperator::SelfCheck() {
    throw std::logic_error("Disallowed in Project 2");
}

OperatorState DeleteOperator::Next(Chunk &output_chunk) {
    throw std::logic_error("Disallowed in Project 2");
}

}