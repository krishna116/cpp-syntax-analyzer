configure_file(config.h.txt config.h)

add_executable(${PROJECT_NAME}
    CppSyntaxAnalyzer.cpp
    miniopt.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE SyntaxAnalyzerLib)