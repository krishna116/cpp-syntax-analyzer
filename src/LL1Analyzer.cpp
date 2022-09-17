/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "LL1Analyzer.h"

using namespace csa;
using namespace csa::html;

using SymbolMappingId = std::map<SymbolPtr, std::size_t>;
using IdMappingSymbol = std::map<std::size_t, SymbolPtr>;
using CellId = std::pair<std::size_t, std::size_t>;
using ProductionIdSet = std::set<std::size_t>;
using CellIdMappingProductionIdSet = std::map<CellId, ProductionIdSet>;

int LL1Analyzer::parse() {
    if(gc_ == nullptr){ return 1; }
    if(isParsed_){ return 0; }

    auto removeAllEpsilon = [&](){
        for (auto &p : gc_->pl->table()) {
            auto epsilon = gc_->st->findSymbol(config::keyword::epsilon);
            setRemove(p.lhs.symbol->firstSet(), epsilon);
            setRemove(p.lhs.symbol->followSet(), epsilon);
            setRemove(p.rhs.firstSet, epsilon);
            setRemove(p.rhs.predictSet, epsilon);
        }
    };

    initEPS();
    buildFirstSet();
    buildFollowSet();
    buildPredictSet();
    //isValidLL1();
    removeAllEpsilon();
    isParsed_ = true;

    return 0;
}

std::string LL1Analyzer::buildHtmlTable(bool hasProductionTable, bool hasLL1Table){
    if(!isParsed_){ return{}; }
    HtmlBuilder builder(gc_, hasProductionTable, hasLL1Table);
    return builder.buildHtmlTable();
}

void LL1Analyzer::initEPS() {
    SymbolSet eps;
    bool hasChange;
    do {
        hasChange = false;
        for (auto &p : gc_->pl->table()) {
            if (isEPS(p.rhs.symbolList)) {
                p.lhs.symbol->setNillable(true);
                p.rhs.isNillable = true;
                if (eps.insert(p.lhs.symbol).second) { hasChange = true; }
            }
        }
    } while (hasChange);
}

bool LL1Analyzer::isEPS(const SymbolList &symbolList) {
    std::size_t symbolCount = 0;
    std::size_t nillableCount = 0;

    for (auto &symbol : symbolList) {
        if (symbol->isNillable() || symbol->isTerminalEpsilon()) { ++nillableCount; }
        ++symbolCount;
    }

    return (symbolCount == nillableCount);
}

void LL1Analyzer::buildFirstSet() {
    for (auto &item : gc_->st->table()) {
        auto &symbol = item.second;
        symbol->firstSet().clear();
        if (symbol->isTerminal()) { symbol->firstSet().insert(symbol); }
    }

    bool hasChange;
    do {
        hasChange = false;
        for (auto &p : gc_->pl->table()) {
            auto tempSet = calculateFirstSet(p.rhs.symbolList);
            if (setUnion(p.lhs.symbol->firstSet(), tempSet)) { hasChange = true; }
        }
    } while (hasChange);
}

SymbolSet LL1Analyzer::calculateFirstSet(const SymbolList &symbolList) {
    SymbolSet set;

    for (auto &symbol : symbolList) {
        setUnion(set, symbol->firstSet());
        if (!symbol->isNillable()) { break; }
    }

    return set;
}

bool LL1Analyzer::setUnion(SymbolSet &set1, SymbolSet &set2) {
    bool hasChange = false;

    for (auto &symbol : set2) {
        if(set1.insert(symbol).second){
            hasChange = true;
        }
    }

    return hasChange;
}

bool LL1Analyzer::setRemove(SymbolSet &set, SymbolPtr symbol) {
    auto it = set.find(symbol);

    if (it != set.end()) {
        set.erase(it);
        return true;
    }

    return false;
}

void LL1Analyzer::buildFollowSet() {
    bool hasChange;
    do {
        hasChange = false;
        for (auto &p : gc_->pl->table()) {
            for (auto beg = p.rhs.symbolList.begin(); beg < (p.rhs.symbolList.end() - 1); ++beg) {
                if ((*beg)->isNonterminal()) {
                    SymbolList symbolList(beg + 1, p.rhs.symbolList.end());
                    auto tempSet = calculateFirstSet(symbolList);
                    if (setUnion((*beg)->followSet(), tempSet)) { hasChange = true; }
                }
            }
            for (auto rbeg = p.rhs.symbolList.rbegin(); rbeg < p.rhs.symbolList.rend(); ++rbeg) {
                if ((*rbeg)->isNonterminal()) {
                    if (setUnion((*rbeg)->followSet(), p.lhs.symbol->followSet())) {
                        hasChange = true;
                    }
                }
                if (!(*rbeg)->isNillable()) { break; }
            }
        }
    } while (hasChange);
}

void LL1Analyzer::buildPredictSet() {
    for (auto &p : gc_->pl->table()) {
        p.rhs.firstSet = calculateFirstSet(p.rhs.symbolList);
        p.rhs.predictSet = p.rhs.firstSet;
        if (p.rhs.isNillable) { setUnion(p.rhs.predictSet, p.lhs.symbol->followSet()); }
    }
}

bool LL1Analyzer::isValidLL1() {
    if(!isParsed_){
        return false;
    }

    bool good = true;
    std::map<SymbolPtr, SymbolSet> symbolAndSet;

    auto hasIntersection = [](const SymbolSet &set1, const SymbolSet &set2) {
        for (auto &symbol : set2) {
            if (set1.find(symbol) != set1.end()) { return true; }
        }
        return false;
    };

    for (auto &p : gc_->pl->table()) {
        if (hasIntersection(symbolAndSet[p.lhs.symbol], p.rhs.predictSet)) {
            printf("[LL1Analyzer::isValidLL1]\n");
            printf("  [note] production(id=%d) has conflict predict set, ", p.id);
            printf("it's invalid LL1 grammar.\n");
            good = false;
            break;
        }
        setUnion(symbolAndSet[p.lhs.symbol], p.rhs.predictSet);
    }

    return good;
}

std::string LL1Analyzer::HtmlBuilder::buildHtmlTable() {

    if(buildProductionTable_ || buildLL1Table_){
        std::string str = line("<!DOCTYPE html>");
        str += line("<html>");
        str += buildHtmlHead();
        str += buildHtmlBody();
        str += line("</html>");
        return str;
    }

    return {};
}

std::string LL1Analyzer::HtmlBuilder::buildHtmlHead() {
    return "<head></head>";
}

std::string LL1Analyzer::HtmlBuilder::buildHtmlBody() {
    std::string str = line("<body>");

    if(buildProductionTable_){
        str += buildHtmlTableOfProductionTable();
    }

    if(buildLL1Table_){
        str += buildHtmlTableOfLL1Table();
    }

    str += line("</body>");
    return str;
}

std::string LL1Analyzer::HtmlBuilder::buildHtmlTableOfProductionTable() {
    auto addTableTitle = [&](std::string title = "Production Table"){
        std::string str;
        str += "<h2>";
        str += title;
        str += "</h2>\n";
        return str;
    };

    auto addTableStyle = [&](){
    return R"(
    <style type="text/css">
        .tg {
            border-collapse: collapse;
            border-color: #bbb;
            border-spacing: 0;
        }

        .tg td {
            background-color: #E0FFEB;
            border-color: #bbb;
            border-style: solid;
            border-width: 1px;
            color: #202020;
            font-family: Monospace, sans-serif, Arial;
            font-size: 14px;
            overflow: hidden;
            padding: 3px 8px;
            word-break: normal;
        }

        .tg th {
            background-color: #9DE0AD;
            border-color: #bbb;
            border-style: solid;
            border-width: 1px;
            color: #202020;
            font-family: Monospace, sans-serif, Arial;
            font-size: 14px;
            font-weight: normal;
            overflow: hidden;
            padding: 3px 8px;
            word-break: normal;
        }

        .tg .tg-18eh {
            border-color: #202020;
            color: #202020;
            font-weight: bold;
            text-align: center;
            vertical-align: middle
        }

        .tg .tg-hos7 {
            border-color: #202020;
            color: #202020;
            font-family: Monospace, sans-serif, Arial !important;
            font-size: 14px;
            text-align: center;
            vertical-align: top
        }

        .tg .tg-1tol {
            border-color: #202020;
            color: #202020;
            font-weight: bold;
            text-align: left;
            vertical-align: middle
        }

        .tg .tg-mqa1 {
            border-color: #202020;
            color: #202020;
            font-weight: bold;
            text-align: center;
            vertical-align: top
        }

        .tg .tg-mcqj {
            border-color: #202020;
            color: #202020;
            font-weight: bold;
            text-align: left;
            vertical-align: top
        }

        .tg .tg-8m2j {
            border-color: #202020;
            color: #202020;
            font-family: Monospace, sans-serif, Arial !important;
            font-size: 14px;
            text-align: left;
            vertical-align: top
        }
    </style>
)";
    };

    auto addProductionTableHead = [&](){
        return R"(
        <thead>
            <tr>
                <th class="tg-1tol">Id</th>
                <th class="tg-mqa1">Production(A -&gt; XYZ)</th>
                <th class="tg-mcqj">FirstSet(XYZ)</th>
                <th class="tg-mcqj">FollowSet(A)</th>
                <th class="tg-18eh">PredictSet(XYZ)</th>
                <th class="tg-mcqj">IsNillable(XYZ)</th>
            </tr>
        </thead>
        )";
    };

    auto addRecord = [&](std::string id, 
                         std::string production, 
                         std::string firstSet, 
                         std::string followSet,
                         std::string predictSet, 
                         std::string isNillable) {
        std::string str;
        str += line("<tr>");
        str +=  "<td class=\"tg-hos7\">" + formatCell(id) + "</td>\n";
        str +=  "<td class=\"tg-8m2j\">" + formatCell(production) + "</td>\n";
        str +=  "<td class=\"tg-8m2j\">" + formatCell(firstSet) + "</td>\n";
        str +=  "<td class=\"tg-8m2j\">" + formatCell(followSet) + "</td>\n";
        str +=  "<td class=\"tg-8m2j\">" + formatCell(predictSet) + "</td>\n";
        str +=  "<td class=\"tg-hos7\">" + formatCell(isNillable) + "</td>\n";
        str += line("</tr>");
        return str;
    };

    auto idToStr = [](const std::size_t id) { return std::to_string(id); };

    auto symbolSetToStr = [](const SymbolSet &set) {
        std::string stream;
        for (auto &symbol : set) {
            if (!stream.empty()) { stream += " "; }
            stream += symbol->name();
        }
        return stream;
    };

    auto addProductionTableBody = [&](){
        std::string str;

        str += line("<tbody>");

        // Create table records.
        std::size_t id = 1;
        for (auto &p : gc_->pl->table()) {
            str += addRecord(idToStr(id++), 
                             gc_->pl->toString(p), 
                             symbolSetToStr(p.rhs.firstSet),
                             symbolSetToStr(p.lhs.symbol->followSet()),
                             symbolSetToStr(p.rhs.predictSet), 
                             p.rhs.isNillable ? "yes" : "no");
        }

        str += line("</tbody>");

        return str;
    };

    auto addProductionTable = [&](){
        std::string str;
        str += addTableTitle();
        str += addTableStyle();
        str += line("<table class=\"tg\">");
        str += addProductionTableHead();
        str += addProductionTableBody();
        str += line("</table>");
        return str;
    };

    return addProductionTable();
}

std::string LL1Analyzer::HtmlBuilder::buildHtmlTableOfLL1Table() {
    // ------------------------------------------------------------
    // Extrat table info functions.
    // ------------------------------------------------------------
    
    // terminal bind id;
    auto buildTerminlMappingId = [&](const ProductionList &pl) {
        SymbolMappingId terminalMappingId;
        std::size_t id = 1;

        SymbolPtr theLastSymbol = nullptr;
        SymbolPtr theEofSymbol = nullptr;

        for (auto &p : pl) {
            for (auto &t : p.rhs.symbolList) {
                if(t->isNonterminal() || t->isTerminalEpsilon()) continue;
                auto &thisId = terminalMappingId[t];
                if (thisId == 0) {
                    thisId = id++;
                    theLastSymbol = t;
                }
                if (t->isTerminalEof()) { theEofSymbol = t; }
            }
        }

        // Make terminal(eof) bind with the last id,
        // so that it always appears at the last colunm of the html table.
        if (theLastSymbol != nullptr && theEofSymbol != nullptr && theLastSymbol != theEofSymbol) {
            auto &theLastSymbolId = terminalMappingId[theLastSymbol];
            auto &theEofSymbolId = terminalMappingId[theEofSymbol];
            std::swap(theLastSymbolId, theEofSymbolId);
        }

        return terminalMappingId;
    };

    // nonterminal bind id;
    auto buildNonterminalMappingId = [&](const ProductionList &pl) {
        SymbolMappingId nonterminalMappingId;
        std::size_t id = 1;
        for (auto &p : pl) {
            auto &thisId = nonterminalMappingId[p.lhs.symbol];
            if (thisId == 0) { thisId = id++; }
        }
        return nonterminalMappingId;
    };

    // cell bind production-id-set.
    auto buildCellIdMappingProductionIdSet = [&](const ProductionList &pl,
                                                 SymbolMappingId &terminalMappingId,
                                                 SymbolMappingId &nonterminalMappingId) {
        CellIdMappingProductionIdSet cmp;
        std::size_t productionId = 1;

        for (auto &p : pl) {
            auto &nt = p.lhs.symbol;
            for (auto &t : p.rhs.predictSet) {
                CellId cellId;
                cellId.first = nonterminalMappingId[nt];
                cellId.second = terminalMappingId[t];
                cmp[cellId].insert(productionId);
            }
            ++productionId;
        }

        return cmp;
    };

    // id bind symbol (and sorted by id).
    auto toIdMappingSymbol = [](const SymbolMappingId &symbolMappingId) {
        IdMappingSymbol ims;
        for (auto &item : symbolMappingId) { ims[item.second] = item.first; }
        return ims;
    };

    // ------------------------------------------------------------
    // Common data.
    // ------------------------------------------------------------
    auto terminalMappingId = buildTerminlMappingId(gc_->pl->table());
    auto nonterminalMappingId = buildNonterminalMappingId(gc_->pl->table());
    auto cellIdMappingProductionIdSet =
        buildCellIdMappingProductionIdSet(gc_->pl->table(), terminalMappingId, nonterminalMappingId);
    auto idMappingTerminal = toIdMappingSymbol(terminalMappingId);
    auto idMappingNonterminal = toIdMappingSymbol(nonterminalMappingId);

    // ------------------------------------------------------------
    // Html functions.
    // ------------------------------------------------------------

    auto addTableTitle = [&](std::string title = "LL(1) Table"){
        std::string str;
        str += "<h2>";
        str += title;
        str += "</h2>\n";
        return str;
    };

    auto addTableStyle = [&](){
        return R"(
    <style type="text/css">
        .tg {
            border-collapse: collapse;
            border-color: #bbb;
            border-spacing: 0;
        }

        .tg td {
            background-color: #E0FFEB;
            border-color: #bbb;
            border-style: solid;
            border-width: 1px;
            color: #594F4F;
            font-family: Monospace, sans-serif, Arial;
            font-size: 14px;
            overflow: hidden;
            padding: 3px 8px;
            word-break: normal;
        }

        .tg th {
            background-color: #9DE0AD;
            border-color: #bbb;
            border-style: solid;
            border-width: 1px;
            color: #493F3F;
            font-family: Monospace, sans-serif, Arial;
            font-size: 14px;
            font-weight: normal;
            overflow: hidden;
            padding: 3px 8px;
            word-break: normal;
        }

        .tg .tg-18eh {
            border-color: #202020;
            font-weight: bold;
            text-align: center;
            vertical-align: middle
        }

        .tg .tg-hos7 {
            border-color: #202020;
            color: #202020;
            font-family: Monospace, sans-serif, Arial !important;
            font-size: 14px;
            text-align: center;
            vertical-align: top
        }

        .tg .tg-1tol {
            border-color: #202020;
            font-weight: bold;
            text-align: left;
            vertical-align: middle
        }

        .tg .tg-mqa1 {
            border-color: #202020;
            font-weight: bold;
            text-align: center;
            vertical-align: top
        }

        .tg .tg-mcqj {
            border-color: #202020;
            font-weight: bold;
            text-align: left;
            vertical-align: top
        }

        .tg .tg-8m2j {
            border-color: #202020;
            color: #202020;
            font-family: Monospace, sans-serif, Arial !important;
            font-size: 14px;
            text-align: left;
            vertical-align: top
        }
    </style>
        )";
    };

    auto getTerminalCount = [&](){
        return std::to_string(idMappingTerminal.size());
    };

    auto getTerminalList = [&](){
        std::vector<std::string> terminals;

        for(auto& pair: idMappingTerminal){
            terminals.push_back(pair.second->name());
        }
        
        return terminals;
    };

    auto addTableHead = [&](){
        std::string str;
        auto terminalList = getTerminalList();

        str += line("<thead>");

        str += line("<tr>");
        str += line("<th class=\"tg-1tol\" rowspan=\"2\">Nonterminal</th>");
        str += "<th class=\"tg-mqa1\" colspan=\"" + getTerminalCount() + "\">Terminal</th>\n";
        str += line("</tr>");

        str += line("<tr>");
        for(auto const& terminal: terminalList){
            str += "<th class=\"tg-mqa1\">";
            str += formatCell(terminal);
            str += "</th>\n";
        }
        str += line("</tr>");

        str += line("</thead>");

        return str;
    };

    auto productionIdSetToStr = [](const ProductionIdSet& ids){
        std::string str;

        for(auto& id : ids){
            if(!str.empty()){
                str += " ";
            }
            str += std::to_string(id);
        }

        return str;
    };

    auto addCell = [&](std::string text){
        std::string str;
        str += "<td class=\"tg-8m2j\">" + formatCell(text) + "</td>\n";
        return str;
    };

    auto addTableRecords = [&](){
        std::string str;

        for (auto &ntItem : idMappingNonterminal) {
            str += line("<tr>");
            str += "<td class=\"tg-hos7\">" + formatCell(ntItem.second->name()) +"</td>";
            for (auto &tItem : idMappingTerminal) {
                CellId cellId;
                cellId.first = ntItem.first;
                cellId.second = tItem.first;
                auto cell = productionIdSetToStr(cellIdMappingProductionIdSet[cellId]);
                str += addCell(cell);
            }
            str += line("</tr>");
        }

        return str;
    };

    auto addTableBody = [&](){
        std::string str;

        str += line("<tbody>");
        str += addTableRecords();
        str += line("</tbody>");

        return str;
    };

    auto addTable = [&](){
        std::string str;
        str += addTableTitle();
        str += addTableStyle();
        str += line("<table class=\"tg\">");
        str += addTableHead();
        str += addTableBody();
        str += line("</table>");
        return str;
    };

    return addTable();
}
