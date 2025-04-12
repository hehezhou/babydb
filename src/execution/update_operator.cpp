#include "execution/update_operator.hpp"

#include "common/macro.hpp"
#include "execution/execution_common.hpp"
#include "storage/catalog.hpp"
#include "storage/index.hpp"
#include "storage/table.hpp"

namespace babydb {

UpdateOperator::UpdateOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator)
    : Operator(exec_ctx, {child_operator}, Schema{}), table_name_(child_operator->BindTableName()),
      input_schema_(std::nullopt) {}

UpdateOperator::UpdateOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator,
                               const Schema &input_schema)
    : Operator(exec_ctx, {child_operator}, Schema{}), table_name_(child_operator->BindTableName()),
      input_schema_(input_schema) {}

void UpdateOperator::SelfCheck() {
    auto &child_schema = child_operators_[0]->GetOutputSchema();
    auto &table = exec_ctx_.catalog_.FetchTable(table_name_);

    if (input_schema_.has_value()) {
        if (input_schema_->size() != table.schema_.size()) {
            throw std::logic_error("UpdateOperator: The schema of the table and the input do not match");
        }
        child_schema.GetKeyAttrs(*input_schema_);
    } else {
        if (child_schema.size() != table.schema_.size()) {
            throw std::logic_error("UpdateOperator: The schema of the table and the input do not match");
        }
    }
}

OperatorState UpdateOperator::Next(Chunk &) {
    auto &table = exec_ctx_.catalog_.FetchTable(table_name_);
    std::vector<idx_t> key_attrs;
    if (input_schema_.has_value()) {
        key_attrs = child_operators_[0]->GetOutputSchema().GetKeyAttrs(*input_schema_);
    }

    Index *index = nullptr;
    idx_t index_key_attr = INVALID_ID;
    if (table.GetIndex() != INVALID_NAME) {
        index = &exec_ctx_.catalog_.FetchIndex(table.GetIndex());
        index_key_attr = table.schema_.GetKeyAttr(index->key_name_);
    } else {
        throw std::logic_error("Disallowed in Project 2");
    }

    Chunk update_chunk;

    Chunk fetch_chunk;
    auto child_state = OperatorState::HAVE_MORE_OUTPUT;
    while (child_state != EXHAUSETED) {
        child_state = child_operators_[0]->Next(fetch_chunk);
        update_chunk.insert(update_chunk.end(), fetch_chunk.begin(), fetch_chunk.end());
    }

    // Directly cover (since in Project 2, there are no primary key update)
    auto write_guard = table.GetWriteTableGuard();
    for (auto &data : update_chunk) {
        auto key = data.first.KeyFromTuple(index_key_attr);
        InsertRow(write_guard, std::move(data.first), index, key, exec_ctx_);
    }

    return EXHAUSETED;
}

}