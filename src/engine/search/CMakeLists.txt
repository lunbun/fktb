add_subdirectory(move_ordering)

target_sources(fktb PRIVATE
        fixed_search.cc
        fixed_search.h
        iterative_search.cc
        iterative_search.h
        score.h
        statistics.h)
