%code requires{

/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "BaseType.h"
#include <iostream>
}

// Require bison version.
%require "3.2"
%language "c++"
%define api.namespace {csa}
%define api.parser.class {Parser}
%header "Parser.h"
%output "Parser.cpp"

%param {void* scanner}
%parse-param {csa::ProductionList& pl} {csa::SymbolTable& st}

%define api.value.type variant

%token <csa::SymbolPtr> SYMBOL
%token END
%right POINTER

%type <csa::ProductionList> ProductionList
%type <csa::Production> Production
%type <csa::SymbolList> SymbolList

%code{
int yylex(csa::Parser::semantic_type* yylval_param , void* yyscanner);


static void printProduction(const csa::Production& p, std::string prefix = {}){
    if(!prefix.empty()){
        std::cout << prefix << " ";
    }
    std::cout << p.lhs.symbol->name();
    std::cout << " ->";
    for (auto &symbol : p.rhs.symbolList) { 
        std::cout <<" " << symbol->name().c_str();
    }
    std::cout << std::endl;
}

static int checkProductionUnique(const csa::ProductionList& pl){
    if(pl.empty()){
        printf("[error] the production list is empty.\n");
        return 1;
    }

    std::set<csa::Production> set;
    for(auto& p : pl){
        if(!set.insert(p).second){
            printf("[error] the production list has duplicate item.");
            printProduction(p, "[error]");
            return 1;
        }
    }

    return 0;
}

static int checkProductionLeftHandSideName(const csa::ProductionList& pl) {
    for(auto& p : pl){
        auto name = p.lhs.symbol->name();
        if (name == csa::config::keyword::eof ||
            name == csa::config::keyword::epsilon) {
            printf("[error] left hand side of production cannot use [%s]\n", name.c_str());
            printProduction(p, "[error]");
            return 1;
        }
    }
    return 0;
}

static int checkProductionRightHandSideNames(const csa::ProductionList &pl) {
    for (auto &p : pl) {
        std::size_t epsilonCount = 0;
        std::size_t eofCount = 0;
        auto &symbolList = p.rhs.symbolList;
        bool good = true;

        for (auto &symbol : symbolList) {
            if (symbol->isTerminalEpsilon()) { ++epsilonCount; }
            if (symbol->isTerminalEof()) { ++eofCount; }
        }

        if (epsilonCount == 1 && symbolList.size() != 1) {
            std::cout << "error: cannot use \"" << csa::config::keyword::epsilon
                        << "\" with other tokens at a production's right hand side.\n";
            good = false;
        }

        if (epsilonCount > 1) {
            std::cout << "error: no more than one \"" << csa::config::keyword::epsilon
                        << "\" is allowed in a production.\n";
            good = false;
        }

        if (eofCount == 1 && !symbolList.back()->isTerminalEof()) {
            std::cout << "error: token \"" << csa::config::keyword::eof
                        << "\" cannot be used at the middle of a production's right hand side.\n";
            good = false;
        }

        if (eofCount > 1) {
            std::cout << "error: no more than one \"" << csa::config::keyword::eof
                        << "\" is allowed in a production.\n";
            good = false;
        }

        if(!good){
            printProduction(p, "[error]");
            return 1;
        }
    }

    return 0;
}

static int checkProductionList(const csa::ProductionList& pl){
    int errors = 0;

    errors += checkProductionUnique(pl);
    errors += checkProductionLeftHandSideName(pl);
    errors += checkProductionRightHandSideNames(pl);

    return errors;
}

static csa::ProductionList adjustProductionList(const csa::ProductionList& pl, 
                                                csa::SymbolTable& st)
{
    csa::ProductionList pl2;

    // Check and add start production.
    if(!pl.front().rhs.symbolList.back()->isTerminalEof()){
        csa::Production startProduction;

        startProduction.lhs.symbol = st.findSymbol(csa::config::keyword::start);
        startProduction.lhs.symbol->setType(csa::Symbol::Type::nonterminal);
        startProduction.rhs.symbolList.push_back(pl.front().lhs.symbol);
        auto eof = st.findSymbol(csa::config::keyword::eof);
        eof->setType(csa::Symbol::Type::terminalIsEof);
        startProduction.rhs.symbolList.push_back(eof);

        pl2.push_back(startProduction); // Add first production.
    }

    // Add rest productions.
    for(auto& p : pl){
        pl2.push_back(p);
    }

    int id = 0;
    for(auto& p : pl2){
        // Make sure left hand side symbol is nonterminal.
        p.lhs.symbol->setType(csa::Symbol::Type::nonterminal);
        // Assign id for this production.
        p.id = id++;
    }

    for(auto& p : pl2){
        // Make sure other symbols is terminal.
        for(auto symbol : p.rhs.symbolList){
            if(symbol->getType() == csa::Symbol::Type::unknown){
                symbol->setType(csa::Symbol::Type::terminal);
            }
        }
    }

    // Make sure epsilon is alwyas exist,
    // and make sure epsilon's type is terminalIsEpsilon.
    auto eps = st.findSymbol(csa::config::keyword::epsilon);
    eps->setType(csa::Symbol::Type::terminalIsEpsilon);

    return pl2;
}

static void dumpProductionList(const csa::ProductionList& pl) {
    printf("[dump-production-list-begin]\n");
    if (!pl.empty()) {
        for (auto &p : pl) {
            printProduction(p);
        }
    }else{
        printf("  [empty]\n");
    }
    printf("[dump-production-list-end]\n");
}
}

%%

Start           : ProductionList { 
                        if(checkProductionList(pl) == 0){
                            pl = adjustProductionList(pl, st);
                        }else{
                            pl.clear();
                        }
                    }
                ;

ProductionList  : ProductionList Production { 
                        if(!$2.empty()){ 
                            pl.push_back($2);
                            $2.clear();
                        }
                    }
                | Production { 
                        if(!$1.empty()){ 
                            pl.push_back($1);
                            $1.clear();
                        } 
                    }
                ;

Production      : SYMBOL POINTER SymbolList END {
                    $$.lhs.symbol = $1;
                    $$.rhs.symbolList = std::move($3);
                  }
                | END {
                    // Bypass empty line.
                }
                ;

SymbolList      : SymbolList SYMBOL { $1.push_back($2); $$ = std::move($1);}
                | SYMBOL { $$.push_back($1); }
                ;

%%

using namespace csa;
void Parser::error (const std::string& msg){
    printf("parser-error: %s\n", msg.c_str());
    dumpProductionList(pl);
}