#pragma once

#include "common/typedefs.hpp"

#include <functional>

namespace babydb {

/**
 * Filter
 * Before call Calc(tuple), you should call Init(input schema) at least once.
 */
class Projection {
public:
    const Schema keys_schema_;

    const std::string output_name_;

public:
    Projection(const Schema &keys_schema, const std::string &output_name)
        : keys_schema_(keys_schema), output_name_(output_name) {}
    virtual ~Projection() {}
    //! returns the calculated data
    data_t Calc(const Tuple &tuple) const {
        return CalcInternal(tuple.KeysFromTuple(key_attrs_));
    }

    void Init(const Schema &input_schema) {
        key_attrs_ = input_schema.GetKeyAttrs(keys_schema_);
    }

private:
    virtual data_t CalcInternal(Tuple &&check_keys) const = 0;

private:
    std::vector<idx_t> key_attrs_;
};

/**
 * Unit Projection
 * Just output the input
 */
class UnitProjection : public Projection {
public:
    UnitProjection(const std::string &column_name) : Projection({column_name}, column_name) {}

    UnitProjection(const std::string &column_name, const std::string &output_name)
        : Projection({column_name}, output_name) {}

    ~UnitProjection() override {}

private:
    data_t CalcInternal(Tuple &&check_keys) const override {
        return check_keys[0];
    }
};

/**
 * User Define Projection
 * Just output the input
 */
class UDProjection : public Projection {
public:
    const std::function<data_t(Tuple&&)> udf_;

public:
    UDProjection(const Schema &keys_schema, const std::string &output_name, const std::function<data_t(Tuple&&)> &udf)
        : Projection(keys_schema, output_name), udf_(udf) {}

    UDProjection(const std::string &column_name, const std::function<data_t(Tuple&&)> &udf)
        : Projection({column_name}, column_name), udf_(udf) {}

    ~UDProjection() override {}

private:
    data_t CalcInternal(Tuple &&check_keys) const override {
        return udf_(std::move(check_keys));
    }
};

}