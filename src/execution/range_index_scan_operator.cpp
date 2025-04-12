#include "execution/range_index_scan_operator.hpp"

#include "storage/catalog.hpp"
#include "storage/index.hpp"
#include "storage/table.hpp"

namespace babydb {

RangeIndexScanOperator::RangeIndexScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name,
                                               const Schema &fetch_columns, const Schema &output_schema,
                                               const std::string &index_name, const RangeInfo &range)
    : Operator(exec_ctx, {}, output_schema), table_name_(table_name), fetch_columns_(fetch_columns),
      index_name_(index_name), range_(range) {}

OperatorState RangeIndexScanOperator::Next(Chunk &output_chunk) {
    output_chunk.clear();

    auto &table = exec_ctx_.catalog_.FetchTable(table_name_);
    auto key_attrs = table.schema_.GetKeyAttrs(fetch_columns_);
    auto read_guard = table.GetReadTableGuard();

    if (!results_scanned_) {
        results_scanned_ = true;

        auto &index = dynamic_cast<RangeIndex&>(exec_ctx_.catalog_.FetchIndex(index_name_));
        index.ScanRange(range_, row_ids_, exec_ctx_);
        next_ite_ = row_ids_.begin();
    }

    while (output_chunk.size() < exec_ctx_.config_.CHUNK_SUGGEST_SIZE) {
        if (next_ite_ == row_ids_.end()) {
            return EXHAUSETED;
        }

        auto row_id = *next_ite_;
        next_ite_++;

        auto& [tuple, meta] = read_guard.Rows()[row_id];
        output_chunk.emplace_back(tuple.KeysFromTuple(key_attrs), row_id);
    }

    return HAVE_MORE_OUTPUT;

}

void RangeIndexScanOperator::SelfInit() {
    row_ids_.clear();
    results_scanned_ = false;
    next_ite_ = row_ids_.end();
}

void RangeIndexScanOperator::SelfCheck() {
    auto &table = exec_ctx_.catalog_.FetchTable(table_name_);
    auto &index = dynamic_cast<RangeIndex&>(exec_ctx_.catalog_.FetchIndex(index_name_));

    if (index.table_name_ != table_name_) {
        throw std::logic_error("RangeIndexScanOperator: Table and Index do not match");
    }

    table.schema_.GetKeyAttrs(fetch_columns_);
    if (fetch_columns_.size() != output_schema_.size()) {
        throw std::logic_error("RangeIndexScanOperator: Fetch columns and output schema do not match");
    }
}

}