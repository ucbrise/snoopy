add_library(log log.cpp)
add_library(log_enc log.cpp)
target_link_libraries(log_enc PRIVATE openenclave::oelibcxx)