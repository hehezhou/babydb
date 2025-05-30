#include "gtest/gtest.h"

#include "storage/catalog.hpp"

#include "storage/index.hpp"

namespace babydb {

class FakeIndex : public Index {
public:
    FakeIndex(const std::string &name, Table &table, const std::string &key_name)
        : Index(name, table, key_name) {}
    ~FakeIndex() override {}

private:
    void InsertEntry(const data_t &, idx_t) override {};
    void EraseEntry(const data_t &, idx_t) override {};
    idx_t ScanKey(const data_t &) override { return INVALID_ID; };
};

TEST(CatalogTest, BasicTest) {
    Catalog catalog;

    Schema schema{"a"};

    EXPECT_NO_THROW(catalog.CreateTable(std::make_unique<Table>("table0", schema)));

    auto index0_0 = std::make_unique<FakeIndex>("index0_0", catalog.FetchTable("table0"), "a");
    EXPECT_NO_THROW(catalog.CreateIndex(std::move(index0_0)));

    EXPECT_NO_THROW(catalog.CreateTable(std::make_unique<Table>("table1", schema)));

    auto index1_0 = std::make_unique<FakeIndex>("index1_0", catalog.FetchTable("table1"), "a");
    EXPECT_NO_THROW(catalog.CreateIndex(std::move(index1_0)));

    EXPECT_NO_THROW(catalog.CreateTable(std::make_unique<Table>("table2", schema)));

    EXPECT_EQ(catalog.FetchTable("table0").name_, "table0");
    EXPECT_EQ(catalog.FetchTable("table1").name_, "table1");
    EXPECT_EQ(catalog.FetchTable("table2").name_, "table2");
    EXPECT_EQ(catalog.FetchTable("table0").GetIndex(), "index0_0");
    EXPECT_EQ(catalog.FetchTable("table1").GetIndex(), "index1_0");
    EXPECT_EQ(catalog.FetchTable("table2").GetIndex(), "");
    EXPECT_EQ(catalog.FetchIndex("index0_0").name_, "index0_0");

    EXPECT_NO_THROW(catalog.DropIndex("index0_0"));
    EXPECT_EQ(catalog.FetchTable("table0").GetIndex(), "");

    auto index0_1 = std::make_unique<FakeIndex>("index0_1", catalog.FetchTable("table0"), "a");
    EXPECT_NO_THROW(catalog.CreateIndex(std::move(index0_1)));
    EXPECT_EQ(catalog.FetchTable("table0").GetIndex(), "index0_1");

    EXPECT_NO_THROW(catalog.DropTable("table1"));
    EXPECT_ANY_THROW(catalog.FetchIndex("index1_0"));

    EXPECT_NO_THROW(catalog.CreateTable(std::make_unique<Table>("table1", schema)));
}

TEST(CatalogTest, ErrorTest) {
    Catalog catalog;

    Schema schema{"a"};

    EXPECT_NO_THROW(catalog.CreateTable(std::make_unique<Table>("table0", schema)));
    EXPECT_NO_THROW(catalog.CreateTable(std::make_unique<Table>("table1", schema)));

    auto index0_0 = std::make_unique<FakeIndex>("index0_0", catalog.FetchTable("table0"), "a");
    EXPECT_NO_THROW(catalog.CreateIndex(std::move(index0_0)));

    EXPECT_ANY_THROW(catalog.CreateTable(std::make_unique<Table>("table0", schema)));

    auto index0_1 = std::make_unique<FakeIndex>("index0_1", catalog.FetchTable("table0"), "a");
    EXPECT_ANY_THROW(catalog.CreateIndex(std::move(index0_1)));

    index0_0 = std::make_unique<FakeIndex>("index0_0", catalog.FetchTable("table1"), "a");
    EXPECT_ANY_THROW(catalog.CreateIndex(std::move(index0_0)));

    EXPECT_ANY_THROW(catalog.DropTable("table"));
    EXPECT_ANY_THROW(catalog.DropIndex("index"));
    EXPECT_ANY_THROW(catalog.DropTable(""));
    EXPECT_ANY_THROW(catalog.DropIndex(""));

    EXPECT_ANY_THROW(catalog.FetchIndex(""));
    EXPECT_ANY_THROW(catalog.FetchTable(""));
}

}