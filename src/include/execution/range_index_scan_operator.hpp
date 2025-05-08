#pragma once

#include "execution/operator.hpp"

namespace babydb {

/**
 * Sequential Scan Operator
 * By default, it will use "<table name>.<column name>" as output schema.
 * You can also manually specify the table name in output schema.
 * Or specify the output schema.
 */
class RangeIndexScanOperator : public Operator {
public:
    RangeIndexScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name,
                           const Schema &fetch_columns, const Schema &output_schema,
                           const std::string &index_name, const RangeInfo &range);

    ~RangeIndexScanOperator() override = default;

    OperatorState Next(Chunk &output_chunk) override;

    void SelfInit() override;

    void SelfCheck() override;

    std::string BindTableName() override { return table_name_; }

private:
    std::string table_name_;

    Schema fetch_columns_;

    std::string index_name_;

    RangeInfo range_;

    std::vector<idx_t> row_ids_;

    std::vector<idx_t>::iterator next_ite_;

    bool results_scanned_{false};
};

}