set(TestFiles
"test01_lexer"
"test02_parser"
"test03_GrammarContextBuilder"
"test04_LL1Analyzer"
# "test05_LR0Analyzer"
)

foreach(TestFile ${TestFiles})
    add_executable(${TestFile} ${TestFile}.cpp)
    target_link_libraries(${TestFile} PRIVATE SyntaxAnalyzerLib)
endforeach()
