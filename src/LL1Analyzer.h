/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#pragma once

#include "BaseType.h"
#include <cassert>

namespace csa {
class LL1Analyzer {
public:
    LL1Analyzer(GrammarContextPtr gc) : gc_(gc), isParsed_(false){}
    int parse();
    bool isValidLL1();
    std::string buildHtmlTable(bool hasProductionTable = true, bool hasLL1Table = true);

private:
    void initEPS();
    bool isEPS(const SymbolList& symbolList);
    void buildFirstSet();
    SymbolSet calculateFirstSet(const SymbolList& symbolList);
    void buildFollowSet();
    void buildPredictSet();


    bool setUnion(SymbolSet& set1, SymbolSet& set2);
    bool setRemove(SymbolSet& set, SymbolPtr symbol);

    GrammarContextPtr gc_;
    bool isParsed_;

    class HtmlBuilder{
    public:
        HtmlBuilder(GrammarContextPtr gc, bool buildProductionTable = true, bool buildLL1Table = true)
        :gc_(gc), buildProductionTable_(buildProductionTable), buildLL1Table_(buildLL1Table){}
        std::string buildHtmlTable();
    private:
        GrammarContextPtr gc_;
        bool buildProductionTable_;
        bool buildLL1Table_;

        std::string buildHtmlHead();
        std::string buildHtmlBody();
        std::string buildHtmlTableOfProductionTable();
        std::string buildHtmlTableOfLL1Table();
    };
};

}    // namespace csa