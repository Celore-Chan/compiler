#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <stack>
#include <functional>
#include <sstream>
#include <iomanip>
#include <cassert>

using namespace std;

// ------------------------------------------------------------
// 1. 词法分析器（基于实验一，返回Token流）
// ------------------------------------------------------------
struct Token {
    string type;   // 文法终结符
    string value;  // 字面量或标识符名
};

bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

vector<Token> tokenize(const string& input) {
    vector<Token> tokens;
    size_t pos = 0, len = input.size();
    while (pos < len) {
        char c = input[pos];
        if (isSpace(c)) { ++pos; continue; }
        if (c == '/') {
            if (pos + 1 < len) {
                char nxt = input[pos + 1];
                if (nxt == '/') { // 单行注释
                    pos += 2;
                    while (pos < len && input[pos] != '\n') ++pos;
                    continue;
                }
                if (nxt == '*') { // 多行注释
                    pos += 2;
                    while (pos < len) {
                        if (input[pos] == '*' && pos + 1 < len && input[pos + 1] == '/') {
                            pos += 2; break;
                        }
                        ++pos;
                    }
                    continue;
                }
            }
        }
        // 数字
        if (isDigit(c) || (c == '.' && pos + 1 < len && isDigit(input[pos + 1]))) {
            string num;
            while (pos < len && (isDigit(input[pos]) || input[pos] == '.')) num += input[pos++];
            int dotCnt = 0;
            for (char ch : num) if (ch == '.') ++dotCnt;
            if (dotCnt > 1 || (dotCnt == 1 && (num.front() == '.' || num.back() == '.'))) {
                // 词法错误，实验二输入保证无词法错误
                return {};
            }
            if (dotCnt == 1) {
                string intPart = num.substr(0, num.find('.'));
                if (intPart.length() > 1 && intPart[0] == '0') return {};
                tokens.push_back({"UFLOAT", num});
            } else {
                if (num.length() > 1 && num[0] == '0') return {};
                tokens.push_back({"UINT", num});
            }
            continue;
        }
        // 标识符与关键字
        if (isAlpha(c)) {
            string ident;
            while (pos < len && (isAlpha(input[pos]) || isDigit(input[pos]))) ident += input[pos++];
            if (ident == "int") tokens.push_back({"int", ident});
            else if (ident == "double") tokens.push_back({"double", ident});
            else if (ident == "scanf") tokens.push_back({"scanf", ident});
            else if (ident == "printf") tokens.push_back({"printf", ident});
            else if (ident == "if") tokens.push_back({"if", ident});
            else if (ident == "then") tokens.push_back({"then", ident});
            else if (ident == "while") tokens.push_back({"while", ident});
            else if (ident == "do") tokens.push_back({"do", ident});
            else tokens.push_back({"id", ident});
            continue;
        }
        // 运算符与分隔符
        if (c == '|' && pos + 1 < len && input[pos + 1] == '|') {
            tokens.push_back({"||", "||"}); pos += 2; continue;
        }
        if (c == '&' && pos + 1 < len && input[pos + 1] == '&') {
            tokens.push_back({"&&", "&&"}); pos += 2; continue;
        }
        if (c == '=' && pos + 1 < len && input[pos + 1] == '=') {
            tokens.push_back({"==", "=="}); pos += 2; continue;
        }
        if (c == '!' && pos + 1 < len && input[pos + 1] == '=') {
            tokens.push_back({"!=", "!="}); pos += 2; continue;
        }
        if (c == '>' && pos + 1 < len && input[pos + 1] == '=') {
            tokens.push_back({">=", ">="}); pos += 2; continue;
        }
        if (c == '<' && pos + 1 < len && input[pos + 1] == '=') {
            tokens.push_back({"<=", "<="}); pos += 2; continue;
        }
        switch (c) {
            case '+': tokens.push_back({"+", "+"}); ++pos; continue;
            case '-': tokens.push_back({"-", "-"}); ++pos; continue;
            case '*': tokens.push_back({"*", "*"}); ++pos; continue;
            case '/': tokens.push_back({"/", "/"}); ++pos; continue;
            case ',': tokens.push_back({",", ","}); ++pos; continue;
            case '(': tokens.push_back({"(", "("}); ++pos; continue;
            case ')': tokens.push_back({")", ")"}); ++pos; continue;
            case '{': tokens.push_back({"{", "{"}); ++pos; continue;
            case '}': tokens.push_back({"}", "}"}); ++pos; continue;
            case ';': tokens.push_back({";", ";"}); ++pos; continue;
            case '=': tokens.push_back({"=", "="}); ++pos; continue;
            case '!': tokens.push_back({"!", "!"}); ++pos; continue;
            case '>': tokens.push_back({">", ">"}); ++pos; continue;
            case '<': tokens.push_back({"<", "<"}); ++pos; continue;
            default: return {}; // 不可识别字符
        }
    }
    return tokens;
}

// ------------------------------------------------------------
// 2. 符号表、四元式、翻译器（全局上下文）
// ------------------------------------------------------------
struct Symbol {
    string name;
    string type;   // "int" / "double"
    string value;  // "null"
    int offset;
};

struct Quad {
    string op, arg1, arg2, result;
};

class Translator {
public:
    vector<Symbol> symTable;
    vector<Quad> quads;
    int nxq = 0;          // 下一条四元式序号
    int tempCount = 0;    // 临时变量计数
    int offset = 0;       // 当前偏移量
    bool errorFlag = false;

    string newtemp(const string& type) {
        string suffix = (type == "int") ? "i" : "d";
        return "T" + to_string(tempCount++) + "_" + suffix;
    }

    // 查找变量，返回符号表指针字符串 "TB0", "TB1"...
    string lookup(const string& name) {
        for (size_t i = 0; i < symTable.size(); ++i)
            if (symTable[i].name == name)
                return "TB" + to_string(i);
        errorFlag = true; // 未定义变量
        return "";
    }

    string lookup_type(const string& name) {
        for (const auto& s : symTable)
            if (s.name == name) return s.type;
        errorFlag = true;
        return "";
    }

    void enter(const string& name, const string& type, int off) {
        for (const auto& s : symTable)
            if (s.name == name) { errorFlag = true; return; } // 重复定义
        symTable.push_back({name, type, "null", off});
    }

    int gen(const string& op, const string& arg1, const string& arg2, const string& result) {
        quads.push_back({op, arg1, arg2, result});
        return nxq++;
    }

    void backpatch(const vector<int>& list, int target) {
        for (int i : list) {
            quads[i].result = to_string(target);
        }
    }

    vector<int> mklist(int addr) {
        return {addr};
    }

    vector<int> merge(const vector<int>& a, const vector<int>& b) {
        vector<int> res = a;
        res.insert(res.end(), b.begin(), b.end());
        return res;
    }

    bool hasError() const { return errorFlag; }

    void printResult() {
        // 符号表
        cout << symTable.size() << "\n";
        for (const auto& s : symTable) {
            int typeOut = (s.type == "int") ? 0 : 1;
            cout << s.name << " " << typeOut << " " << s.value << " " << s.offset << "\n";
        }
        // 临时变量个数
        cout << tempCount << "\n";
        // 四元式
        cout << quads.size() << "\n";
        for (size_t i = 0; i < quads.size(); ++i) {
            cout << i << ": (" << quads[i].op << "," << quads[i].arg1
                 << "," << quads[i].arg2 << "," << quads[i].result << ")\n";
        }
    }
};

// 全局指针，语义动作中使用
Translator* g_trans = nullptr;

// ------------------------------------------------------------
// 3. 属性结构
// ------------------------------------------------------------
struct Attr {
    string name;            // 用于ID
    string place;           // 变量/临时变量
    string type;            // "int" / "double"
    string op;              // 运算符
    string val;             // UINT/UFLOAT 字面量
    int width = 0;
    int quad = 0;           // N.quad
    vector<int> truelist;
    vector<int> falselist;
    vector<int> nextlist;
};

// ------------------------------------------------------------
// 4. LR(1) 分析表生成
// ------------------------------------------------------------
struct Production {
    string lhs;
    vector<string> rhs;
    function<Attr(const vector<Attr>&)> action;
};

// 所有符号的集合（终结符和非终结符）
set<string> terminals;
set<string> nonterminals;

// FIRST 集
map<string, set<string>> FIRST;

// 产生式列表
vector<Production> productions;

// 增广产生式编号
enum { ACC_PRODUCTION = 0 };

// 计算FIRST集（简单迭代）
void computeFirst() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& p : productions) {
            const string& A = p.lhs;
            const auto& rhs = p.rhs;
            if (rhs.empty()) {
                // 空产生式
                if (FIRST[A].insert("").second) changed = true;
                continue;
            }
            bool allNullable = true;
            for (const string& X : rhs) {
                if (terminals.count(X)) {
                    if (FIRST[A].insert(X).second) changed = true;
                    allNullable = false;
                    break;
                } else {
                    // 非终结符
                    for (const string& f : FIRST[X]) {
                        if (f != "" && FIRST[A].insert(f).second) changed = true;
                    }
                    if (FIRST[X].count("") == 0) {
                        allNullable = false;
                        break;
                    }
                }
            }
            if (allNullable) {
                if (FIRST[A].insert("").second) changed = true;
            }
        }
    }
}

// 求符号串的FIRST集
set<string> firstOfSequence(const vector<string>& seq) {
    set<string> res;
    bool allNullable = true;
    for (const string& X : seq) {
        if (terminals.count(X)) {
            res.insert(X);
            allNullable = false;
            break;
        } else {
            for (const string& f : FIRST[X]) {
                if (f != "") res.insert(f);
            }
            if (FIRST[X].count("") == 0) {
                allNullable = false;
                break;
            }
        }
    }
    if (allNullable) res.insert("");
    return res;
}

// LR(1) 项目
struct LR1Item {
    int prodId;     // 产生式编号
    int dot;        // 点的位置
    string lookahead;

    bool operator<(const LR1Item& o) const {
        if (prodId != o.prodId) return prodId < o.prodId;
        if (dot != o.dot) return dot < o.dot;
        return lookahead < o.lookahead;
    }
};

// LR(1) 项目集
using ItemSet = set<LR1Item>;

// 产生式右部指定位置的符号（若dot超出返回""）
string symbolAfterDot(const Production& p, int dot) {
    if (dot < (int)p.rhs.size()) return p.rhs[dot];
    return "";
}

// 闭包计算
ItemSet closure(ItemSet I) {
    bool changed = true;
    while (changed) {
        changed = false;
        ItemSet addItems;
        for (const auto& item : I) {
            const Production& p = productions[item.prodId];
            string B = symbolAfterDot(p, item.dot);
            if (B != "" && nonterminals.count(B)) {
                // 构造 β a
                vector<string> beta;
                for (int i = item.dot + 1; i < (int)p.rhs.size(); ++i)
                    beta.push_back(p.rhs[i]);
                beta.push_back(item.lookahead);
                set<string> firstBetaA = firstOfSequence(beta);
                for (int pi = 0; pi < (int)productions.size(); ++pi) {
                    if (productions[pi].lhs == B) {
                        for (const string& b : firstBetaA) {
                            if (b != "") { // 展望符不能是空
                                LR1Item newItem{pi, 0, b};
                                if (I.count(newItem) == 0 && addItems.count(newItem) == 0) {
                                    addItems.insert(newItem);
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        for (const auto& item : addItems) I.insert(item);
    }
    return I;
}

// GOTO 函数
ItemSet go(const ItemSet& I, const string& X) {
    ItemSet J;
    for (const auto& item : I) {
        const Production& p = productions[item.prodId];
        if (symbolAfterDot(p, item.dot) == X) {
            J.insert({item.prodId, item.dot + 1, item.lookahead});
        }
    }
    return closure(J);
}

// LR(1) 分析表
map<pair<int, string>, pair<char, int>> ACTION; // (state, term) -> (action, num)
map<pair<int, string>, int> GOTO;               // (state, nonterm) -> state
vector<ItemSet> canonicalCollection;
int startState = 0;

void buildLR1Table() {
    // 添加增广产生式 PROG' -> PROG
    productions.insert(productions.begin(), {"PROG'", {"PROG"}, [](const vector<Attr>&) -> Attr { return Attr(); }});
    // 确定终结符和非终结符
    for (const auto& p : productions) {
        nonterminals.insert(p.lhs);
        for (const string& s : p.rhs) {
            if (s != "" && s != "^") {
                // 如果不在非终结符中，就是终结符
                // 但有些符号既是非终结符也可能作为终结符？不会，文法设计区分大小写。
            }
        }
    }
    // 实际上从文法读取终结符，更稳妥的是手动给定。我们根据文法显式列出。
    // 由于已经定义好产生式，可以用排除法：所有出现在右部且不是lhs的即为终结符。
    set<string> allSymbols;
    for (const auto& p : productions) {
        for (const string& s : p.rhs) if (s != "") allSymbols.insert(s);
    }
    for (const string& s : allSymbols) {
        if (nonterminals.count(s) == 0) terminals.insert(s);
    }
    terminals.insert("$"); // 结束符

    // 计算FIRST
    computeFirst();

    // 构建规范项目集族
    ItemSet I0 = closure({{0, 0, "$"}});
    canonicalCollection.push_back(I0);
    map<ItemSet, int> stateId;
    stateId[I0] = 0;
    bool added = true;
    while (added) {
        added = false;
        size_t sz = canonicalCollection.size();
        for (size_t i = 0; i < sz; ++i) {
            const ItemSet& I = canonicalCollection[i];
            // 对于每个可能符号X
            for (const string& X : allSymbols) {
                ItemSet J = go(I, X);
                if (J.empty()) continue;
                if (stateId.count(J) == 0) {
                    stateId[J] = (int)canonicalCollection.size();
                    canonicalCollection.push_back(J);
                    added = true;
                }
                int target = stateId[J];
                if (terminals.count(X)) {
                    ACTION[{i, X}] = {'s', target};
                } else {
                    GOTO[{i, X}] = target;
                }
            }
        }
    }

    // 填充规约和接受动作
    for (size_t i = 0; i < canonicalCollection.size(); ++i) {
        const ItemSet& I = canonicalCollection[i];
        for (const auto& item : I) {
            if (item.prodId == ACC_PRODUCTION && item.dot == 1 && item.lookahead == "$") {
                ACTION[{i, "$"}] = {'a', 0}; // accept
            } else if (symbolAfterDot(productions[item.prodId], item.dot) == "") {
                // 规约
                if (ACTION.count({i, item.lookahead})) {
                    // 冲突，简单起见这里不处理，假设文法LR(1)
                }
                ACTION[{i, item.lookahead}] = {'r', item.prodId};
            }
        }
    }
}

// ------------------------------------------------------------
// 5. 文法产生式定义及语义动作
// ------------------------------------------------------------
void initProductions() {
    productions.clear();
    // 增广产生式在build前添加，这里定义原始产生式
    // 为了清晰，产生式顺序与题目一致，但需要与build中配合（增广在0号）
    // 我们在这里定义除增广外的所有产生式，build时将增广插入最前。
    // 使用函数对象，lambda捕获全局g_trans。

    // 总程序部分
    productions.push_back({"PROG", {"SUBPROG"}, [](const vector<Attr>& attr) -> Attr {
        return Attr();
    }});
    productions.push_back({"SUBPROG", {"M", "VARIABLES", "STATEMENT"}, [](const vector<Attr>& attr) -> Attr {
        // backpatch(STATEMENT.nextlist, nxq); gen(End,-,-,-)
        g_trans->backpatch(attr[2].nextlist, g_trans->nxq);
        g_trans->gen("End", "-", "-", "-");
        return Attr();
    }});
    productions.push_back({"M", {}, [](const vector<Attr>&) -> Attr {
        g_trans->offset = 0;
        return Attr();
    }});
    productions.push_back({"N", {}, [](const vector<Attr>&) -> Attr {
        Attr a;
        a.quad = g_trans->nxq;
        return a;
    }});

    // 变量声明部分
    productions.push_back({"VARIABLES", {"VARIABLES", "VARIABLE", ";"}, [](const vector<Attr>& attr) -> Attr {
        return Attr(); // VARIABLES 无需属性
    }});
    productions.push_back({"VARIABLES", {"VARIABLE", ";"}, [](const vector<Attr>& attr) -> Attr {
        return Attr();
    }});
    productions.push_back({"T", {"int"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.type = "int"; a.width = 4; return a;
    }});
    productions.push_back({"T", {"double"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.type = "double"; a.width = 8; return a;
    }});
    productions.push_back({"ID", {"id"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.name = attr[0].name; return a;
    }});
    productions.push_back({"VARIABLE", {"T", "ID"}, [](const vector<Attr>& attr) -> Attr {
        g_trans->enter(attr[1].name, attr[0].type, g_trans->offset);
        g_trans->offset += attr[0].width;
        Attr a; a.type = attr[0].type; a.width = attr[0].width; return a;
    }});
    productions.push_back({"VARIABLE", {"VARIABLE", ",", "ID"}, [](const vector<Attr>& attr) -> Attr {
        g_trans->enter(attr[2].name, attr[0].type, g_trans->offset);
        g_trans->offset += attr[0].width;
        Attr a; a.type = attr[0].type; a.width = attr[0].width; return a;
    }});

    // 语句部分
    productions.push_back({"ASSIGN", {"ID", "=", "EXPR"}, [](const vector<Attr>& attr) -> Attr {
        string p = g_trans->lookup(attr[0].name);
        g_trans->gen("=", attr[2].place, "-", p);
        Attr a; a.nextlist = g_trans->mklist(-1); // 但STATEMENT.nextlist = mklist() 空列表
        a.nextlist.clear(); // 空列表
        return a;
    }});
    productions.push_back({"STATEMENT", {"ASSIGN"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.nextlist = attr[0].nextlist; return a;
    }});
    productions.push_back({"SCANF", {"SCANF_BEGIN", ")"}, [](const vector<Attr>&) -> Attr {
        return Attr();
    }});
    productions.push_back({"SCANF_BEGIN", {"SCANF_BEGIN", ",", "ID"}, [](const vector<Attr>& attr) -> Attr {
        string p = g_trans->lookup(attr[2].name);
        g_trans->gen("R", "-", "-", p);
        return Attr();
    }});
    productions.push_back({"SCANF_BEGIN", {"scanf", "(", "ID"}, [](const vector<Attr>& attr) -> Attr {
        string p = g_trans->lookup(attr[2].name);
        g_trans->gen("R", "-", "-", p);
        return Attr();
    }});
    productions.push_back({"STATEMENT", {"SCANF"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.nextlist.clear(); return a;
    }});
    productions.push_back({"PRINTF", {"PRINTF_BEGIN", ")"}, [](const vector<Attr>&) -> Attr {
        return Attr();
    }});
    productions.push_back({"PRINTF_BEGIN", {"printf", "(", "ID"}, [](const vector<Attr>& attr) -> Attr {
        string p = g_trans->lookup(attr[2].name);
        g_trans->gen("W", "-", "-", p);
        return Attr();
    }});
    productions.push_back({"PRINTF_BEGIN", {"PRINTF_BEGIN", ",", "ID"}, [](const vector<Attr>& attr) -> Attr {
        string p = g_trans->lookup(attr[2].name);
        g_trans->gen("W", "-", "-", p);
        return Attr();
    }});
    productions.push_back({"STATEMENT", {"PRINTF"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.nextlist.clear(); return a;
    }});
    productions.push_back({"STATEMENT", {}, [](const vector<Attr>&) -> Attr {
        Attr a; a.nextlist.clear(); return a;
    }});
    productions.push_back({"STATEMENT", {"{", "L", ";", "}"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.nextlist = attr[1].nextlist; return a;
    }});
    productions.push_back({"STATEMENT", {"while", "N", "B", "do", "N", "STATEMENT"}, [](const vector<Attr>& attr) -> Attr {
        // N1=attr[1], B=attr[2], N2=attr[4], STATEMENT=attr[5]
        g_trans->backpatch(attr[5].nextlist, attr[1].quad);
        g_trans->backpatch(attr[2].truelist, attr[4].quad);
        Attr a; a.nextlist = attr[2].falselist;
        g_trans->gen("j", "-", "-", to_string(attr[1].quad));
        return a;
    }});
    productions.push_back({"STATEMENT", {"if", "B", "then", "N", "STATEMENT"}, [](const vector<Attr>& attr) -> Attr {
        g_trans->backpatch(attr[1].truelist, attr[3].quad);
        Attr a;
        a.nextlist = g_trans->merge(attr[1].falselist, attr[4].nextlist);
        return a;
    }});
    productions.push_back({"L", {"L", ";", "N", "STATEMENT"}, [](const vector<Attr>& attr) -> Attr {
        g_trans->backpatch(attr[0].nextlist, attr[2].quad);
        Attr a; a.nextlist = attr[3].nextlist; return a;
    }});
    productions.push_back({"L", {"STATEMENT"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.nextlist = attr[0].nextlist; return a;
    }});

    // 数值表达式部分
    productions.push_back({"EXPR", {"EXPR", "||", "ORITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp("int"); a.type = "int";
        g_trans->gen("||", attr[0].place, attr[2].place, a.place);
        return a;
    }});
    productions.push_back({"EXPR", {"ORITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = attr[0].place; a.type = attr[0].type; return a;
    }});
    productions.push_back({"ORITEM", {"ORITEM", "&&", "ANDITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp("int"); a.type = "int";
        g_trans->gen("&&", attr[0].place, attr[2].place, a.place);
        return a;
    }});
    productions.push_back({"ORITEM", {"ANDITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = attr[0].place; a.type = attr[0].type; return a;
    }});
    productions.push_back({"ANDITEM", {"NOITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = attr[0].place; a.type = attr[0].type; return a;
    }});
    productions.push_back({"ANDITEM", {"!", "NOITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp("int"); a.type = "int";
        g_trans->gen("!", attr[1].place, "-", a.place);
        return a;
    }});
    productions.push_back({"NOITEM", {"NOITEM", "REL", "RELITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp("int"); a.type = "int";
        g_trans->gen(attr[1].op, attr[0].place, attr[2].place, a.place);
        return a;
    }});
    productions.push_back({"NOITEM", {"RELITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = attr[0].place; a.type = attr[0].type; return a;
    }});
    productions.push_back({"RELITEM", {"RELITEM", "PLUS_MINUS", "ITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp(attr[0].type); a.type = attr[0].type;
        g_trans->gen(attr[1].op, attr[0].place, attr[2].place, a.place);
        return a;
    }});
    productions.push_back({"RELITEM", {"ITEM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = attr[0].place; a.type = attr[0].type; return a;
    }});
    productions.push_back({"ITEM", {"FACTOR"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = attr[0].place; a.type = attr[0].type; return a;
    }});
    productions.push_back({"ITEM", {"ITEM", "MUL_DIV", "FACTOR"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp(attr[2].type); a.type = attr[2].type;
        g_trans->gen(attr[1].op, attr[0].place, attr[2].place, a.place);
        return a;
    }});
    productions.push_back({"FACTOR", {"ID"}, [](const vector<Attr>& attr) -> Attr {
        Attr a;
        a.place = g_trans->lookup(attr[0].name);
        a.type = g_trans->lookup_type(attr[0].name);
        return a;
    }});
    productions.push_back({"FACTOR", {"UINT"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp("int"); a.type = "int";
        g_trans->gen("=", attr[0].val, "-", a.place);
        return a;
    }});
    productions.push_back({"FACTOR", {"UFLOAT"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp("double"); a.type = "double";
        g_trans->gen("=", attr[0].val, "-", a.place);
        return a;
    }});
    productions.push_back({"FACTOR", {"(", "EXPR", ")"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = attr[1].place; a.type = attr[1].type; return a;
    }});
    productions.push_back({"FACTOR", {"PLUS_MINUS", "FACTOR"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp(attr[1].type); a.type = attr[1].type;
        g_trans->gen(attr[0].op, "0", attr[1].place, a.place);
        return a;
    }});

    // 条件控制表达式 B
    productions.push_back({"B", {"B", "||", "N", "BORTERM"}, [](const vector<Attr>& attr) -> Attr {
        g_trans->backpatch(attr[0].falselist, attr[2].quad);
        Attr a;
        a.truelist = g_trans->merge(attr[0].truelist, attr[3].truelist);
        a.falselist = attr[3].falselist;
        return a;
    }});
    productions.push_back({"B", {"BORTERM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.truelist = attr[0].truelist; a.falselist = attr[0].falselist; return a;
    }});
    productions.push_back({"BORTERM", {"BORTERM", "&&", "N", "BANDTERM"}, [](const vector<Attr>& attr) -> Attr {
        g_trans->backpatch(attr[0].truelist, attr[2].quad);
        Attr a;
        a.truelist = attr[4].truelist;
        a.falselist = g_trans->merge(attr[0].falselist, attr[4].falselist);
        return a;
    }});
    productions.push_back({"BORTERM", {"BANDTERM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.truelist = attr[0].truelist; a.falselist = attr[0].falselist; return a;
    }});
    productions.push_back({"BANDTERM", {"(", "B", ")"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.truelist = attr[1].truelist; a.falselist = attr[1].falselist; return a;
    }});
    productions.push_back({"BANDTERM", {"!", "BANDTERM"}, [](const vector<Attr>& attr) -> Attr {
        Attr a;
        a.truelist = attr[1].falselist;
        a.falselist = attr[1].truelist;
        return a;
    }});
    productions.push_back({"BANDTERM", {"BFACTOR", "REL", "BFACTOR"}, [](const vector<Attr>& attr) -> Attr {
        int nxq = g_trans->nxq;
        g_trans->gen("j" + attr[1].op, attr[0].place, attr[2].place, "0");
        g_trans->gen("j", "-", "-", "0");
        Attr a;
        a.truelist = g_trans->mklist(nxq);
        a.falselist = g_trans->mklist(nxq + 1);
        return a;
    }});
    productions.push_back({"BANDTERM", {"BFACTOR"}, [](const vector<Attr>& attr) -> Attr {
        int nxq = g_trans->nxq;
        g_trans->gen("jnz", attr[0].place, "-", "0");
        g_trans->gen("j", "-", "-", "0");
        Attr a;
        a.truelist = g_trans->mklist(nxq);
        a.falselist = g_trans->mklist(nxq + 1);
        return a;
    }});
    productions.push_back({"BFACTOR", {"UINT"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp("int"); a.type = "int";
        g_trans->gen("=", attr[0].val, "-", a.place);
        return a;
    }});
    productions.push_back({"BFACTOR", {"UFLOAT"}, [](const vector<Attr>& attr) -> Attr {
        Attr a; a.place = g_trans->newtemp("double"); a.type = "double";
        g_trans->gen("=", attr[0].val, "-", a.place);
        return a;
    }});
    productions.push_back({"BFACTOR", {"ID"}, [](const vector<Attr>& attr) -> Attr {
        Attr a;
        a.place = g_trans->lookup(attr[0].name);
        a.type = g_trans->lookup_type(attr[0].name);
        return a;
    }});

    // 运算符
    productions.push_back({"PLUS_MINUS", {"+"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = "+"; return a;
    }});
    productions.push_back({"PLUS_MINUS", {"-"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = "-"; return a;
    }});
    productions.push_back({"MUL_DIV", {"*"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = "*"; return a;
    }});
    productions.push_back({"MUL_DIV", {"/"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = "/"; return a;
    }});
    productions.push_back({"REL", {"=="}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = "=="; return a;
    }});
    productions.push_back({"REL", {"!="}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = "!="; return a;
    }});
    productions.push_back({"REL", {"<"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = "<"; return a;
    }});
    productions.push_back({"REL", {"<="}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = "<="; return a;
    }});
    productions.push_back({"REL", {">"}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = ">"; return a;
    }});
    productions.push_back({"REL", {">="}, [](const vector<Attr>&) -> Attr {
        Attr a; a.op = ">="; return a;
    }});
}

// ------------------------------------------------------------
// 6. LR(1) 语法分析驱动
// ------------------------------------------------------------
bool parse(const vector<Token>& tokens) {
    stack<int> states;
    stack<Attr> attrs;
    states.push(startState);
    size_t idx = 0;
    while (true) {
        int s = states.top();
        string lookahead = (idx < tokens.size()) ? tokens[idx].type : "$";
        auto it = ACTION.find({s, lookahead});
        if (it == ACTION.end()) {
            return false; // 语法错误
        }
        char act = it->second.first;
        int num = it->second.second;
        if (act == 's') {
            // 移进
            states.push(num);
            // 根据token设置属性
            Attr a;
            if (lookahead == "id") a.name = tokens[idx].value;
            else if (lookahead == "UINT" || lookahead == "UFLOAT") a.val = tokens[idx].value;
            else if (lookahead == "+" || lookahead == "-") a.op = lookahead;
            else if (lookahead == "*" || lookahead == "/") a.op = lookahead;
            else if (lookahead == "==" || lookahead == "!=" || lookahead == "<" || lookahead == "<=" || lookahead == ">" || lookahead == ">=") a.op = lookahead;
            // 其他终结符属性不重要
            attrs.push(a);
            ++idx;
        } else if (act == 'r') {
            // 规约
            const Production& prod = productions[num];
            int len = (int)prod.rhs.size();
            vector<Attr> childAttrs(len);
            for (int i = len - 1; i >= 0; --i) {
                childAttrs[i] = attrs.top(); attrs.pop();
                states.pop();
            }
            if (g_trans->hasError()) return false; // 语义错误
            Attr newAttr = prod.action(childAttrs);
            if (g_trans->hasError()) return false;
            int topState = states.top();
            auto git = GOTO.find({topState, prod.lhs});
            if (git == GOTO.end()) return false;
            states.push(git->second);
            attrs.push(newAttr);
        } else if (act == 'a') {
            return true; // 接受
        } else {
            return false;
        }
    }
}

// ------------------------------------------------------------
// 主程序
// ------------------------------------------------------------
int main() {
    // 读取全部输入
    string input;
    char ch;
    while (cin.get(ch)) input += ch;
    // 去除'\r'
    input.erase(remove(input.begin(), input.end(), '\r'), input.end());

    // 词法分析
    vector<Token> tokens = tokenize(input);
    if (tokens.empty()) {
        cout << "Syntax Error";
        return 0;
    }
    // 添加结束符
    tokens.push_back({"$", ""});

    // 初始化产生式（必须在buildLR1Table前，因为build会添加增广产生式）
    initProductions();

    // 构建LR(1)表
    buildLR1Table();

    // 创建翻译器
    Translator trans;
    g_trans = &trans;

    // 语法分析
    bool ok = parse(tokens);
    if (!ok || trans.hasError()) {
        cout << "Syntax Error";
        return 0;
    }

    // 输出结果
    trans.printResult();
    return 0;
}
