#include "gtest/gtest.h"

#include "babydb.hpp"
#include "execution/insert_operator.hpp"
#include "execution/value_operator.hpp"
#include "execution/update_operator.hpp"
#include "execution/range_index_scan_operator.hpp"
#include "execution/projection_operator.hpp"

#include <algorithm>
#include <random>
#include <thread>

namespace babydb {

extern std::atomic<idx_t> current_nodes;
extern std::atomic<idx_t> max_nodes;

static const idx_t seed = 42;

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

static bool AbortOrCommit(BabyDB &db, Transaction &txn) {
    if (txn.GetState() == TAINTED) {
        db.Abort(txn);
        return false;
    }
    return db.Commit(txn);
}

static std::vector<idx_t> GenKeySet(idx_t n, idx_t seed) {
    std::mt19937_64 rnd(seed);
    std::vector<idx_t> keyset;
    do {
        for (idx_t i = 0; i < n; i++) {
            keyset.push_back(rnd() >> 1);
        }
        std::sort(keyset.begin(), keyset.end());
        keyset.erase(std::unique(keyset.begin(), keyset.end()), keyset.end());
    } while (keyset.size() < n);
    std::shuffle(keyset.begin(), keyset.end(), rnd);
    return keyset;
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
}

TEST(Project2Test, NonRepeatableRead) {
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

    auto update_operator_1 = UpdateOperator(db.GetExecutionContext(txn1),
        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn1), read_operator_1,
            std::make_unique<UDProjection>("payload", [](Tuple &&a) { return a[0] + 1; })));
    EXPECT_EQ(RunOperator(*read_operator_1), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(*read_operator_2), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(update_operator_1), std::vector<Tuple>());
    EXPECT_EQ(RunOperator(*read_operator_1), (std::vector<Tuple>{Tuple{0, 1}}));
    EXPECT_EQ(RunOperator(*read_operator_2), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(db.Commit(*txn1), true);
    EXPECT_EQ(RunOperator(*read_operator_2), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(db.Commit(*txn2), true);
    EXPECT_GT(txn1->GetCommitTs(), txn2->GetCommitTs());
}

TEST(Project2Test, TaintedTest) {
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

    auto update_operator_1 = UpdateOperator(db.GetExecutionContext(txn1),
        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn1), read_operator_1,
            std::make_unique<UDProjection>("payload", [](Tuple &&a) { return a[0] + 1; })));
    auto update_operator_2 = UpdateOperator(db.GetExecutionContext(txn2),
        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn2), read_operator_2,
            std::make_unique<UDProjection>("payload", [](Tuple &&a) { return a[0] + 1; })));
    EXPECT_EQ(RunOperator(*read_operator_1), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(*read_operator_2), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(update_operator_1), std::vector<Tuple>());
    EXPECT_THROW(RunOperator(update_operator_2), TaintedException);
    EXPECT_EQ(RunOperator(*read_operator_1), (std::vector<Tuple>{Tuple{0, 1}}));
    EXPECT_EQ(txn2->GetState(), TAINTED);
    EXPECT_EQ(db.Commit(*txn1), true);
}

TEST(Project2Test, AbortTest) {
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
    auto txn3 = db.CreateTxn();

    auto read_operator_1 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn1), "t0", schema, schema, "t0_i0", RangeInfo{0, 0});
    auto update_operator_1 = UpdateOperator(db.GetExecutionContext(txn1),
        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn1), read_operator_1,
            std::make_unique<UDProjection>("payload", [](Tuple &&a) { return a[0] + 1; })));
    EXPECT_EQ(RunOperator(*read_operator_1), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(update_operator_1), std::vector<Tuple>());
    EXPECT_EQ(RunOperator(*read_operator_1), (std::vector<Tuple>{Tuple{0, 1}}));
    EXPECT_NO_THROW(db.Abort(*txn1));

    auto txn2 = db.CreateTxn();
    
    auto read_operator_3 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn3), "t0", schema, schema, "t0_i0", RangeInfo{0, 0});
    auto update_operator_3 = UpdateOperator(db.GetExecutionContext(txn3),
        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn3), read_operator_3,
            std::make_unique<UDProjection>("payload", [](Tuple &&a) { return a[0] + 3; })));

    EXPECT_EQ(RunOperator(*read_operator_3), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(update_operator_3), std::vector<Tuple>());
    EXPECT_EQ(RunOperator(*read_operator_3), (std::vector<Tuple>{Tuple{0, 3}}));
    EXPECT_NO_THROW(db.Abort(*txn3));

    auto read_operator_2 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn2), "t0", schema, schema, "t0_i0", RangeInfo{0, 0});
    auto update_operator_2 = UpdateOperator(db.GetExecutionContext(txn2),
        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn2), read_operator_2,
            std::make_unique<UDProjection>("payload", [](Tuple &&a) { return a[0] + 2; })));

    EXPECT_EQ(RunOperator(*read_operator_2), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(update_operator_2), std::vector<Tuple>());
    EXPECT_EQ(RunOperator(*read_operator_2), (std::vector<Tuple>{Tuple{0, 2}}));
    EXPECT_EQ(db.Commit(*txn2), true);
}

TEST(Project2Test, SerializableTest) {
    BabyDB db(ConfigGroup{.ISOLATION_LEVEL = IsolationLevel::SERIALIZABLE});
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

    auto read_operator_1_0 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn1), "t0", schema, schema, "t0_i0", RangeInfo{0, 0});
    auto read_operator_1_10 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn1), "t0", schema, schema, "t0_i0", RangeInfo{10, 10});
    auto update_operator_1 = UpdateOperator(db.GetExecutionContext(txn1),
        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn1), read_operator_1_0,
            std::make_unique<UDProjection>("payload", [](Tuple &&a) { return a[0] + 1; })));

    auto read_operator_2_0 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn2), "t0", schema, schema, "t0_i0", RangeInfo{0, 0});
    auto read_operator_2_10 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn2), "t0", schema, schema, "t0_i0", RangeInfo{10, 10});
    auto update_operator_2 = UpdateOperator(db.GetExecutionContext(txn2),
        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn2), read_operator_2_10,
            std::make_unique<UDProjection>("payload", [](Tuple &&a) { return a[0] + 1; })));

    EXPECT_EQ(RunOperator(*read_operator_1_0), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(*read_operator_1_10), (std::vector<Tuple>{Tuple{10, 10}}));
    EXPECT_EQ(RunOperator(*read_operator_2_0), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(*read_operator_2_10), (std::vector<Tuple>{Tuple{10, 10}}));
    EXPECT_EQ(RunOperator(update_operator_1), std::vector<Tuple>());
    EXPECT_EQ(RunOperator(update_operator_2), std::vector<Tuple>());
    EXPECT_EQ(RunOperator(*read_operator_1_0), (std::vector<Tuple>{Tuple{0, 1}}));
    EXPECT_EQ(RunOperator(*read_operator_1_10), (std::vector<Tuple>{Tuple{10, 10}}));
    EXPECT_EQ(RunOperator(*read_operator_2_0), (std::vector<Tuple>{Tuple{0, 0}}));
    EXPECT_EQ(RunOperator(*read_operator_2_10), (std::vector<Tuple>{Tuple{10, 11}}));
    EXPECT_EQ(static_cast<idx_t>(AbortOrCommit(db, *txn1)) + static_cast<idx_t>(AbortOrCommit(db, *txn2)), 1);

    auto txn3 = db.CreateTxn();
    auto read_operator_3 = std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn3), "t0", schema, schema, "t0_i0", RangeInfo{0, 10});
    std::vector<Tuple> result;
    EXPECT_NO_THROW(result = RunOperator(*read_operator_3));
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].size(), 2);
    EXPECT_EQ(result[1].size(), 2);
    EXPECT_EQ(result[0][1] + result[1][1], 11);
}

TEST(Project2Test, BankSystemTest) {
    BabyDB db(ConfigGroup{});
    Schema schema{"name", "balance"};
    db.CreateTable("t0", schema);
    db.CreateIndex("t0_i0", "t0", "name", IndexType::ART);
    const idx_t n = 100, total_tasks = 100000, thread_count = 8;
    std::vector<idx_t> names = GenKeySet(n, seed * 2);
    std::vector<Tuple> init_tuples;
    for (idx_t i = 0; i < n; i++) {
        init_tuples.push_back(Tuple{names[i], total_tasks});
    }
    auto init_txn = db.CreateTxn();
    auto init_operator = InsertOperator(db.GetExecutionContext(init_txn),
        std::make_shared<ValueOperator>(db.GetExecutionContext(init_txn), schema, std::move(init_tuples)), "t0");
    EXPECT_EQ(RunOperator(init_operator), std::vector<Tuple>());
    EXPECT_EQ(db.Commit(*init_txn), true);

    std::atomic<idx_t> executed_tasks(0), failed_task(0);
    auto WorkThread = [&db, &executed_tasks, &failed_task, n, total_tasks, &schema, &names](idx_t thread_id) {
        std::mt19937_64 rnd(thread_id + seed);
        std::uniform_int_distribution<idx_t> dist_source(0, n / 2 - 1);
        std::uniform_int_distribution<idx_t> dist_target(n / 2, n - 1);
        idx_t local_tasks = 0;
        while (executed_tasks.fetch_add(1) < total_tasks) {
            bool success;
            do {
                std::shared_ptr<Transaction> txn;
                try {
                    auto source = names[dist_source(rnd)];
                    auto target = names[dist_target(rnd)];
                    txn = db.CreateTxn();
                    auto read_operator_source =
                        std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{source, source});
                    auto read_operator_target =
                        std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{target, target});
                    auto update_operator_source = UpdateOperator(db.GetExecutionContext(txn),
                        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn), read_operator_source,
                            std::make_unique<UDProjection>("balance", [](Tuple &&a) { return a[0] - 1; })));
                    auto update_operator_target = UpdateOperator(db.GetExecutionContext(txn),
                        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn), read_operator_target,
                            std::make_unique<UDProjection>("balance", [](Tuple &&a) { return a[0] + 1; })));
                    ASSERT_EQ(RunOperator(*read_operator_source).size(), 1);
                    ASSERT_EQ(RunOperator(*read_operator_target).size(), 1);
                    RunOperator(update_operator_source);
                    RunOperator(update_operator_target);
                    success = db.Commit(*txn);
                } catch (const TaintedException &e) {
                    success = false;
                    db.Abort(*txn);
                }
                if (!success) {
                    ASSERT_LE(failed_task.fetch_add(1) + 1, total_tasks) << "too many rollback";
                }
            } while (!success);
            if (++local_tasks % n == 0) {
                auto txn = db.CreateTxn();
                auto read_operator =
                    std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{DATA_MIN, DATA_MAX});
                idx_t sum = 0;
                auto result = RunOperator(*read_operator);
                ASSERT_EQ(result.size(), n);
                for (auto &row : result) {
                    ASSERT_EQ(row.size(), 2);
                    sum += row[1];
                }
                ASSERT_EQ(sum, n * total_tasks);
                db.Commit(*txn);
            }
        }
    };
    std::vector<std::thread> thread_pool;
    for (idx_t i = 0; i < thread_count - 1; i++) {
        thread_pool.emplace_back(WorkThread, i);
    }
    WorkThread(thread_count - 1);
    for (auto &thr : thread_pool) {
        thr.join();
    }
    auto txn = db.CreateTxn();
    auto read_operator =
        std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{DATA_MIN, DATA_MAX});
    idx_t sum = 0;
    auto result = RunOperator(*read_operator);
    ASSERT_EQ(result.size(), n);
    for (auto &row : result) {
        ASSERT_EQ(row.size(), 2);
        if (std::find(names.begin(), names.end(), row[0]) < names.begin() + n / 2) {
            sum += row[1];
        }
    }
    ASSERT_EQ(sum, (n / 2 - 1) * total_tasks);
    db.Commit(*txn);
    GTEST_LOG_(INFO) << std::to_string(failed_task.load()) + " txns has been aborted.";
}

TEST(Project2Test, SellSystemTest) {
    BabyDB db(ConfigGroup{.ISOLATION_LEVEL = IsolationLevel::SERIALIZABLE});
    Schema price_schema{"key", "price"};
    db.CreateTable("t_price", price_schema);
    db.CreateIndex("t_price_i", "t_price", "key", IndexType::ART);

    Schema goods_schema{"key", "goods"};
    db.CreateTable("t_goods", goods_schema);
    db.CreateIndex("t_goods_i", "t_goods", "key", IndexType::ART);

    Schema selled_schema{"key", "selled"};
    db.CreateTable("t_selled", selled_schema);
    db.CreateIndex("t_selled_i", "t_selled", "key", IndexType::ART);

    const idx_t total_tasks = 100000, thread_count = 3;

    auto init_txn = db.CreateTxn();
    std::vector<Tuple> init_tuples;

    for (idx_t i = 0; i < thread_count; i++) {
        init_tuples.push_back(Tuple{i, 0});
    }
    auto init_operator = std::make_shared<InsertOperator>(db.GetExecutionContext(init_txn),
        std::make_shared<ValueOperator>(db.GetExecutionContext(init_txn), selled_schema, std::move(init_tuples)), "t_selled");
    EXPECT_EQ(RunOperator(*init_operator), std::vector<Tuple>());

    init_tuples.clear();
    init_tuples.push_back(Tuple{0, 0});
    init_operator = std::make_shared<InsertOperator>(db.GetExecutionContext(init_txn),
        std::make_shared<ValueOperator>(db.GetExecutionContext(init_txn), price_schema, std::move(init_tuples)), "t_price");
    EXPECT_EQ(RunOperator(*init_operator), std::vector<Tuple>());

    init_tuples.clear();
    init_tuples.push_back(Tuple{0, total_tasks});
    init_operator = std::make_shared<InsertOperator>(db.GetExecutionContext(init_txn),
        std::make_shared<ValueOperator>(db.GetExecutionContext(init_txn), goods_schema, std::move(init_tuples)), "t_goods");
    EXPECT_EQ(RunOperator(*init_operator), std::vector<Tuple>());

    EXPECT_EQ(db.Commit(*init_txn), true);

    std::atomic<idx_t> executed_tasks(0), failed_task(0);
    std::atomic<data_t> expected_result(0);
    auto WorkThread = [&](idx_t thread_id) {
        idx_t local_tasks = 0;
        while (executed_tasks.fetch_add(1) < total_tasks) {
            bool success;
            do {
                std::shared_ptr<Transaction> txn;
                try {
                    txn = db.CreateTxn();
                    auto read_operator_price = std::make_shared<RangeIndexScanOperator>(
                        db.GetExecutionContext(txn), "t_price", Schema{"price"}, Schema{"price"}, "t_price_i", RangeInfo{0, 0});
                    auto result = RunOperator(*read_operator_price);
                    ASSERT_EQ(result.size(), 1);
                    ASSERT_EQ(result[0].size(), 1);
                    auto price = result[0][0];

                    auto read_operator_selled = std::make_shared<RangeIndexScanOperator>(
                        db.GetExecutionContext(txn), "t_selled", selled_schema, selled_schema, "t_selled_i", RangeInfo{thread_id, thread_id});
                    auto update_operator_selled = UpdateOperator(db.GetExecutionContext(txn),
                        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn), read_operator_selled,
                            std::make_unique<UDProjection>("selled", [price](Tuple &&a) { return a[0] + price; })));
                    RunOperator(update_operator_selled);

                    auto read_operator_goods = std::make_shared<RangeIndexScanOperator>(
                        db.GetExecutionContext(txn), "t_goods", goods_schema, goods_schema, "t_goods_i", RangeInfo{0, 0});
                    auto update_operator_goods = UpdateOperator(db.GetExecutionContext(txn),
                        std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn), read_operator_goods,
                            std::make_unique<UDProjection>("goods", [price](Tuple &&a) { return a[0] - 1; })));
                    RunOperator(update_operator_goods);

                    success = db.Commit(*txn);
                } catch (const TaintedException &e) {
                    success = false;
                    db.Abort(*txn);
                }
                if (!success) {
                    ASSERT_LE(failed_task.fetch_add(1) + 1, total_tasks * 5) << "too many rollback";
                }
            } while (!success);
            if (++local_tasks % 100 == 0) {
                bool success;
                do {
                    std::shared_ptr<Transaction> txn;
                    try {
                        txn = db.CreateTxn();
                        auto read_operator_price = std::make_shared<RangeIndexScanOperator>(
                            db.GetExecutionContext(txn), "t_price", price_schema, price_schema, "t_price_i", RangeInfo{0, 0});
                        auto update_operator_price = UpdateOperator(db.GetExecutionContext(txn),
                            std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn), read_operator_price,
                                std::make_unique<UDProjection>("price", [](Tuple &&a) { return a[0] + 1; })));
                        RunOperator(update_operator_price);

                        auto read_operator_goods = std::make_shared<RangeIndexScanOperator>(
                            db.GetExecutionContext(txn), "t_goods", Schema{"goods"}, Schema{"goods"}, "t_goods_i", RangeInfo{0, 0});
                        auto result = RunOperator(*read_operator_goods);
                        ASSERT_EQ(result.size(), 1);
                        ASSERT_EQ(result[0].size(), 1);

                        success = db.Commit(*txn);
                        if (success) {
                            expected_result.fetch_add(result[0][0]);
                        }
                    } catch (const TaintedException &e) {
                        success = false;
                        db.Abort(*txn);
                    }
                    if (!success) {
                        ASSERT_LE(failed_task.fetch_add(1) + 1, total_tasks * 5) << "too many rollback";
                    }
                } while (!success);
            }
        }
    };
    std::vector<std::thread> thread_pool;
    for (idx_t i = 0; i < thread_count - 1; i++) {
        thread_pool.emplace_back(WorkThread, i);
    }
    WorkThread(thread_count - 1);
    for (auto &thr : thread_pool) {
        thr.join();
    }
    auto txn = db.CreateTxn();
    auto read_operator_selled = std::make_shared<RangeIndexScanOperator>(
        db.GetExecutionContext(txn), "t_selled", Schema{"selled"}, Schema{"selled"}, "t_selled_i", RangeInfo{0, thread_count - 1});
    auto result = RunOperator(*read_operator_selled);
    ASSERT_EQ(result.size(), thread_count);
    data_t selled_sum = 0;
    for (auto &row : result) {
        ASSERT_EQ(row.size(), 1);
        selled_sum += row[0];
    }
    EXPECT_EQ(selled_sum, expected_result);
    EXPECT_EQ(db.Commit(*txn), true);
    GTEST_LOG_(INFO) << std::to_string(failed_task.load()) + " txns has been aborted.";
}

TEST(Project2Test, GCTest) {
    BabyDB db(ConfigGroup{});
    Schema schema{"name", "balance"};
    db.CreateTable("t0", schema);
    db.CreateIndex("t0_i0", "t0", "name", IndexType::ART);

    const idx_t n = 1000, total_tasks = 100000, thread_count = 2;
    std::vector<idx_t> names = GenKeySet(n, seed * 3);
    std::vector<Tuple> init_tuples;
    for (idx_t i = 0; i < n; i++) {
        init_tuples.push_back(Tuple{names[i], 2 * total_tasks});
    }
    auto init_txn = db.CreateTxn();
    auto init_operator = InsertOperator(db.GetExecutionContext(init_txn),
        std::make_shared<ValueOperator>(db.GetExecutionContext(init_txn), schema, std::move(init_tuples)), "t0");
    EXPECT_EQ(RunOperator(init_operator), std::vector<Tuple>());
    EXPECT_EQ(db.Commit(*init_txn), true);

    current_nodes.store(0);
    max_nodes.store(0);
    {
        std::atomic<idx_t> executed_tasks(0), failed_task(0);
        auto WorkThread = [&db, &executed_tasks, &failed_task, n, total_tasks, &schema, &names](idx_t thread_id) {
            std::mt19937_64 rnd(thread_id + seed);
            std::uniform_int_distribution<idx_t> dist_source(0, n / 2 - 1);
            std::uniform_int_distribution<idx_t> dist_target(n / 2, n - 1);
            idx_t local_tasks = 0;
            while (executed_tasks.fetch_add(1) < total_tasks) {
                bool success;
                do {
                    std::shared_ptr<Transaction> txn;
                    try {
                        auto source = names[dist_source(rnd)];
                        auto target = names[dist_target(rnd)];
                        txn = db.CreateTxn();
                        auto read_operator_source =
                            std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{source, source});
                        auto read_operator_target =
                            std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{target, target});
                        auto update_operator_source = UpdateOperator(db.GetExecutionContext(txn),
                            std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn), read_operator_source,
                                std::make_unique<UDProjection>("balance", [](Tuple &&a) { return a[0] - 1; })));
                        auto update_operator_target = UpdateOperator(db.GetExecutionContext(txn),
                            std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn), read_operator_target,
                                std::make_unique<UDProjection>("balance", [](Tuple &&a) { return a[0] + 1; })));
                        ASSERT_EQ(RunOperator(*read_operator_source).size(), 1);
                        ASSERT_EQ(RunOperator(*read_operator_target).size(), 1);
                        RunOperator(update_operator_source);
                        RunOperator(update_operator_target);
                        success = db.Commit(*txn);
                    } catch (const TaintedException &e) {
                        success = false;
                        db.Abort(*txn);
                    }
                    if (!success) {
                        ASSERT_LE(failed_task.fetch_add(1) + 1, total_tasks) << "too many rollback";
                    }
                } while (!success);
                if (++local_tasks % n == 0) {
                    auto txn = db.CreateTxn();
                    auto read_operator =
                        std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{DATA_MIN, DATA_MAX});
                    idx_t sum = 0;
                    auto result = RunOperator(*read_operator);
                    ASSERT_EQ(result.size(), n);
                    for (auto &row : result) {
                        ASSERT_EQ(row.size(), 2);
                        sum += row[1];
                    }
                    ASSERT_EQ(sum, n * 2 * total_tasks);
                    db.Commit(*txn);
                }
            }
        };
        std::vector<std::thread> thread_pool;
        for (idx_t i = 0; i < thread_count - 1; i++) {
            thread_pool.emplace_back(WorkThread, i);
        }
        WorkThread(thread_count - 1);
        for (auto &thr : thread_pool) {
            thr.join();
        }
        GTEST_LOG_(INFO) << "Max version nodes: " + std::to_string(max_nodes.load());
        EXPECT_LE(max_nodes.load(), 2 * n);
        auto txn = db.CreateTxn();
        auto read_operator =
            std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{DATA_MIN, DATA_MAX});
        idx_t sum = 0;
        auto result = RunOperator(*read_operator);
        ASSERT_EQ(result.size(), n);
        for (auto &row : result) {
            ASSERT_EQ(row.size(), 2);
            if (std::find(names.begin(), names.end(), row[0]) < names.begin() + n / 2) {
                sum += row[1];
            }
        }
        ASSERT_EQ(sum, (n / 2 * 2 - 1) * total_tasks);
        db.Commit(*txn);
    }
    {
        std::atomic<idx_t> executed_tasks(0), failed_task(0);
        std::vector<std::shared_ptr<Transaction>> blocked_txns[thread_count];
        auto WorkThread = [&db, &executed_tasks, &failed_task, n, total_tasks, &schema, &names, &blocked_txns](idx_t thread_id) {
            std::mt19937_64 rnd(thread_id + seed);
            std::uniform_int_distribution<idx_t> dist_source(0, n / 2 - 1);
            std::uniform_int_distribution<idx_t> dist_target(n / 2, n - 1);
            idx_t local_tasks = 0;
            while (executed_tasks.fetch_add(1) < total_tasks) {
                blocked_txns[thread_id].push_back(db.CreateTxn());
                bool success;
                do {
                    std::shared_ptr<Transaction> txn;
                    try {
                        auto source = names[dist_source(rnd)];
                        auto target = names[dist_target(rnd)];
                        txn = db.CreateTxn();
                        auto read_operator_source =
                            std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{source, source});
                        auto read_operator_target =
                            std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{target, target});
                        auto update_operator_source = UpdateOperator(db.GetExecutionContext(txn),
                            std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn), read_operator_source,
                                std::make_unique<UDProjection>("balance", [](Tuple &&a) { return a[0] - 1; })));
                        auto update_operator_target = UpdateOperator(db.GetExecutionContext(txn),
                            std::make_shared<ProjectionOperator>(db.GetExecutionContext(txn), read_operator_target,
                                std::make_unique<UDProjection>("balance", [](Tuple &&a) { return a[0] + 1; })));
                        ASSERT_EQ(RunOperator(*read_operator_source).size(), 1);
                        ASSERT_EQ(RunOperator(*read_operator_target).size(), 1);
                        RunOperator(update_operator_source);
                        RunOperator(update_operator_target);
                        success = db.Commit(*txn);
                    } catch (const TaintedException &e) {
                        success = false;
                        db.Abort(*txn);
                    }
                    if (!success) {
                        ASSERT_LE(failed_task.fetch_add(1) + 1, total_tasks) << "too many rollback";
                    }
                } while (!success);
                if (++local_tasks % n == 0) {
                    auto txn = db.CreateTxn();
                    auto read_operator =
                        std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{DATA_MIN, DATA_MAX});
                    idx_t sum = 0;
                    auto result = RunOperator(*read_operator);
                    ASSERT_EQ(result.size(), n);
                    for (auto &row : result) {
                        ASSERT_EQ(row.size(), 2);
                        sum += row[1];
                    }
                    ASSERT_EQ(sum, n * 2 * total_tasks);
                    db.Commit(*txn);
                }
            }
        };
        std::vector<std::thread> thread_pool;
        for (idx_t i = 0; i < thread_count - 1; i++) {
            thread_pool.emplace_back(WorkThread, i);
        }
        WorkThread(thread_count - 1);
        for (auto &thr : thread_pool) {
            thr.join();
        }
        EXPECT_GE(current_nodes.load(), total_tasks * 2);
        for (auto &buffer : blocked_txns) {
            for (auto &txn : buffer) {
                db.Commit(*txn);
            }
        }
        auto txn = db.CreateTxn();
        auto read_operator =
            std::make_shared<RangeIndexScanOperator>(db.GetExecutionContext(txn), "t0", schema, schema, "t0_i0", RangeInfo{DATA_MIN, DATA_MAX});
        idx_t sum = 0;
        auto result = RunOperator(*read_operator);
        ASSERT_EQ(result.size(), n);
        for (auto &row : result) {
            ASSERT_EQ(row.size(), 2);
            if (std::find(names.begin(), names.end(), row[0]) < names.begin() + n / 2) {
                sum += row[1];
            }
        }
        ASSERT_EQ(sum, (n / 2 * 2 - 2) * total_tasks);
        db.Commit(*txn);
    }
}

}