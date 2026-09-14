add_executable(helion_server server/main.cpp)
add_executable(helion_client client/main.cpp)

target_compile_options(helion_server PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(helion_client PRIVATE -Wall -Wextra -Wpedantic)

if(UNIX)
  target_link_libraries(helion_server PRIVATE pthread)
endif()

find_package(OpenGL REQUIRED)
find_package(SDL2 2.0 REQUIRED CONFIG)
target_link_libraries(helion_client PRIVATE SDL2::SDL2 OpenGL::GL)
target_compile_definitions(helion_client PRIVATE HELION_HAS_SDL2=1)

install(TARGETS helion_server helion_client RUNTIME DESTINATION bin)
