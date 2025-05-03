set(SFEMOVIE_MINIMALIST_DEMO "sfeMovieMinimalistDemo")

set(SFEMOVIE_MINIMALIST_DEMO_SRC
    main.cpp
)
source_group("Sources" FILES ${SFEMOVIE_MINIMALIST_DEMO_SRC})

add_executable(
    ${SFEMOVIE_MINIMALIST_DEMO}
    ${SFEMOVIE_MINIMALIST_DEMO_SRC}
)

target_link_libraries(
  ${SFEMOVIE_MINIMALIST_DEMO}
  ${SFEMOVIE_LIB}
  ${SFML_LIBRARIES}
  ${SFML_DEPENDENCIES}
)

if (MACOSX)
    set_target_properties(${SFEMOVIE_MINIMALIST_DEMO} PROPERTIES 
                          BUILD_WITH_INSTALL_RPATH 1 
                          INSTALL_RPATH "@executable_path/")
endif()
