#pragma once

#include "common/typedefs.hpp"

namespace babydb {

struct ConfigGroup {
    idx_t CHUNK_SUGGEST_SIZE = 128;
    IsolationLevel ISOLATION_LEVEL = IsolationLevel::SNAPSHOT;
};

}