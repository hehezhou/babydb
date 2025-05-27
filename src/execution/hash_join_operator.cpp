#include "execution/hash_join_operator.hpp"

#include "common/config.hpp"

namespace babydb {

HashJoinOperator::HashJoinOperator(const ExecutionContext &exec_ctx,
                                   const std::shared_ptr<Operator> &probe_child_operator,
                                   const std::shared_ptr<Operator> &build_child_operator,
                                   const std::string &probe_column_name,
                                   const std::string &build_column_name)
    : Operator(exec_ctx, {probe_child_operator, build_child_operator}),
      probe_column_name_(probe_column_name),
      build_column_name_(build_column_name) {}

static Tuple UnionTuple(const Tuple &a, const std::vector<data_t>::iterator &start, idx_t width) {
    Tuple result = a;
    result.insert(result.end(), start, start + width);
    return result;
}

OperatorState HashJoinOperator::Next(Chunk &output_chunk) {
    idx_t output_size = 0;

    if (!hash_table_build_) {
        hash_table_build_ = true;
        BuildHashTable();
    }

    auto &probe_child_operator = child_operators_[0];
    auto probe_key_attr = probe_child_operator->GetOutputSchema().GetKeyAttrs({probe_column_name_})[0];
    while (output_chunk.size() < exec_ctx_.config_.CHUNK_SUGGEST_SIZE) {
        if (buffer_ptr_ == buffer_.size() && !probe_child_exhausted_) {
            if (probe_child_operator->Next(buffer_) == EXHAUSETED) {
                probe_child_exhausted_ = true;
            }
            buffer_ptr_ = 0;
        }
        if (buffer_ptr_ == buffer_.size()) {
            output_chunk.resize(output_size);
            return EXHAUSETED;
        }

        auto &probe_tuple = buffer_[buffer_ptr_].first;
        buffer_ptr_++;
        auto match_range = pointer_table_.equal_range(probe_tuple.KeyFromTuple(probe_key_attr));
        for (auto match_ite = match_range.first; match_ite != match_range.second; match_ite++) {
            if (output_size == output_chunk.size()) {
                output_chunk.push_back(
                    std::make_pair(UnionTuple(probe_tuple, tuples_.begin() + match_ite->second, width_)
                    , INVALID_ID));
            } else {
                output_chunk[output_size].first = UnionTuple(probe_tuple, tuples_.begin() + match_ite->second, width_);
                output_chunk[output_size].second = INVALID_ID;
            }
        }
    }
    output_chunk.resize(output_size);
    return HAVE_MORE_OUTPUT;
}

void HashJoinOperator::SelfInit() {
    tuple_count_ = 0;
    width_ = child_operators_[1]->GetOutputSchema().size();
    tuples_.clear();
    pointer_table_.clear();
    buffer_.clear();
    buffer_ptr_ = 0;
    probe_child_exhausted_ = false;
    hash_table_build_ = false;
}

void HashJoinOperator::SelfCheck() {
    child_operators_[0]->GetOutputSchema().GetKeyAttrs({probe_column_name_});
    child_operators_[1]->GetOutputSchema().GetKeyAttrs({build_column_name_});
}

void HashJoinOperator::BuildHashTable() {
    auto &build_child_operator = child_operators_[1];
    OperatorState state = HAVE_MORE_OUTPUT;
    Chunk build_chunk;
    const idx_t build_key_attr = build_child_operator->GetOutputSchema().GetKeyAttrs({build_column_name_})[0];
    while (state != EXHAUSETED) {
        state = build_child_operator->Next(build_chunk);
        for (auto &chunk_row : build_chunk) {
            auto &tuple = chunk_row.first;
            tuples_.insert(tuples_.end(), tuple.begin(), tuple.end());
            tuple_count_++;
        }
    }
    // Since usually build side is much smaller than probe side, we reserve much more number of tuples
    // to reduce the probe complexity
    pointer_table_.reserve(tuple_count_ * 4);
    for (idx_t i = 0; i < tuple_count_; i++) {
        pointer_table_.insert(std::make_pair(tuples_[i * width_ + build_key_attr], i * width_));
    }
}

}