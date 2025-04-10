add_library(
    babydb_concurrency
    OBJECT
    transaction.cpp
    transaction_manager.cpp
    version_link.cpp)

set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:babydb_concurrency>
    PARENT_SCOPE)
