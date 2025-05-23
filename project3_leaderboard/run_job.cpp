#include "babydb.hpp"
#include "execution/operator.hpp"
#include "execution/seq_scan_operator.hpp"
#include "execution/hash_join_operator.hpp"
#include "execution/insert_operator.hpp"
#include "execution/value_operator.hpp"

#include <chrono>
#include <fstream>
#include <iostream>

using namespace babydb;

template<class T>
static void Read(std::ifstream &data_file, T &data) {
    auto address = reinterpret_cast<char*>(&data);
    data_file.read(address, sizeof(T));
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    std::reverse(address, address + sizeof(T));
#endif
}

enum class OPType : uint8_t {
    hash_join = 0,
    scan = 1
};

static void BuildTable(std::ifstream &data_file, BabyDB &db) {
    OPType op_type;
    Read(data_file, op_type);
    if (op_type == OPType::hash_join) {
        idx_t probe_cid, build_cid;
        Read(data_file, probe_cid);
        Read(data_file, build_cid);
        BuildTable(data_file, db);
        BuildTable(data_file, db);
    } else if (op_type == OPType::scan) {
        auto table_name = std::to_string(data_file.tellg());
        idx_t row, col;
        Read(data_file, row);
        Read(data_file, col);
        Schema schema;
        for (idx_t i = 0; i < col; i++) {
            schema.push_back(std::to_string(i));
        }
        db.CreateTable(table_name, schema);

        std::vector<Tuple> tuples;
        for (idx_t i = 0; i < row; i++) {
            Tuple tuple;
            for (idx_t j = 0; j < col; j++) {
                data_t data;
                Read(data_file, data);
                tuple.push_back(data);
            }
            tuples.push_back(std::move(tuple));
        }

        auto txn = db.CreateTxn();
        auto exec_ctx = db.GetExecutionContext(txn);
        auto value_op = std::make_shared<ValueOperator>(exec_ctx, schema, std::move(tuples));
        auto insert_op = std::make_shared<InsertOperator>(exec_ctx, value_op, table_name);

        Chunk chunk_ignore;
        insert_op->Check();
        insert_op->Init();
        insert_op->Next(chunk_ignore);
        db.Commit(*txn);
    } else {
        abort();
    }
}

static std::shared_ptr<Operator> BuildTree(std::ifstream &data_file, ExecutionContext &exec_ctx) {
    OPType op_type;
    Read(data_file, op_type);
    if (op_type == OPType::hash_join) {
        idx_t probe_id, build_id;
        Read(data_file, probe_id);
        Read(data_file, build_id);
        auto probe_child = BuildTree(data_file, exec_ctx);
        auto build_child = BuildTree(data_file, exec_ctx);
        auto probe_col = probe_child->GetOutputSchema()[probe_id];
        auto build_col = build_child->GetOutputSchema()[build_id];
        return std::make_shared<HashJoinOperator>(exec_ctx, probe_child, build_child, probe_col, build_col);
    } else if (op_type == OPType::scan) {
        auto table_name = std::to_string(data_file.tellg());
        idx_t row, col;
        Read(data_file, row);
        Read(data_file, col);
        data_file.seekg(row * col * sizeof(data_t), std::ios::cur);
        return std::make_shared<SeqScanOperator>(exec_ctx, table_name);
    } else {
        abort();
    }
}

static int64_t Run(std::string data_path, std::string test_name) {
    std::ifstream data_file(data_path, std::ios::binary | std::ios::in);
    BabyDB db_instance;
    BuildTable(data_file, db_instance);
    idx_t expect_size;
    Read(data_file, expect_size);

    auto txn = db_instance.CreateTxn();
    auto exec_ctx = db_instance.GetExecutionContext(txn);
    data_file.seekg(0, std::ios::beg);
    auto op_tree = BuildTree(data_file, exec_ctx);

    auto start = std::chrono::high_resolution_clock::now();
    db_instance.OptimizeJoinPlan(op_tree);
    Chunk data_chunk;
    OperatorState state = OperatorState::HAVE_MORE_OUTPUT;
    idx_t total_size = 0;
    op_tree->Check();
    op_tree->Init();
    while (state == OperatorState::HAVE_MORE_OUTPUT) {
        state = op_tree->Next(data_chunk);
        total_size += data_chunk.size();
    }
    auto end = std::chrono::high_resolution_clock::now();
    db_instance.Commit(*txn);

    auto cost = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << test_name << ": " << (expect_size == total_size ? "correct" : "wrong") << " time: " << cost.count() / 1000000 << "ms\n";

    return cost.count();
}

static std::vector<std::string> all_tests = {
    "10a", "11a", "12a", "13a", "14a", "15a", "16a", "17a", "18a", "19a"
};

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Call " << argv[0] << " path-to-data\n";
        exit(1);
    }
    std::string data_path = argv[1];
    std::vector<std::string> run_tests;
    if (argc == 2) {
        run_tests = all_tests;
    } else {
        for (idx_t i = 2; i < argc; i++) {
            run_tests.push_back(argv[i]);
        }
    }
    int64_t total_time = 0;
    for (auto test : run_tests) {
        total_time += Run(data_path + "/" + test + ".data", test);
    }
    std::cout << "Total time: " << total_time / 1000000 << "ms\n";
}