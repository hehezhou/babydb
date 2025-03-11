#include "execution/operator.hpp"

#include <unordered_map>

namespace babydb {

/**
 * Aggregate Operator
 * We only support summary aggregation on one column
 * The output schema is [aggregate column, aggregate value]
 */
class AggregateOperator : public Operator {
public:
    AggregateOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator,
                      const std::string &group_by, const std::string &aggregate_key);

    ~AggregateOperator() override = default;

    OperatorState Next(Chunk &output_chunk) override;

    void SelfInit() override;

    void SelfCheck() override;

private:
    void BuildHashTable();

private:
    std::string group_by_;

    std::string aggregate_column_;

    std::unordered_multimap<data_t, data_t> hash_table_;

    std::unordered_multimap<data_t, data_t>::iterator output_ptr_;

    bool hash_table_build_{false};
};

}