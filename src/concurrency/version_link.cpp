#include "concurrency/version_link.hpp"

#include <atomic>

namespace babydb {

// START: Do not modify this part.

std::atomic<idx_t> current_nodes{0};
std::atomic<idx_t> max_nodes{0};

void RegisterVersionNode() {
    auto update = current_nodes.fetch_add(1, std::memory_order_relaxed) + 1;
    auto expected = max_nodes.load(std::memory_order_relaxed);
    while (update > expected && !max_nodes.compare_exchange_weak(expected, update, std::memory_order_relaxed));
}

void UnregisterVersionNode() {
    current_nodes.fetch_sub(1, std::memory_order_relaxed);
}

// END: Do not modify this part.

}