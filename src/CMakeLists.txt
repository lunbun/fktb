add_subdirectory(engine)

target_sources(fktb PRIVATE
        test.cc
        test.h
        uci.cc
        uci.h)
