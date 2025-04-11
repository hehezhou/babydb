#include "gtest/gtest.h"

#include "babydb.hpp"
#include "execution/insert_operator.hpp"
#include "execution/value_operator.hpp"
#include "execution/update_operator.hpp"
#include "execution/range_index_scan_operator.hpp"
#include "execution/projection_operator.hpp"

namespace babydb {

static std::vector<Tuple> RunOperator(Operator &test_operator, bool sort_output = true) {
    test_operator.Check();
    test_operator.Init();
    std::vector<Tuple> results;
    Chunk chunk;
    auto operator_state = OperatorState::HAVE_MORE_OUTPUT;
    while (operator_state != EXHAUSETED) {
        operator_state = test_operator.Next(chunk);
        for (auto &row : chunk) {
            results.push_back(row.first);
        }
    }
    if (sort_output) {
        std::sort(results.begin(), results.end());
    }
    return results;
}

TEST(Project2Test, DirtyRead) {
    BabyDB db;
    Schema schema{"key", "payload"};
    db.CreateTable("t0", schema);
    db.CreateIndex("t0_i0", "t0", "key", IndexType::ART);
    std::vector<Tuple> init_tuples;
    init_tuples.push_back(Tuple{0, 0});
    init_tuples.push_back(Tuple{10, 10});
    auto init_txn = db.CreateTxn();
    auto init_operator = InsertOperator(db.GetExecutionContext(init_txn),
        std::make_shared<ValueOperator>(db.GetExecutionContext(init_txn), schema, std::move(init_tuples)), "t0");
    EXPECT_EQ(RunOperator(init_operator), std::vector<Tuple>());
    EXPECT_EQ(db.Commit(*init_txn), true);

    auto txn1 = db.CreateTxn();
    auto txn2 = db.CreateTxn();
    auto read_operator_1 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn1), "t0", schema, schema, "t0_i0", RangeInfo{0, 0});
    auto read_operator_2 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn2), "t0", schema, schema, "t0_i0", RangeInfo{0, 0});

    std::vector<std::unique_ptr<Projection>> payload_plus_1;
    auto update_operator_1 = UpdateOperator(db.GetExecutionContext(txn1),
        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn1), read_operator_1,
            std::make_unique<UDProjection>("payload", [](Tuple &&a) { return a[0] + 1; })));
    EXPECT_EQ(RunOperator(*read_operator_1), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(*read_operator_2), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(update_operator_1), std::vector<Tuple>());
    EXPECT_EQ(RunOperator(*read_operator_1), (std::vector<Tuple>{Tuple{0, 1}}));
    EXPECT_EQ(RunOperator(*read_operator_2), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(db.Commit(*txn1), true);
    EXPECT_EQ(db.Commit(*txn2), true);
    EXPECT_LT(txn1->GetCommitTs(), txn2->GetCommitTs());
}

}