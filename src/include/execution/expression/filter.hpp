#pragma once

#include "common/typedefs.hpp"

#include <functional>

namespace babydb {

/**
 * Filter
 * Before call Check(tuple), you should call Init(input schema) at least once.
 */
class Filter {
public:
    const Schema keys_schema_;

public:
    Filter(const Schema &keys_schema) : keys_schema_(keys_schema) {}
    virtual ~Filter() {}
    //! return false if the tuple does not satisfy some condition.
    bool Check(const Tuple &tuple) const {
        return CheckInternal(tuple.KeysFromTuple(key_attrs_));
    }

    void Init(const Schema &input_schema) {
        key_attrs_ = input_schema.GetKeyAttrs(keys_schema_);
    }

private:
    virtual bool CheckInternal(Tuple &&check_keys) const = 0;

private:
    std::vector<idx_t> key_attrs_;
};

/**
 * Range Filter
 * Only accept key in [minimum, maximum]
 */
class RangeFilter : public Filter {
public:
    const RangeInfo range_;

public:
    RangeFilter(const std::string &column_name, const RangeInfo &range)
        : Filter({column_name}), range_(range) {}
    ~RangeFilter() override {}

private:
    bool CheckInternal(Tuple &&tuple) const override {
        if (tuple[0] < range_.start || tuple[0] > range_.end) {
            return false;
        }
        if (!range_.contain_start && tuple[0] == range_.start) {
            return false;
        }
        if (!range_.contain_end && tuple[0] == range_.end) {
            return false;
        }
        return true;
    }
};

/**
 * Equal Filter
 * Only accept key equals to <target key>
 */
class EqualFilter : public Filter {
public:
    const data_t target_key_;

public:
    EqualFilter(const std::string &column_name, const data_t &target_key)
        : Filter({column_name}), target_key_(target_key) {}
    ~EqualFilter() override {}

    bool CheckInternal(Tuple &&tuple) const override {
        return tuple[0] == target_key_;
    }
};

/**
 * User Define Filter
 */
class UDFilter : public Filter {
public:
    const std::function<bool(Tuple&&)> udf_;

public:
    UDFilter(const Schema &keys_schema, const std::function<bool(Tuple&&)> &udf)
        : Filter(keys_schema), udf_(udf) {}
    ~UDFilter() override {}

    bool CheckInternal(Tuple &&tuple) const override {
        return udf_(std::move(tuple));
    }
};

}