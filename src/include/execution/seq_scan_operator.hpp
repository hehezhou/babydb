#pragma once

#include "execution/operator.hpp"

namespace babydb {

/**
 * Sequential Scan Operator
 * By default, it will use "<table name>.<column name>" as output schema.
 * You can also manually specify the table name in output schema.
 * Or specify the output schema.
 */
class SeqScanOperator : public Operator {
public:
    SeqScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name);

    SeqScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name, const Schema &fetch_columns);

    SeqScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name, const Schema &fetch_columns,
                    const std::string &table_output_name);

    SeqScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name, const Schema &fetch_columns,
                    const Schema &output_schema);

    ~SeqScanOperator() override = default;

    OperatorState Next(Chunk &output_chunk) override;

    void SelfInit() override;

    void SelfCheck() override;

    std::string BindTableName() override { return table_name_; }

private:
    std::string table_name_;

    Schema fetch_columns_;

    idx_t next_row_id{0};
};

}