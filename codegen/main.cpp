#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <algorithm>
#include <climits>
#include <cctype>

using namespace std;

// 输出流
ostringstream out;

// 待用信息（附加在四元式操作数上）
struct Info {
    int use;   // 下一次引用点编号，-1 表示不再使用
    bool live; // 是否活跃（在基本块出口之后）
    Info() : use(-1), live(false) {}
};

// 四元式
struct Quad {
    int index;
    string op;
    string x, y, z;
    Info xInfo, yInfo, zInfo;
};

// 符号表条目
struct Var {
    string name;
    int type;   // 0 int, 1 real (本实验不作区分)
    int size;   // 4 或 8
    int offset; // 栈帧偏移，-1 表示未分配
    bool isTemp;
    set<string> avalRegs; // 当前存放该变量值的寄存器集合
    bool avalMem;         // 值是否同时在内存中

    Var() : type(0), size(4), offset(-1), isTemp(true), avalMem(false) {}
};

// 基本块
struct Block {
    int startIdx, endIdx;
    vector<Quad*> quads;
};

// 全局变量
vector<Quad> quads;
vector<Block> blocks;
map<string, Var> symTable;
map<string, set<string>> Rval; // 寄存器描述符：R0,R1,R2 -> 存放的变量名集合
int nextTempOffset;             // 下一个可分配的临时变量偏移

// ----------------------------------------------------------------------
// 辅助函数
// ----------------------------------------------------------------------

// 判断字符串是否为已登记变量
bool isVariable(const string &s) {
    return symTable.count(s) > 0;
}

// 判断字符串是否为数字常量（整数或浮点数）
bool isNumber(const string &s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-') i++;
    bool hasDot = false, hasDigit = false;
    for (; i < s.size(); i++) {
        if (s[i] == '.') {
            if (hasDot) return false;
            hasDot = true;
        } else if (isdigit(s[i])) {
            hasDigit = true;
        } else {
            return false;
        }
    }
    return hasDigit;
}

// 获取或分配变量的栈偏移
int getOrAllocOffset(const string &name) {
    Var &v = symTable[name];
    if (v.offset != -1) return v.offset;
    v.offset = nextTempOffset;
    nextTempOffset += v.size;
    return v.offset;
}

// 将操作数格式化为目标代码形式：寄存器、[ebp-offset] 或数字
string formatOperand(const string &op) {
    if (op.empty() || op == "-") return "-";
    // 寄存器直接返回
    if (op.size() == 2 && op[0] == 'R' && op[1] >= '0' && op[1] <= '2')
        return op;
    if (isVariable(op)) {
        int off = getOrAllocOffset(op);
        return "[ebp-" + to_string(off) + "]";
    }
    return op; // 数字常量
}

// 若变量在某寄存器中，返回该寄存器名；否则返回变量名/常量本身
string getAvalRegOrVar(const string &name) {
    if (isVariable(name) && !symTable[name].avalRegs.empty())
        return *symTable[name].avalRegs.begin();
    return name;
}

// 在基本块 [curIdx+1, endIdx] 范围内查找变量 var 的下一次引用点
int getNextUse(const string &var, int curIdx, int endIdx) {
    for (int i = curIdx + 1; i <= endIdx; i++) {
        if (quads[i].x == var || quads[i].y == var)
            return i;
    }
    return INT_MAX; // 无后续引用
}

// 释放变量占用的寄存器（仅针对临时变量，跨基本块不保留）
void releaseReg(const string &var, const Block &b) {
    if (!isVariable(var)) return;
    if (symTable[var].isTemp) {
        for (const string &reg : symTable[var].avalRegs)
            Rval[reg].erase(var);
        symTable[var].avalRegs.clear();
    }
}

// 为四元式 q 分配寄存器，考虑基本块范围
string getReg(Quad *q, int blockEndIdx) {
    string x = q->x, y = q->y, z = q->z;

    // 情形1：x 独占某寄存器且满足复用条件
    if (isVariable(x)) {
        for (const string &reg : symTable[x].avalRegs) {
            if (Rval[reg].size() == 1 && Rval[reg].count(x) &&
                (x == z || q->xInfo.live == false))
                return reg;
        }
    }

    // 情形2：有空闲寄存器则直接分配
    for (const string &reg : {"R0", "R1", "R2"})
        if (Rval[reg].empty()) return reg;

    // 情形3：选择溢出的寄存器
    string selected;
    // 优先选择所有值都在内存中的寄存器
    for (const string &reg : {"R0", "R1", "R2"}) {
        bool allInMem = true;
        for (const string &a : Rval[reg])
            if (!symTable[a].avalMem) { allInMem = false; break; }
        if (allInMem) { selected = reg; break; }
    }
    // 否则选择最远下一次使用的寄存器
    if (selected.empty()) {
        int bestMinUse = -1;
        for (const string &reg : {"R0", "R1", "R2"}) {
            int minUse = INT_MAX;
            for (const string &a : Rval[reg]) {
                int use = getNextUse(a, q->index, blockEndIdx);
                if (use < minUse) minUse = use;
            }
            if (minUse > bestMinUse) {
                bestMinUse = minUse;
                selected = reg;
            }
        }
    }

    // 溢出 selected 中保存的变量
    vector<string> toRemove(Rval[selected].begin(), Rval[selected].end());
    for (const string &a : toRemove) {
        if (!symTable[a].avalMem && a != z) {
            int off = getOrAllocOffset(a);
            out << "mov [ebp-" << off << "], " << selected << "\n";
            symTable[a].avalMem = true;
        }
        if (a == x || (a == y && Rval[selected].count(x))) {
            symTable[a].avalRegs.insert(selected);
            symTable[a].avalMem = true;
        } else {
            symTable[a].avalRegs.erase(selected);
            symTable[a].avalMem = true;
        }
        Rval[selected].erase(a);
    }
    return selected;
}

// 算术/逻辑运算指令映射
string mapArith(const string &op) {
    if (op == "+") return "add";
    if (op == "-") return "sub";
    if (op == "*") return "mul";
    if (op == "/") return "div";
    if (op == "&&") return "and";
    if (op == "||") return "or";
    return "";
}

// set 指令映射
string mapSet(const string &op) {
    if (op == "==") return "sete";
    if (op == "!=") return "setne";
    if (op == "<")  return "setl";
    if (op == "<=") return "setle";
    if (op == ">")  return "setg";
    if (op == ">=") return "setge";
    return "";
}

// ----------------------------------------------------------------------
// 主函数
// ----------------------------------------------------------------------
int main() {
    // ---------- 读符号表 ----------
    int varCount;
    cin >> varCount;
    vector<Var> userVars(varCount);
    for (int i = 0; i < varCount; i++) {
        string name, nullStr;
        int type, offset;
        cin >> name >> type >> nullStr >> offset;
        userVars[i].name = name;
        userVars[i].type = type;
        userVars[i].size = (type == 1) ? 8 : 4;
        userVars[i].offset = offset;
        userVars[i].isTemp = false;   // 用户声明的变量和形参均视为非临时
    }

    int tempCount, quadCount;
    cin >> tempCount >> quadCount;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    // ---------- 读四元式 ----------
    quads.resize(quadCount);
    for (int i = 0; i < quadCount; i++) {
        string line;
        getline(cin, line);
        Quad q;
        q.index = i;
        size_t colon = line.find(':');
        string content = line.substr(colon + 1);
        size_t lparen = content.find('(');
        size_t rparen = content.rfind(')');
        string args = content.substr(lparen + 1, rparen - lparen - 1);
        stringstream ss(args);
        vector<string> parts(4);
        for (int j = 0; j < 4; j++) {
            getline(ss, parts[j], ',');
            // trim
            auto trim = [](string &s) {
                size_t b = s.find_first_not_of(" \t");
                size_t e = s.find_last_not_of(" \t");
                if (b == string::npos) s = "";
                else s = s.substr(b, e - b + 1);
            };
            trim(parts[j]);
        }
        q.op = parts[0];
        q.x  = parts[1];
        q.y  = parts[2];
        q.z  = parts[3];
        quads[i] = q;
    }

    // ---------- 构建符号表（临时变量与 TB 变量） ----------
    for (Quad &q : quads) {
        for (string s : {q.x, q.y, q.z}) {
            if (s.empty() || s == "-" || isNumber(s)) continue;
            if (symTable.count(s)) continue;
            if (s.substr(0, 2) == "TB") {
                int idx = stoi(s.substr(2));
                if (idx >= 0 && idx < (int)userVars.size()) {
                    Var v = userVars[idx];
                    v.name = s;
                    v.isTemp = false;
                    symTable[s] = v;
                }
            } else if (s[0] == 'T') {
                Var v;
                v.name = s;
                v.isTemp = true;
                // 根据后缀简单区分整型/实型
                if (s.size() >= 2 && s.substr(s.size()-2) == "_i") {
                    v.type = 0; v.size = 4;
                } else if (s.size() >= 2 && s.substr(s.size()-2) == "_d") {
                    v.type = 1; v.size = 8;
                } else {
                    v.type = 0; v.size = 4;
                }
                v.offset = -1;
                symTable[s] = v;
            }
        }
    }

    // 计算局部变量总空间，决定临时变量起始偏移
    int totalLocal = 0;
    for (Var &v : userVars)
        totalLocal = max(totalLocal, v.offset + v.size);
    nextTempOffset = totalLocal;

    // ---------- 基本块划分 ----------
    vector<bool> isEntry(quadCount, false);
    isEntry[0] = true;
    for (int i = 0; i < quadCount; i++) {
        string op = quads[i].op;
        if (op == "jnz" || (op.size() >= 2 && op[0] == 'j' && op != "j")) {
            int target = stoi(quads[i].z);
            isEntry[target] = true;
            if (i + 1 < quadCount) isEntry[i + 1] = true;
        } else if (op == "j") {
            int target = stoi(quads[i].z);
            isEntry[target] = true;
        } else if (op == "R" || op == "W") {
            isEntry[i] = true;   // 视作基本块入口
        }
    }

    int i = 0;
    while (i < quadCount) {
        if (isEntry[i]) {
            int start = i;
            if (i == quadCount - 1) {
                blocks.push_back({start, i, {}});
                break;
            }
            int j = i + 1;
            for (; j < quadCount; j++) {
                if (isEntry[j]) {
                    blocks.push_back({start, j - 1, {}});
                    i = j;
                    break;
                }
                string op = quads[j].op;
                bool isTransfer = (op == "j" || op == "jnz" ||
                    (op.size() >= 2 && op[0] == 'j' && op != "j") ||
                    op == "End" || op == "Syntax Error");
                if (isTransfer) {
                    blocks.push_back({start, j, {}});
                    i = j + 1;
                    break;
                }
            }
            if (j >= quadCount) {
                blocks.push_back({start, quadCount - 1, {}});
                i = quadCount;
            }
        } else {
            i++;
        }
    }

    // 将四元式指针填入基本块
    for (Block &b : blocks)
        for (int k = b.startIdx; k <= b.endIdx; k++)
            b.quads.push_back(&quads[k]);

    // ---------- 求解待用信息与活跃信息 ----------
    for (Block &b : blocks) {
        // 符号表中变量的初始状态
        map<string, int> use;
        map<string, bool> live;
        for (auto &var : symTable) {
            use[var.first] = -1;
            live[var.first] = !var.second.isTemp; // 非临时变量出口活跃
        }
        // 逆序扫描基本块内四元式
        for (int k = (int)b.quads.size() - 1; k >= 0; k--) {
            Quad *q = b.quads[k];
            // z：左值，先保存当前待用信息再置为非待用
            if (isVariable(q->z)) {
                q->zInfo.use = use[q->z];
                q->zInfo.live = live[q->z];
                use[q->z] = -1;
                live[q->z] = false;
            }
            // x, y：右值，记录当前待用信息后更新为当前四元式
            if (isVariable(q->x)) {
                q->xInfo.use = use[q->x];
                q->xInfo.live = live[q->x];
                use[q->x] = q->index;
                live[q->x] = true;
            }
            if (isVariable(q->y)) {
                q->yInfo.use = use[q->y];
                q->yInfo.live = live[q->y];
                use[q->y] = q->index;
                live[q->y] = true;
            }
        }
    }

    // 收集所有需要标签的四元式编号（跳转目标）
    set<int> labelIndices;
    for (Quad &q : quads) {
        if (q.op == "j" || q.op == "jnz" || (q.op.size() >= 2 && q.op[0] == 'j' && q.op != "j")) {
            labelIndices.insert(stoi(q.z));
        }
    }

    // ---------- 目标代码生成 ----------
    for (Block &b : blocks) {
        // 若基本块第一条指令需要标签，则输出
        int firstIdx = b.quads[0]->index;
        if (labelIndices.count(firstIdx)) {
            out << "?" << firstIdx << ":\n";
            labelIndices.erase(firstIdx); // 防止重复
        }

        // 重置寄存器与地址描述符
        Rval.clear();
        for (auto &var : symTable) {
            var.second.avalRegs.clear();
            var.second.avalMem = !var.second.isTemp; // 非临时变量默认在内存
        }

        // 处理基本块内部的非控制转移指令
        for (Quad *q : b.quads) {
            string op = q->op;
            // 控制转移指令留到块结尾处理
            if (op == "j" || op == "jnz" ||
                (op.size() >= 2 && op[0] == 'j' && op != "j") ||
                op == "End" || op == "Syntax Error")
                continue;

            if (op == "R") {
                int off = getOrAllocOffset(q->z);
                out << "jmp ?read([ebp-" << off << "])\n";
            } else if (op == "W") {
                int off = getOrAllocOffset(q->z);
                out << "jmp ?write([ebp-" << off << "])\n";
            } else if (op == "=" || op == "+" || op == "-" || op == "*" || op == "/" ||
                       op == "&&" || op == "||" || op == "!" ||
                       op == "==" || op == "!=" || op == "<" || op == "<=" ||
                       op == ">" || op == ">=") {
                string Rz = getReg(q, b.endIdx);
                string x_prime = getAvalRegOrVar(q->x);
                string y_prime = getAvalRegOrVar(q->y);

                // 若 x 不在 Rz 中，则生成 mov
                if (x_prime != Rz)
                    out << "mov " << Rz << ", " << formatOperand(x_prime) << "\n";

                if (op == "=") {
                    // 无需额外指令
                } else if (op == "!") {
                    out << "not " << Rz << "\n";
                } else if (op == "+" || op == "-" || op == "*" || op == "/" ||
                           op == "&&" || op == "||") {
                    out << mapArith(op) << " " << Rz << ", " << formatOperand(y_prime) << "\n";
                } else if (op == "==" || op == "!=" || op == "<" || op == "<=" ||
                           op == ">" || op == ">=") {
                    out << "cmp " << Rz << ", " << formatOperand(y_prime) << "\n";
                    out << mapSet(op) << " " << Rz << "\n";
                }

                // 更新地址描述符
                if (x_prime == Rz && isVariable(q->x))
                    symTable[q->x].avalRegs.erase(Rz);
                if (y_prime == Rz && isVariable(q->y))
                    symTable[q->y].avalRegs.erase(Rz);

                // 将 z 绑定到 Rz
                Rval[Rz].clear();
                Rval[Rz].insert(q->z);
                symTable[q->z].avalRegs.clear();
                symTable[q->z].avalRegs.insert(Rz);
                symTable[q->z].avalMem = false;

                // 释放 x, y 可能占用的寄存器
                releaseReg(q->x, b);
                releaseReg(q->y, b);
            }
        }

        // 活跃变量存回内存（按字典序）
        vector<string> liveVars;
        for (auto &var : symTable)
            if (!var.second.isTemp)   // 出口活跃的变量
                liveVars.push_back(var.first);
        sort(liveVars.begin(), liveVars.end());
        for (const string &vname : liveVars) {
            Var &v = symTable[vname];
            if (!v.avalMem) {
                string reg = v.avalRegs.empty() ? "" : *v.avalRegs.begin();
                if (!reg.empty()) {
                    int off = getOrAllocOffset(vname);
                    out << "mov [ebp-" << off << "], " << reg << "\n";
                    v.avalMem = true;
                }
            }
        }

        // 处理基本块结尾的控制转移指令
        Quad *last = b.quads.back();
        string op = last->op;
        if (op == "j") {
            out << "jmp ?" << last->z << "\n";
        } else if (op == "jnz") {
            string x_prime = getAvalRegOrVar(last->x);
            if (x_prime == last->x) { // 不在寄存器中
                string newReg = getReg(last, b.endIdx);
                out << "mov " << newReg << ", " << formatOperand(last->x) << "\n";
                x_prime = newReg;
            }
            out << "cmp " << x_prime << ", 0\n";
            out << "jne ?" << last->z << "\n";
        } else if (op.size() > 0 && op[0] == 'j' && op != "j") {
            string cond = op.substr(1);
            string jmpCond;
            if (cond == "==") jmpCond = "je";
            else if (cond == "!=") jmpCond = "jne";
            else if (cond == "<") jmpCond = "jl";
            else if (cond == "<=") jmpCond = "jle";
            else if (cond == ">") jmpCond = "jg";
            else if (cond == ">=") jmpCond = "jge";

            string x_prime = getAvalRegOrVar(last->x);
            string y_prime = getAvalRegOrVar(last->y);
            if (x_prime == last->x) {
                string newReg = getReg(last, b.endIdx);
                out << "mov " << newReg << ", " << formatOperand(last->x) << "\n";
                x_prime = newReg;
            }
            out << "cmp " << x_prime << ", " << formatOperand(y_prime) << "\n";
            out << jmpCond << " ?" << last->z << "\n";
        } else if (op == "End") {
            out << "halt\n";
        }
    }

    cout << out.str();
    return 0;
}
