/**
 * The MIT License
 *
 * Copyright 2022 Krishna sssky307@163.com
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#pragma once

#include <cassert>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace csa {

namespace config {
namespace keyword {
//
// These are reserved keywords.
//
auto constexpr start = "Start";          ///< Reserved token for the start production.
auto constexpr epsilon = "epsilon";      ///< The epsilon token.
auto constexpr eof = "$";                ///< The "End of file" token.
auto constexpr pointer = "->";           ///< Pointer of a production rule.
auto constexpr alien = "<- alien ->";    ///< An impossible token doesn't belong to any grammar.
}    // namespace keyword
}    // namespace config

class Symbol;
using SymbolPtr = Symbol *;
using SymbolSet = std::set<SymbolPtr>;
using SymbolList = std::vector<SymbolPtr>;

/**
 * @brief Symbol definition.
 *
 * A symbol(aka token) is terminal or nonterminal.
 */
class Symbol {
public:
    enum class Type : int {
        unknown = 0,         ///< Token type is unknown.
        nonterminal,         ///< The order matters: (int)nonterminal < (int)any_terminal
        terminal,            ///< It is a terminal.
        terminalIsEof,       ///< It is a terminal, and it is "End of file" token.
        terminalIsEpsilon    ///< It is a terminal, and it is "empty string" token.
    };

    /**
     * @brief Construct a new Symbol object.
     *
     * The symbol's type is decided by three phase.
     * 1, When constructed, it is initialized with unknown type.
     * 2, At lexer, it may be given a terminal type.
     * 3, At parser, if the symbol is production's left hand symbol, it is nonterminal,
     *    and all other symbols(type is unkown) will be terminal.
     *
     * @param[in] name  Input symbol name.
     */
    Symbol(std::string name) : name_(name), type_(Type::unknown), isNillable_(false) {}
    std::string name() { return name_; }
    void setNillable(bool value) { isNillable_ = value; }
    bool isNillable() { return isNillable_; }
    bool isTerminal() { return static_cast<int>(type_) > static_cast<int>(Type::nonterminal); }
    bool isTerminalEof() { return type_ == Type::terminalIsEof; }
    bool isTerminalEpsilon() { return type_ == Type::terminalIsEpsilon; }
    bool isNonterminal() { return !isTerminal(); }
    bool isStartSymbol(){ return this->name() == config::keyword::start; }
    bool isAlienSymbol(){ return this->name() == config::keyword::alien; }
    void setType(Type type) { type_ = type; }
    Type getType() { return type_; }
    SymbolSet &firstSet() { return std::ref(firstSet_); }
    SymbolSet &followSet() { return std::ref(followSet_); }

private:
    std::string name_;       ///< Symbol name.
    Type type_;              ///< Symbol type.
    bool isNillable_;        ///< Only used by nonterminal.
    SymbolSet firstSet_;     ///< First set of this symbol.
    SymbolSet followSet_;    ///< Follow set of this symbol.
};

/**
 * @brief A symbol table manage all symbol's life time.
 *
 * All the symbols are singleton, so no duplicated symbol exist.
 */
class SymbolTable;
using SymbolTablePtr = std::shared_ptr<SymbolTable>;

class SymbolTable {
public:
    SymbolTable() {
        alien_ = new Symbol(config::keyword::alien);
        alien_->setType(Symbol::Type::terminal);
        alien_->firstSet().insert(alien_);
    }
    ~SymbolTable() {
        for (auto &item : table_) { delete item.second; }
        if(alien_) { delete alien_; }
    }

    /**
     * @brief Get the Alien Symbol which doesn't belong to any grammar.
     *
     * @return SymbolPtr    The Alien Symbol.
     */
    SymbolPtr getAlienSymbol() {
        assert(alien_);
        return alien_;
    }

    /**
     * @brief Find symbol by name.
     *
     * If the symbol is not found, a new symbol will be constructed and returned,
     * so it never return nullptr.
     *
     * @param[in] name      Input symbol name.
     * @return SymbolPtr    A symbol instance.
     */
    SymbolPtr findSymbol(std::string name) {
        assert(!name.empty());

        auto &symbol = table_[name];
        if (!symbol) {
            symbol = new Symbol(name);
            assert(symbol);
        }

        return symbol;
    }

    const std::map<std::string, SymbolPtr> &table() const { return std::ref(table_); }

    void dump() {
        std::size_t max = 0;
        for (auto &item : table_) {
            auto size = item.first.size();
            if (max < size) { max = size; }
        }
        printf("[dump-symboltable-begin]\n");
        for (auto &item : table_) {
            printf("  name = %-*s", max, item.first.c_str());
            printf("  type = ");
            switch (item.second->getType()) {
                case Symbol::Type::unknown:
                    printf("unknown");
                    break;
                case Symbol::Type::nonterminal:
                    printf("nonterminal");
                    break;
                case Symbol::Type::terminal:
                    printf("terminal");
                    break;
                case Symbol::Type::terminalIsEof:
                    printf("terminalIsEof");
                    break;
                case Symbol::Type::terminalIsEpsilon:
                    printf("terminalIsEpsilon");
                    break;
                default:
                    break;
            }
            printf("\n");
        }
        printf("[dump-symboltable-end]\n\n");
    }

private:
    std::map<std::string, SymbolPtr> table_;
    SymbolPtr alien_;
};

struct Production {
    struct LeftHandSide {
        Symbol *symbol = nullptr;    ///< A symbol reference from symbol table.
    };

    struct RightHandSide {
        SymbolList symbolList;              ///< All the symbols are reference from symbol table.
        mutable SymbolSet firstSet;         ///< First set of right-hand-side.
        mutable SymbolSet predictSet;       ///< Predict set of right-hand-side.
        mutable bool isNillable = false;    ///< Is right-hand-side nillable.
    };

    int id = -1;        ///< Production id.
    LeftHandSide lhs;   ///< Production left hand symbol.
    RightHandSide rhs;  ///< Production right hand symbol(s).

    bool empty() { return this->lhs.symbol == nullptr; }
    void clear() {
        this->lhs.symbol = nullptr;
        this->rhs.symbolList.clear();
    }
    bool operator==(const Production &other) const {
        return this->lhs.symbol == other.lhs.symbol && this->rhs.symbolList == other.rhs.symbolList;
    }
    bool operator!=(const Production &other) const { return !(*this == other); }
    bool operator<(const Production &other) const {
        return std::tie(this->lhs.symbol, this->rhs.symbolList)
               < std::tie(other.lhs.symbol, other.rhs.symbolList);
    }
};

using ProductionList = std::vector<Production>;
class ProductionTable;
using ProductionTablePtr = std::shared_ptr<ProductionTable>;

/**
 * @brief A production table manage all production's life time.
 */
class ProductionTable {
public:
    ProductionTable(ProductionList &pl) : pl_(std::move(pl)) {}
    const ProductionList &table() const { return std::ref(pl_); }
    bool empty() { return pl_.empty(); }
    int size() { return pl_.size(); }
    std::size_t getMaxWidthOfNt() {
        static std::size_t max = 0;
        if (max == 0) {
            for (auto &p : pl_) {
                auto size = p.lhs.symbol->name().size();
                if (max < size) { max = size; }
            }
        }
        return max;
    }
    void dump() {
        std::cout << "[dump-production-table-begin]\n";
        for (auto &p : pl_) { 
            printf("  [%02d] ", p.id);
            std::cout << toString(p) << std::endl; 
        }
        std::cout << "[dump-production-table-end]\n\n";
    }

    std::string toString(const Production &p, bool alignPointer = true) {
        std::string str;
        str += p.lhs.symbol->name();
        if (alignPointer) {
            auto max = getMaxWidthOfNt();
            str += std::string(max - p.lhs.symbol->name().size(), ' ');
        }
        str += " ->";
        for (auto &symbol : p.rhs.symbolList) {
            str += " ";
            str += symbol->name();
        }
        return str;
    }

private:
    ProductionList pl_;
};

/**
 * @brief A grammar is <N, T, P, S> where 
 * N is nonterminal, 
 * T is terminal, 
 * P is production set, 
 * S is start symbol.
 * 
 * So the GrammarContext are all those stuff.
 */
struct GrammarContext {
    GrammarContext(ProductionTablePtr pl, SymbolTablePtr st) : pl(pl), st(st) {}
    ProductionTablePtr pl;      ///< All productions, the first item is the start production.
    SymbolTablePtr st;          ///< All terminals and nonterminals.
};
using GrammarContextPtr = std::shared_ptr<GrammarContext>;

/**
 * @brief Some helper functions for building html.
 */
namespace html {
inline std::string quota(std::string text) { return "\"" + text + "\""; }
inline std::string line(std::string text) { return text + "\n"; }
inline std::string formatCell(std::string text) {
    std::string str;
    for (auto &c : text) {
        if (c == ' ') {
            str += "&nbsp;";
        } else if (c == '<') {
            str += "&lt;";
        } else if (c == '>') {
            str += "&gt;";
        } else if (c == '&') {
            str += "&amp;";
        } else if (c == '"') {
            str += "&quot;";
        } else if (c == '\'') {
            str += "&apos;";
        } else {
            str += c;
        }
    }
    return str;
}
};    // namespace html

/**
 * @brief A state family is a collection of states.
 */
class LRxStateFamily;
using LRxStateFamilyPtr = std::shared_ptr<LRxStateFamily>;

class LRxStateFamily {
public:
    /**
     * @brief A state is a collection of items.
     */
    struct State;
    using StatePtr = State *;
    struct State {
        /**
         * @brief An item is just a production with a dot.
         */
        struct Item {
            using ProductionPtr = const Production *;
            using Dot = std::size_t;

            ProductionPtr p;    ///< A production reference.
            Dot dot;            ///< Dot at the production's right hand side.

            Item(ProductionPtr p, Dot dot = 0) : p(p), dot(dot) {}
            Item(const Item &other) {
                this->p = other.p;
                this->dot = other.dot;
            }

            bool operator == (Item other)const{
                return std::tie(p, dot) == std::tie(other.p, other.dot);
            }
            
            bool operator != (const Item& other)const{
                return std::tie(p, dot) != std::tie(other.p, other.dot);
            }
            bool operator<(const Item &other) const {
                return std::tie(p, dot) < std::tie(other.p, other.dot);
            }
            void dump(std::size_t indent = 2)const{
                std::cout << std::string(indent, ' ');
                std::cout << this->p->lhs.symbol->name();
                std::cout << " ->";

                auto& symbolList = this->p->rhs.symbolList;
                for(auto i = 0; i < this->dot; ++i){
                    std::cout << " " << symbolList[i]->name();
                }
                std::cout << " .";
                for(auto i = this->dot; i < symbolList.size(); ++i){
                    std::cout << " " << symbolList[i]->name();
                }
                std::cout << "\n";
            }
        };

        using ItemPtr = Item *;
        using ItemSet = std::set<ItemPtr>;

        ItemSet items;              ///< All the items.
        mutable int id = -1;        ///< This state id.

        State() {}
        State(const State &other) {
            this->items = other.items;
            this->id = other.id;
        }
        State &operator=(const State &other) {
            if (this != &other) {
                this->items = other.items;
                this->id = other.id;
            }
            return *this;
        }
        bool insertItem(ItemPtr item) { return (items.insert(item).second); }
        bool empty() const { return items.empty(); }
        bool operator==(const State &other) const { 
            return (this->items == other.items); 
        }
        bool operator!=(const State &other) const { 
            return this->items != other.items; 
        }
        bool operator<(const State &other) const { 
            return (this->items < other.items); 
        }
        void dump(int indent = 2)const{
            for(auto& item : this->items){
                item->dump(indent);
            }
        }
    };

    using Item = State::Item;
    using ItemPtr = State::ItemPtr;
    using StateTable = std::map<State, StatePtr>;
    using ItemTable = std::map<Item, ItemPtr>;

    LRxStateFamily() {}
    ~LRxStateFamily() {
        for (auto &pair : itemTable_) { delete pair.second; }
        for (auto &pair : stateTable_) { delete pair.second; }
    }

    StatePtr findState(const State &other) { return stateTable_[other]; }
    StatePtr createNewState(const State &other) {

        auto &state = stateTable_[other];
        if (state == nullptr) {
            state = new State(other);
            state->id = nextStateId_++;
        }
        return state;
    }
    ItemPtr createNewItem(const Item &other) {
        auto &item = itemTable_[other];
        if (item == nullptr) { 
            item = new Item(other); 
        }

        return item;
    }
    const StateTable &stateTable() const { return std::ref(stateTable_); }
    const ItemTable &itemTable() const { return std::ref(itemTable_); }
    bool empty(){
        return stateTable_.empty()  || itemTable_.empty();
    }

    LRxStateFamilyPtr clone(){
        LRxStateFamilyPtr sf = std::make_shared<LRxStateFamily>();
        sf->stateTable_ = this->stateTable_;
        sf->itemTable_ = this->itemTable_;
        return sf;
    }
    void setStateTable(const StateTable& stateTable){
        stateTable_ = stateTable;
    }
    void setItemTable(const ItemTable& itemTable){
        itemTable_ = itemTable;
    }

    void dumpStateTable(std::size_t indent = 2)const{
        std::cout << "[dump-state-table-begin]\n";
        for(auto& pair : this->stateTable()){
            std::cout << std::string(indent, ' ') << "[" << pair.second->id << "]\n";
            pair.second->dump(indent + 2);
        }
        std::cout << "[dump-state-table-end]\n\n";
    }
    void dumpItemTable(std::size_t indent = 2)const{
        std::cout << "[dump-item-table-begin]\n";
        int i = 0;
        for(auto&item: itemTable_){
            std::cout << std::string(indent, ' ');
            printf("[%02d]", i++);
            item.second->dump();
        }
        std::cout << "[dump-item-table-end]\n\n";
    }

private:
    int nextStateId_ = 0;
    StateTable stateTable_;
    ItemTable itemTable_;
};

/**
 * @brief A uniform LR(0), LR(1), LALR table.
 * 
 * It is a data objct.
 * It is a data object as an interface.
 * It is to be produced or consumed by other objects.
 */
struct LRxTable {
    using StatePtr = LRxStateFamily::StatePtr;

    struct Cell {
        StatePtr state;     ///< Y coordinate in the table.
        SymbolPtr symbol;   ///< X coordinate in the table.

        Cell(StatePtr state = nullptr, SymbolPtr symbol = nullptr)
            : state(state), symbol(symbol) {}
        bool operator<(const Cell &other) const {
            if (this->state < other.state) {
                return true;
            } else {
                if (this->symbol < other.symbol) { return true; }
                return false;
            }
        }
        bool operator==(const Cell &other) const {
            if (this->state == other.state && this->symbol == other.symbol) { return true; }
            return false;
        }
        bool operator!=(const Cell &other) const { return !(*this == other); }
    };

    /**
     * @brief Action of a LR table's cell.
     */
    struct Action {
        enum class Type : int { Error = 0, Goto, Reduce, Accept };

        Type type = Type::Error;        ///< Action type.
        StatePtr gotoState = nullptr;   ///< Goto target state, valid if type == Type::Goto.
        int reducePid = -1;             ///< Reduce production id, valid if type == Type::Reduce.

        bool operator<(const Action &other) const {
            if (this->type < other.type) {
                return true;
            } else {
                if (this->gotoState < other.gotoState) {
                    return true;
                } else {
                    if (this->reducePid < other.reducePid) { return true; }
                    return false;
                }
            }
        }
        bool operator==(const Action &other) const {
            return this->type == other.type && this->gotoState == other.gotoState &&
                    this->reducePid == other.reducePid;
        }
        bool operator!=(const Action &other) const { return !(*this == other); }
        std::string toString() const {
            if (this->type == Type::Accept) {
                return "Accept";
            } else if (this->type == Type::Goto) {
                assert(this->gotoState != nullptr);
                return "S" + std::to_string(this->gotoState->id); // Goto state id.
            } else if (this->type == Type::Reduce) {
                assert(this->reducePid >= 0);
                return "R" + std::to_string(this->reducePid);   // Reduce production id.
            }
            return {};
        }
    };
    
    using ActionSet = std::set<Action>;

    void clear(){
        idMappingState.clear();
        idMappingSymbol.clear();
        cellMappingAction.clear();
    }

    // state-id mapping state.
    std::map<int, StatePtr> idMappingState;
    // symbol-id mapping symbol.
    // The front part is terminal.
    // The back part is nonterminal.
    std::map<int, SymbolPtr> idMappingSymbol;
    // All the table cell's infomation.
    std::map<Cell, ActionSet> cellMappingAction;
};

}    // namespace csa