#include "execution/insert_operator.hpp"

#include "execution/execution_common.hpp"
#include "storage/catalog.hpp"
#include "storage/index.hpp"
#include "storage/table.hpp"

#include <optional>

namespace babydb {

InsertOperator::InsertOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator,
                               const std::string &table_name)
    : Operator(exec_ctx, {child_operator}, Schema{}), table_name_(table_name) {
    auto &child_schema = child_operators_[0]->GetOutputSchema();
    input_schema_ = child_schema;
}

InsertOperator::InsertOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator,
                               const std::string &table_name, const Schema &input_schema)
    : Operator(exec_ctx, {child_operator}, Schema{}), table_name_(table_name), input_schema_(input_schema) {}

void InsertOperator::SelfCheck() {
    auto &child_schema = child_operators_[0]->GetOutputSchema();
    auto &table = exec_ctx_.catalog_.FetchTable(table_name_);

    if (input_schema_.size() != table.schema_.size()) {
        throw std::logic_error("InsertOperator: The schema of the table and the input do not match");
    }

    child_schema.GetKeyAttrs(input_schema_);
}

OperatorState InsertOperator::Next(Chunk &output_chunk) {
    output_chunk.clear();
    auto &table = exec_ctx_.catalog_.FetchTable(table_name_);
    auto key_attrs = child_operators_[0]->GetOutputSchema().GetKeyAttrs(input_schema_);
    Index *index = nullptr;
    idx_t index_key_attr = INVALID_ID;
    if (table.GetIndex() != INVALID_NAME) {
        index = &exec_ctx_.catalog_.FetchIndex(table.GetIndex());
        index_key_attr = table.schema_.GetKeyAttr(index->key_name_);
    } else {
        throw std::logic_error("Disallowed in Project 2");
    }

    Chunk insert_chunk;
    auto child_state = OperatorState::HAVE_MORE_OUTPUT;
    while (child_state != EXHAUSETED) {
        child_state = child_operators_[0]->Next(insert_chunk);
        auto write_guard = table.GetWriteTableGuard();
        for (auto &insert_data : insert_chunk) {
            auto insert_tuple = insert_data.first.KeysFromTuple(key_attrs);

            auto key = insert_tuple.KeyFromTuple(index_key_attr);
            InsertRow(write_guard, std::move(insert_tuple), index, key, exec_ctx_);
        }
    }
    return EXHAUSETED;
}

}