#pragma once

namespace babydb {

class Transaction;
class Catalog;
struct ConfigGroup;
struct ExecutionContext {
    Transaction &txn_;
    const Catalog &catalog_;
    const ConfigGroup &config_;
};

}