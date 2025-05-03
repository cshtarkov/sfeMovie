set(SFEMOVIE_DEMO "sfeMovieDemo")

set(SFEMOVIE_DEMO_SRC
    main.cpp
    MediaInfo.cpp
    MediaInfo.hpp
    StreamSelector.cpp
    StreamSelector.hpp
    UserInterface.cpp
    UserInterface.hpp
)
source_group("Sources" FILES ${SFEMOVIE_DEMO_SRC})

add_executable(
    ${SFEMOVIE_DEMO}
    ${SFEMOVIE_DEMO_SRC}
)

target_link_libraries(
  ${SFEMOVIE_DEMO}
  ${SFEMOVIE_LIB}
)

if (MACOSX)
    set_target_properties(${SFEMOVIE_DEMO} PROPERTIES 
                          BUILD_WITH_INSTALL_RPATH 1 
                          INSTALL_RPATH "@executable_path/")
endif()
