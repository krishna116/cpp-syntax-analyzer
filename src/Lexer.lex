%{

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

static void RegisterSymbol(std::string name, 
                           csa::SymbolTablePtr st, 
                           csa::SymbolPtr& symbol, 
                           csa::Parser::token::token_kind_type& type){
  symbol = st->findSymbol(name); 

  if(name == csa::config::keyword::start){
    symbol->setType(csa::Symbol::Type::nonterminal);
    printf("error, token [%s] is a reserved keyword cannot be used by user.\n", name.c_str());
    type = csa::Parser::token::token_kind_type::YYUNDEF;
  }else{
    if(name == csa::config::keyword::epsilon){
      symbol->setType(csa::Symbol::Type::terminalIsEpsilon);
    }else if(name == csa::config::keyword::eof){
      symbol->setType(csa::Symbol::Type::terminalIsEof);
    }
    type = csa::Parser::token::token_kind_type::SYMBOL;
  }
}

%}

%option noyywrap
%option nounistd
%option reentrant
%option bison-bridge
%option never-interactive
%option extra-type="csa::SymbolTablePtr"
%option header-file="Lexer.h"
%option outfile="Lexer.cpp"

SYMBOL      [^|\"\n\r\v\f\t ]+
STRING      ["](\\.|[^\\"\n])+["]
END         [\r]?[\n]
SPACES      [\t ]+
COMMENT     "//".*[\n]

%%

{SYMBOL}    { 
              if(std::string(yytext) == csa::config::keyword::pointer){
                return csa::Parser::token::token_kind_type::POINTER;
              }else{
                csa::SymbolPtr symbol;
                csa::Parser::token::token_kind_type type;
                RegisterSymbol(yytext, yyextra, symbol, type);
                (*yylval).emplace<csa::SymbolPtr>(symbol);
                return type;
              }
            }

{STRING}    { 
              std::string str;
              auto it = yytext + 1;             // Remove first ["]
              auto end = yytext + yyleng - 1;   // Remove last ["]

              while(it < end){
                  if(*it == '\\'){              // Convert [\] meaning
                    if((it+1) < end){
                      str += *(it+1);
                      it += 2;
                    }else{
                      printf("error, found invalid token: %s\n", yytext);
                      return csa::Parser::token::token_kind_type::YYUNDEF;
                    }
                  }else{
                    str += *it++;
                  }
              }

              csa::SymbolPtr symbol;
              csa::Parser::token::token_kind_type type;
              RegisterSymbol(str, yyextra, symbol, type);
              (*yylval).emplace<csa::SymbolPtr>(symbol);
              return type;
            }

{END}       { return csa::Parser::token::token_kind_type::END; }

{COMMENT}   {}
{SPACES}    {}

.           { printf("error, found invalid token: %s\n", yytext); 
              return csa::Parser::token::token_kind_type::YYUNDEF; }

%%