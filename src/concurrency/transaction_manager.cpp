#include "concurrency/transaction_manager.hpp"

namespace babydb {

std::shared_ptr<Transaction> TransactionManager::CreateTxn(std::shared_lock<std::shared_mutex> &&db_lock) {
    std::unique_lock lock(txn_map_latch_);
    auto result = std::make_shared<Transaction>(next_txn_id_, last_commit_ts_, last_commit_ts_, std::move(db_lock));
    txn_map_.insert(std::make_pair(next_txn_id_, result));
    next_txn_id_++;
    return result;
}

bool TransactionManager::VerifyTxn([[maybe_unused]] Transaction &txn) {
    // Project 2: Verify the txn
    return true;
}

bool TransactionManager::Commit(Transaction &txn) {
    if (txn.state_ != RUNNING) {
        throw std::logic_error("Try to commit a not running transaction."); 
    }
    std::unique_lock commit_lock(commit_latch_);
    if (!VerifyTxn(txn)) {
        commit_lock.unlock();
        Abort(txn);
        return false;
    }
    // Project 2: Commit the txn

    std::unique_lock map_lock(txn_map_latch_);
    txn.commit_ts_ = ++last_commit_ts_;
    txn.state_ = COMMITED;
    txn.Done();
    txn_map_.erase(txn.txn_id_);
    return true;
}
//! The txn should roll back. We do not implement it.
void TransactionManager::Abort(Transaction &txn) {
    if (txn.state_ != RUNNING && txn.state_ != TAINTED) {
        throw std::logic_error("Try to abort a not running or tainted transaction."); 
    }
    // Project 2: Rollback the txn

    std::unique_lock map_lock(txn_map_latch_);
    txn.state_ = ABORTED;
    txn.Done();
    txn_map_.erase(txn.txn_id_);

    throw std::logic_error("Txn Abort is not implemented.");
}

}