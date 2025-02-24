#include "babydb.hpp"

#include "common/typedefs.hpp"
#include "storage/catalog.hpp"
#include "storage/index.hpp"
#include "storage/stlmap_index.hpp"
#include "storage/table.hpp"
#include "concurrency/transaction_manager.hpp"

namespace babydb {

BabyDB::BabyDB()
    : catalog_(std::make_unique<Catalog>()), txn_mgr_(std::make_unique<TransactionManager>()) {}

BabyDB::~BabyDB() {
    catalog_.reset();
    txn_mgr_.reset();
}

void BabyDB::CreateTable(const std::string &table_name, const std::vector<std::string> &column_name) {
    std::unique_lock lock(db_lock_);
    catalog_->CreateTable(std::make_unique<Table>(table_name, column_name));
}

void BabyDB::DropTable(const std::string &table_name) {
    std::unique_lock lock(db_lock_);
    catalog_->DropTable(table_name);
}

void BabyDB::CreateIndex(const std::string &index_name, const std::string &table_name, idx_t key_position,
                         IndexType index_type) {
    std::unique_lock lock(db_lock_);
    auto table = catalog_->FetchTable(table_name);
    if (table == nullptr) {
        throw std::logic_error("CREATE INDEX: table does not exist");
    }

    switch (index_type) {
    case Stlmap:
        catalog_->CreateIndex(std::make_unique<StlmapIndex>(index_name, *table, key_position));
        break;
    
    default:
        throw std::logic_error("CREATE INDEX: unknown index type");
    }
}

void BabyDB::DropIndex(const std::string &index_name) {
    std::unique_lock lock(db_lock_);
    catalog_->DropIndex(index_name);
}

std::unique_ptr<Transaction> BabyDB::CreateTxn() {
    return txn_mgr_->CreateTxn(std::unique_lock(db_lock_));
}

bool BabyDB::Commit(Transaction &txn) {
    return txn_mgr_->Commit(txn);
}

void BabyDB::Abort(Transaction &txn) {
    txn_mgr_->Abort(txn);
}

}