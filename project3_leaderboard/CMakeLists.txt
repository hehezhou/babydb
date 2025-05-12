add_executable(p3-leaderboard EXCLUDE_FROM_ALL run_job.cpp)
target_include_directories(
        p3-leaderboard PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(p3-leaderboard babydb)