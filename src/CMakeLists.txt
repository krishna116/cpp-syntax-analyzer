
find_program(FLEX flex REQUIRED)
find_program(BISON bison REQUIRED)

# Generate lexer files.
set(LEXER_DOT_H "${CMAKE_CURRENT_BINARY_DIR}/Lexer.h")
set(LEXER_DOT_CPP "${CMAKE_CURRENT_BINARY_DIR}/Lexer.cpp")
set(LEXER_DOT_LEX "${CMAKE_CURRENT_SOURCE_DIR}/Lexer.lex")
add_custom_command(
    OUTPUT ${LEXER_DOT_H} ${LEXER_DOT_CPP}
    DEPENDS ${LEXER_DOT_LEX}
    COMMAND FLEX ${LEXER_DOT_LEX}
)

# Generate parser files.
set(PARSER_DOT_H "${CMAKE_CURRENT_BINARY_DIR}/Parser.h")
set(PARSER_DOT_CPP "${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp")
set(PARSER_DOT_YACC "${CMAKE_CURRENT_SOURCE_DIR}/Parser.yacc")
add_custom_command(
    OUTPUT ${PARSER_DOT_H} ${PARSER_DOT_CPP}
    DEPENDS ${PARSER_DOT_YACC}
    COMMAND BISON ${PARSER_DOT_YACC}
)

# Add library.
add_library(SyntaxAnalyzerLib STATIC 
    ${LEXER_DOT_CPP}
    ${PARSER_DOT_CPP}
    GrammarContextBuilder.cpp
    LL1Analyzer.cpp
)
target_include_directories(SyntaxAnalyzerLib 
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)