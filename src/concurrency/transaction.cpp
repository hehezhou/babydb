#include "concurrency/transaction.hpp"

namespace babydb {

void Transaction::Done() {
    db_lock_.unlock();
}

}