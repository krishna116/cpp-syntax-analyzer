/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "GrammarContextBuilder.h"
#include <iostream>

using namespace csa;

int main(){
    std::string stream = R"(
S -> ( S ) S
S -> "epsilon"
)";

    auto gc = GrammarContextBuilder::buildFromStream(stream);

    if(gc){
        gc->pl->dump();
        gc->st->dump();
        printf("test pass\n");
        return 0;
    }else{
        printf("test fail.\n");
        return 1;
    }
}