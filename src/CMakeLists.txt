add_subdirectory(engine)

target_sources(chess PRIVATE
        uci.cc
        uci.h)
