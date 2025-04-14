## BabyDB

This is the codebase for Database System course, IIIS, Tsinghua University in 2025.

You should start from the branch `main`, and when each project posted, this branch will be updated. So before you start your next project, confirm your code is up to date.

**Important Notice: You should not make your code public anywhere.**

### Cloning this Repository

1. Go [here](https://github.com/new) to create a new empty (without README.md) repository under your account. Pick a name (e.g. `babydb-private`) and select **Private** for the repository visibility level.
2. Create a bare clone of the repository:

```
$ git clone --bare https://github.com/hehezhou/babydb.git
```

3. Mirror-push to the new repository:

```
$ cd babydb.git
$ git push https://github.com/USERNAME/babydb-private.git main
```

And remove the filefolder:

```
$ cd ..
$ rm babydb.git -rf
```

4. Clone your private repository to your machine:

```
$ git clone https://github.com/USERNAME/babydb-private.git
$ cd babydb-private
```

5. Add the codebase BabyDB repository as a second remote, which allows you to retrieve changes from the codebase repository and merge them into your repository:

```
$ git remote add codebase https://github.com/hehezhou/babydb.git
```

You can verify that the remote was added with the following command:

```
$ git remote -v
codebase        https://github.com/hehezhou/babydb.git (fetch)
codebase        https://github.com/hehezhou/babydb.git (push)
origin ...
```

You can now pull in changes from the codebase repository as needed with:

```
$ git pull codebase main
```

We suggest you working on your projects in separate branches. If you do not understand how Git branches work, [learn how](https://git-scm.com/book/en/v2/Git-Branching-Basic-Branching-and-Merging).

### Build

**We recommend you run this code on Linux.** If you're using Windows, MacOS or other systems, and fails to build BabyDB, you should first try to install a Linux virtual machine, or WSL2 on Windows. If you still have problems, find TAs' help.

Before start building, make sure you have `make`, `cmake` and C++ compiler (`g++` or `clang++`) on your machine and they are added into your environment variable, which means you can directly use them on your shell.

Then run the following commands to build the system:

```
$ mkdir build
$ cd build
$ cmake ..
$ make -j`nproc`
```

If you want to compile the system in release mode, pass in the following flag to cmake:

```
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make -j`nproc`
```

Otherwise by default the system will be compiled in debug mode. We recommend you to develop and debug on debug mode, but test performance on release mode.

By default, debug mode will use address sanitizer. If you want to use other sanitizers like undefined sanitizer, pass in the following flag:

```
$ cmake -DBABYDB_SANITIZER=undefined ..
$ make -j`nproc`
```

Then you can run the tests:

```
$ make check-tests
```

or get more details:

```
$ make check-tests-details
```

You may fail on some tests. If their name are all start with `Project`, don't worry, they will be passed if you correctly complete your project. But if you fail on other tests, please first check your steps, and then contact with the TA.

### Project 1

You should only modify `src/storage/art.cpp` and `src/include/storage/art.hpp`, and pass all `Project1ArtIndexMVCC` tests if you complete the whole project.

**Notice:** The basic version does not store row id in the index, but in the final version, `LookupKey` and `ScanRange` should returns row id instead of key itself. You can choose a reasonable point to implement it. (Suggestion: When implementing the version link.)

At the start of `src/storage/art.cpp`, there are some tips.

To submit, run the following code to generate the submission file (make sure `zip` is in your environment variable):

```
make submit-p1
```

then manually add your report into the zip file, and submit to Web Learning.

Here's some information about tests:

For tests start with `SortedKeys`, the key is equal to the row id, so you can pass it without storing row id.

For each test, you'll get the whole score if you pass the test within reasonable time.

1. SortedKeys_RangeQuery: Require task 1.
2. SortedKeys_RangeQuery_MultipleRanges: Require task 1. Note that this test have a large number of range queries with a small range.
3. RandomKeys_OnlyPointQuery: No addition requirement. (Only require returning correct row id.)
4. RandomKeys_RangeQuery: Require task 1.
5. SparseKeys_OnlyPointQuery: No addition requirement.
6. SparseKeys_RangeQuery: Require task 1.
7. DenseKeys_WithUpdates_PointQuery: Require task 2.
8. MixedReadWrite_HighQueryRatio: Require task 2.
9. LongVersionChain_SequentialTs: Require task 2. Note that this test have a large number of version link update.
10. LongVersionChain_RandomTs: Require task 3. Note that this test have a large number of version link update.
11. LongVersionChain_SequentialTs_RangeQuery: Require task 1 & 2. Note that this test have a large number of version link updates and range queries.
12. LongVersionChain_RandomTs_RangeQuery: Require ALL task. Note that this test have a large number of version link updates and range queries.

### Project 2

You should at least modify `src/storage/art.cpp`, `src/concurrency/version_link.cpp`, `src/concurrency/transaction_manager.cpp`, `src/execution/execution_common.cpp` and the header file corresponding to them. You should move your version links to `src/concurrency/version_link.cpp` so that other parts of the database can use it.

For a transaction, we will first call `BabyDB::CreateTxn` to create a transaction, then execute with this transaction, finally call `BabyDB::Commit(txn)` or `BabyDB::Abort(txn)` to commit or rollback the transaction.

There is a configurable domain in `ConfigGroup` that indicates the isolation level this database instance using. **Do not check serializability on Snapshot Isolation Level.**

Once a conflict is found, call `Transaction::SetTainted` and throw a `TaintedException` error.

In summary, for a transaction, there are $4$ cases:

1. The transaction is tainted. **The database should not directly rollback the transaction when finding conflict. The application will abort the transaction.**
2. The transaction works well but aborted by the application.
3. The transaction works well but finds conflicts when trying to commit. In this case, you should call `Abort(txn)` in the Commit function.
4. The transaction works well and finds no conflict when trying to commit. Only in this case, the transaction will commit successfully.

**Version Link Trigger**: When creating a version node, call `RegisterVersionNode`. When deleting a version node, call `UnregisterVersionNode`.

Here is the information about testcases:

1. DirtyRead (1 pts): Require snapshot isolation.
1. NonRepeatableRead (1 pts): Require snapshot isolation.
1. TaintedTest (1 pts): Require snapshot isolation and write-write conflict detection.
1. AbortTest (1 pts): Require snapshot isolation, write-write conflict detection, and rollback.
1. SerializableTest (1 pts): Require serializable check and rollback.
1. BankSystemTest (4 pts): A workload on Snapshot Isolation. Multi-thread.
1. SellSystemTest (4 pts): A workload on Serializable. Multi-thread.
1. GCTest (3 pts): Check if your GC implementation is correct. We will manually check if you use trigger correctly, if not, you will not get the points even if you pass this test.

You can see `test/project2/project2_test.cpp` to get more details about the workload.

**Important Notice:** If your code has any one of the following problems, you will get 0 points for the whole project 2:

1. Memory leak or other memory management problems. We check this by `asan` which is enabled by default in the `Debug` mode.
1. Any Lock (except for the `shared_mutex` on the whole database we given, but you cannot modify any sentence about it). Since MVCC and OCC are lock-free concurrency control methods. Latches are acceptable, the standard for distinguishing between locks and latches is, when getting a mutex, if your code will release the mutex after a finite time in the same `Operator`.
1. Any behavior to identify workload. Although we will grade your implementation on these 8 testcases, your implementation should work well on any possible workload.
