#include "execution/seq_scan_operator.hpp"

#include "storage/catalog.hpp"
#include "storage/table.hpp"

namespace babydb {

static const Schema& FetchTableSchema(const ExecutionContext &exec_ctx, const std::string &table_name) {
    auto &table = exec_ctx.catalog_.FetchTable(table_name);
    return table.schema_;
}

static Schema CombineSchema(const std::string &table_name, const Schema &schema) {
    auto schema_copy = schema;
    for (auto &column : schema_copy) {
        column = table_name + "." + column;
    }
    return schema_copy;
}

SeqScanOperator::SeqScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name)
    : SeqScanOperator(exec_ctx, table_name, FetchTableSchema(exec_ctx, table_name)) {}

SeqScanOperator::SeqScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name,
                                 const Schema &fetch_columns)
    : SeqScanOperator(exec_ctx, table_name, fetch_columns, table_name) {}

SeqScanOperator::SeqScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name,
                                 const Schema &fetch_columns, const std::string &table_output_name)
    : SeqScanOperator(exec_ctx, table_name, fetch_columns, CombineSchema(table_output_name, fetch_columns)) {}

SeqScanOperator::SeqScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name,
                                 const Schema &fetch_columns, const Schema &output_schema)
    : Operator(exec_ctx, {}, output_schema), table_name_(table_name), fetch_columns_(fetch_columns) {
    throw std::logic_error("Disallowed in Project 2");
}

OperatorState SeqScanOperator::Next(Chunk &output_chunk) {
    output_chunk.clear();

    auto &table = exec_ctx_.catalog_.FetchTable(table_name_);
    auto key_attrs = table.schema_.GetKeyAttrs(fetch_columns_);

    auto read_guard = table.GetReadTableGuard();

    while (output_chunk.size() < exec_ctx_.config_.CHUNK_SUGGEST_SIZE) {
        if (next_row_id >= read_guard.Rows().size()) {
            return EXHAUSETED;
        }
        auto& [tuple, meta] = read_guard.Rows()[next_row_id];
        next_row_id++;

        output_chunk.emplace_back(tuple.KeysFromTuple(key_attrs), next_row_id - 1);
    }

    return HAVE_MORE_OUTPUT;
}

void SeqScanOperator::SelfInit() {
    next_row_id = 0;
}

void SeqScanOperator::SelfCheck() {
    FetchTableSchema(exec_ctx_, table_name_).GetKeyAttrs(fetch_columns_);

    if (fetch_columns_.size() != output_schema_.size()) {
        throw std::logic_error("SeqScanOperator: Fetch columns and output schema do not match");
    }
}

}