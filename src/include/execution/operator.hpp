#pragma once

#include "common/config.hpp"
#include "common/macro.hpp"
#include "common/typedefs.hpp"
#include "execution/execution_context.hpp"

#include <algorithm>
#include <memory>

namespace babydb {

typedef std::vector<std::pair<Tuple, idx_t>> Chunk;

enum OperatorState {
    HAVE_MORE_OUTPUT,
    EXHAUSETED
};

class Operator {
public:
    virtual ~Operator() = default;

    DISALLOW_COPY_AND_MOVE(Operator);

    Operator(const ExecutionContext &exec_ctx, std::vector<std::shared_ptr<Operator>> &&child_operators,
             const Schema &output_schema)
        : exec_ctx_(exec_ctx), child_operators_(std::move(child_operators)), 
          output_schema_(output_schema) {}

    Operator(const ExecutionContext &exec_ctx, std::vector<std::shared_ptr<Operator>> &&child_operators)
        : exec_ctx_(exec_ctx), child_operators_(std::move(child_operators)), output_schema_{} {
        for (auto &child_operator : child_operators_) {
            auto child_schema = child_operator->GetOutputSchema();
            output_schema_.insert(output_schema_.end(), child_schema.begin(), child_schema.end());
        }
    }

    virtual OperatorState Next(Chunk &output_chunk) = 0;

    void Init() {
        for (auto &child_operator : child_operators_) {
            child_operator->Init();
        }
        SelfInit();
    }

    const Schema& GetOutputSchema() {
        return output_schema_;
    }

    void Check() {
        for (auto &child_operator : child_operators_) {
            child_operator->Check();
        }
        CheckSchema();
        SelfCheck();
    }

    virtual std::string BindTableName() { return INVALID_NAME; }

protected:
    virtual void SelfInit() = 0;

    virtual void SelfCheck() = 0;

    void CheckSchema() {
        auto schema_copy = output_schema_;
        std::sort(schema_copy.begin(), schema_copy.end());
        for (idx_t column_id = 1; column_id < schema_copy.size(); column_id++) {
            if (schema_copy[column_id] == schema_copy[column_id - 1]) {
                throw std::logic_error("Duplicated column name in operator's output");
            }
        }
    }

protected:
    ExecutionContext exec_ctx_;

    std::vector<std::shared_ptr<Operator>> child_operators_;

    Schema output_schema_;
};

}