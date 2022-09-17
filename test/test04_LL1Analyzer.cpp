/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "GrammarContextBuilder.h"
#include "LL1Analyzer.h"
#include <iostream>

using namespace csa;

int main(){
    std::string stream = R"(
S -> ( S ) S
S -> "epsilon"
)";

    auto gc = GrammarContextBuilder::buildFromStream(stream);
    if(gc){
        LL1Analyzer theLL1Analyzer(gc);
        
        if(theLL1Analyzer.parse() == 0){
            std::cout << theLL1Analyzer.buildHtmlTable() << std::endl;
            return 0;
        }
    }

    printf("--test LL1Analyzer failed--\n");
    return 1;
}