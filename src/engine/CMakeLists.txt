add_subdirectory(board)
add_subdirectory(eval)
add_subdirectory(hash)
add_subdirectory(move)
add_subdirectory(search)

target_sources(chess PRIVATE
        init.h
        inline.h
        intrinsics.h
        mutex.h)
