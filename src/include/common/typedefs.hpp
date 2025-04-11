#pragma once

#include <exception>
#include <stdbool.h>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

namespace babydb {

//! babydb's only data type
typedef uint64_t data_t;
const data_t DATA_MIN = 0;
const data_t DATA_MAX = INT64_MAX;
//! babydb's index type
typedef uint64_t idx_t;

[[maybe_unused]] const std::string INVALID_NAME = "";
[[maybe_unused]] const idx_t INVALID_ID = static_cast<idx_t>(-1);

const idx_t TXN_START_ID = 1ll << 62;

struct RangeInfo {
    data_t start;
    data_t end;
    bool contain_start = true;
    bool contain_end = true;
};

class Tuple : public std::vector<data_t> {
public:
    using std::vector<data_t>::vector;

    Tuple KeysFromTuple(const std::vector<idx_t> &key_attrs) const {
        Tuple result;
        for (auto position : key_attrs) {
            result.push_back(this->operator[](position));
        }
        return result;
    }

    data_t KeyFromTuple(idx_t key_attr) const {
        return this->operator[](key_attr);
    }
};

//! Since babydb only has one type, the schema of the table is just the column names.
class Schema : public std::vector<std::string> {
public:
    using std::vector<std::string>::vector;

    std::vector<idx_t> GetKeyAttrs(const Schema &key_schema) const {
        std::vector<idx_t> result;
        for (auto cname : key_schema) {
            result.push_back(GetKeyAttr(cname));
        }
        return result;
    }

    idx_t GetKeyAttr(const std::string &column_name) const {
        auto position = this->Find(column_name);
        if (position == INVALID_ID) {
            throw std::logic_error("Invalid key schema");
        }
        return position;
    }

private:
    idx_t Find(const std::string &cname) const {
        for (idx_t i = 0; i < cname.size(); i++) {
            if (this->operator[](i) == cname) {
                return i;
            }
        }
        return INVALID_ID;
    }
};

class TaintedException : public std::exception {
public:
    TaintedException(std::string reason) : reason_(std::move(reason)) {}
    const char* what() const throw() {
        return reason_.c_str();
    }

private:
    std::string reason_;
};

enum class IsolationLevel : uint8_t {
    SNAPSHOT,
    SERIALIZABLE
};

}