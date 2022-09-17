/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "BaseType.h"
#include "Parser.h"

#define YYSTYPE csa::Parser::semantic_type

#ifndef YY_NO_UNISTD_H
#define YY_NO_UNISTD_H
#endif
#include "Lexer.h"

using namespace csa;

int main() {
    std::string stream = R"(
S -> ( S ) S
S -> "epsilon"
)";
    YY_BUFFER_STATE yyBufState;
    csa::Parser::semantic_type lvalp;

    yyscan_t scanner;
    csa::SymbolTablePtr st{new csa::SymbolTable};

    yylex_init_extra(st, &scanner);

    yyBufState = yy_scan_string(stream.c_str(), scanner);

    // while (yylex(&lvalp, scanner) > 0){
    // 	printf("  word = %s\n", yyget_text(scanner));
    // }

    ProductionList pl;
    csa::Parser parser(scanner, pl, *st);
    auto result = parser.parse();

    yy_delete_buffer(yyBufState, scanner);
    yylex_destroy(scanner);

    st->dump();

    ProductionTable pt(pl);
    pt.dump();
    std::cout <<"total size = " << pt.size() << "\n";

    return result;
}
