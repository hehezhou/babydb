#include "execution/projection_operator.hpp"

namespace babydb {

static std::vector<std::unique_ptr<Projection>> TransToVec(std::unique_ptr<Projection> &&projection) {
    std::vector<std::unique_ptr<Projection>> result;
    result.push_back(std::move(projection));
    return result;
}

ProjectionOperator::ProjectionOperator(const ExecutionContext &exec_ctx,
                                       const std::shared_ptr<Operator> &child_operator,
                                       std::vector<std::unique_ptr<Projection>> &&projections,
                                       bool update_in_place)
    : Operator(exec_ctx, {child_operator}) {
    for (auto &projection_function : projections_) {
        if (projection_function == nullptr) {
            throw std::logic_error("ProjectionOperator: projection function is nullptr");
        }
    }
    if (update_in_place) {
        output_schema_ = child_operators_[0]->GetOutputSchema();
        projections_.resize(output_schema_.size());

        for (auto &projection_function : projections) {
            auto key_attr = output_schema_.GetKeyAttr(projection_function->output_name_);
            if (projections_[key_attr] != nullptr) {
                throw std::logic_error("ProjectionOperator: Duplicate projection function's output");
            }
            projections_[key_attr] = std::move(projection_function);
        }
        for (idx_t column_id = 0; column_id < output_schema_.size(); column_id++) {
            if (projections_[column_id] == nullptr) {
                projections_[column_id] = std::make_unique<UnitProjection>(output_schema_[column_id]);
            }
        }
    } else {
        projections_ = std::move(projections);
        for (auto &projection_function : projections_) {
            output_schema_.push_back(projection_function->output_name_);
        }
    }
}

ProjectionOperator::ProjectionOperator(const ExecutionContext &exec_ctx,
                                       const std::shared_ptr<Operator> &child_operator,
                                       std::unique_ptr<Projection> &&projection,
                                       bool update_in_place)
    : ProjectionOperator(exec_ctx, child_operator, TransToVec(std::move(projection)), update_in_place) {}

OperatorState ProjectionOperator::Next(Chunk &output_chunk) {
    auto result = child_operators_[0]->Next(output_chunk);
    for (auto &data : output_chunk) {
        auto &old_tuple = data.first;
        Tuple new_tuple;
        for (auto &projection_function : projections_) {
            new_tuple.push_back(projection_function->Calc(old_tuple));
        }
        old_tuple = new_tuple;
    }
    return result;
}

void ProjectionOperator::SelfInit() {
    auto &input_schema = child_operators_[0]->GetOutputSchema();
    for (auto &projection_function : projections_) {
        projection_function->Init(input_schema);
    }
}

void ProjectionOperator::SelfCheck() {
    SelfInit();
}

}