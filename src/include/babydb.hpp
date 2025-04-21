#pragma once

#include "common/config.hpp"
#include "common/macro.hpp"
#include "common/typedefs.hpp"
#include "common/types.hpp"
#include "concurrency/transaction.hpp"
#include "execution/execution_context.hpp"

#include <memory>
#include <shared_mutex>

namespace babydb {

class Catalog;
struct ConfigGroup;
class TransactionManager;
class Transaction;

//! Instance of BabyDB.
class BabyDB {
public:
    explicit BabyDB(const ConfigGroup &config = ConfigGroup());

    ~BabyDB();

    DISALLOW_COPY(BabyDB);

    void CreateTable(const std::string &table_name, const Schema &schema);

    void DropTable(const std::string &table_name);

    void CreateIndex(const std::string &index_name, const std::string &table_name, const std::string &key_column,
                     IndexType index_type);

    void DropIndex(const std::string &index_name);

    std::shared_ptr<Transaction> CreateTxn();

    bool Commit(Transaction &txn);

    void Abort(Transaction &txn);

    const Catalog& GetCatalog() {
        return *catalog_;
    }

    const ConfigGroup& GetConfig() {
        return *config_;
    }

    ExecutionContext GetExecutionContext(const std::shared_ptr<Transaction> &txn) {
        return ExecutionContext{*txn, GetCatalog(), GetConfig()};
    }

private:
    std::unique_ptr<Catalog> catalog_;

    std::unique_ptr<TransactionManager> txn_mgr_;

    std::unique_ptr<ConfigGroup> config_;

    std::shared_mutex db_lock_;
};

}