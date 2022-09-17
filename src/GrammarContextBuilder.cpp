/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "BaseType.h"
#include "GrammarContextBuilder.h"

#include "Parser.h"

#define YYSTYPE csa::Parser::semantic_type
#ifndef YY_NO_UNISTD_H
#define YY_NO_UNISTD_H
#endif
#include "Lexer.h"

#include <fstream>

using namespace csa;

GrammarContextPtr GrammarContextBuilder::buildFromBuffer(std::vector<char> &buf){
    if (buf.empty()) { return {}; }
    if (buf.back() != '\n') { buf.push_back('\n'); }
    // The last two bytes must be YY_END_OF_BUFFER_CHAR (ASCII NUL) for flex.
    buf.push_back(0);
    buf.push_back(0);

    yyscan_t scanner;
    YY_BUFFER_STATE yyBufState;

    ProductionList pl;
    auto st = std::make_shared<csa::SymbolTable>();

    yylex_init_extra(st, &scanner);
    yyBufState = yy_scan_buffer(buf.data(), buf.size(), scanner);
    csa::Parser parser(scanner, pl, *st);
    if(parser.parse() != 0){ pl.clear(); }

    yy_delete_buffer(yyBufState, scanner);
    yylex_destroy(scanner);

    if(!pl.empty()){
        auto pt = std::make_shared<ProductionTable>(pl);
        return std::make_shared<GrammarContext>(pt, st);
    }

    return {};
}

GrammarContextPtr GrammarContextBuilder::buildFromStream(const std::string &stream) {
    if (stream.empty()) return {};

    std::vector<char> buffer(stream.begin(), stream.end());

    return buildFromBuffer(buffer);
}

GrammarContextPtr GrammarContextBuilder::buildFromFile(const std::string &filename) {
    if (filename.empty()) return {};

    std::ifstream ifs(filename);
    std::vector<char> buffer;
    if (ifs) {
        std::istreambuf_iterator<char> begin(ifs);
        std::istreambuf_iterator<char> end;
        buffer.assign(begin, end);
        return buildFromBuffer(buffer);
    } else {
        printf("error, cannot read file = %s\n", filename.c_str());
    }

    return {};
}