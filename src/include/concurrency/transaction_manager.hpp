#pragma once

#include "common/typedefs.hpp"
#include "transaction.hpp"

#include <memory>
#include <unordered_map>

namespace babydb {

const idx_t TXN_START_ID = 1ll << 62;

class TransactionManager {
public:
    TransactionManager(IsolationLevel isolation_level = IsolationLevel::SNAPSHOT) : isolation_level_(isolation_level) {}
    //! Create a new transaction.
    std::shared_ptr<Transaction> CreateTxn(std::shared_lock<std::shared_mutex> &&db_lock);
    //! Commit a transaction, return false if aborted.
    bool Commit(Transaction &txn);
    //! Abort a transaction.
    void Abort(Transaction &txn);

private:
    bool VerifyTxn(Transaction &txn);

private:
    std::atomic<idx_t> next_txn_id_{0};

    std::atomic<idx_t> last_commit_ts_{0};

    std::unordered_map<idx_t, std::shared_ptr<Transaction>> txn_map_;

    std::shared_mutex txn_map_latch_;

    std::mutex commit_latch_;

    const IsolationLevel isolation_level_;
};

}