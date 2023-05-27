add_subdirectory(engine)

target_sources(chess PRIVATE
        test.cc
        test.h
        uci.cc
        uci.h)
