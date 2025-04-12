#pragma once

#include "common/typedefs.hpp"
#include "common/macro.hpp"

#include <atomic>
#include <mutex>
#include <shared_mutex>

namespace babydb {

class VersionSkipList;

//! Transaction State
enum TransactionState { RUNNING, TAINTED, COMMITED, ABORTED };

//! The concurrency control is just lock the whole database.
class Transaction {
public:
    explicit Transaction(idx_t txn_id, idx_t read_ts, idx_t gc_ts, std::shared_lock<std::shared_mutex> &&db_lock)
        : txn_id_(txn_id), read_ts_(read_ts), gc_ts_(gc_ts), db_lock_(std::move(db_lock)) {}

    DISALLOW_COPY(Transaction);

    ~Transaction() = default;

    const idx_t txn_id_;

    const idx_t read_ts_;

    const idx_t gc_ts_;

public:
    void SetTainted() {
        state_ = TAINTED;
    }

    idx_t GetCommitTs() {
        return commit_ts_;
    }

    TransactionState GetState() const {
        return state_.load();
    }

    void AddModifiedRow(VersionSkipList *row_list) {
        modified_rows_.push_back(row_list);
    }

    void AddReadRow(VersionSkipList *row_list) {
        read_rows_.push_back(row_list);
    }

    bool ReadOnly() {
        return modified_rows_.empty();
    }

private:
    void Done();

private:
    std::atomic<TransactionState> state_{RUNNING};

    std::shared_lock<std::shared_mutex> db_lock_;

    idx_t commit_ts_{INVALID_ID};

    std::vector<VersionSkipList*> modified_rows_;

    std::vector<VersionSkipList*> read_rows_;

friend class TransactionManager;
};

}