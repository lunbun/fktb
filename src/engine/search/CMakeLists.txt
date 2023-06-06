add_subdirectory(move_ordering)

target_sources(chess PRIVATE
        fixed_search.cc
        fixed_search.h
        iterative_search.cc
        iterative_search.h
        score.h
        statistics.h)
