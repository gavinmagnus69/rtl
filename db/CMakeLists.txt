option(WITH_DB_TESTS OFF)


add_subdirectory(core)
add_subdirectory(pool)
add_subdirectory(tx)
# add_subdirectory(mapping)
# add_subdirectory(obs)
# add_subdirectory(orm)
# add_subdirectory(query)

if(WITH_DB_TESTS) 
    add_subdirectory(test)
endif()