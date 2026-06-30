## 项目概述
本项目实现了一个完整的C语言子集编译器前端，包含三个核心项目：
1. **词法分析器**：将源代码转换为Token流
2. **语法分析器 + 语义翻译器**：将Token流解析并生成四元式中间代码
3. **目标代码生成器**：将四元式翻译为x86汇编风格目标代码

## Highlights

- Hand-written compiler (No Flex/Bison)
- Canonical LR(1) Parser
- Syntax-directed Translation
- Quadruple IR
- Register Allocation
- x86-style Target Code Generation

## Statistics

26 Token Types

65 Grammar Productions

150+ LR(1) States

7 Language Constructs

14 Target Instructions

## 整体架构
```
源代码 (C子集)
    ↓
词法分析器
    ↓
Token流
    ↓
LR(1)语法分析器 + 语义翻译器
    ↓
四元式 + 符号表
    ↓
目标代码生成器
    ↓
x86汇编风格目标代码
```

## 实验对照表

| 输入 | 输出 | 核心算法 | 文件 |
|------|------|----------|------|
| C子集源代码 | Token列表 | 状态机扫描 | `lexer/` |
| C子集源代码 | 符号表 + 四元式 | LR(1)语法分析 + 回填 | `parser/` |
| 符号表 + 四元式 | x86汇编代码 | 寄存器分配 + 基本块划分 | `codegen/` |

## 环境要求
- **操作系统**：Linux / macOS / Windows (WSL)
- **编译器**：g++ 或 clang++，支持 C++11 及以上标准
- **构建工具**：bash（用于执行 build.sh）

## 快速开始

### 1. 克隆或下载项目
```bash
git clone <repository-url>
cd compiler-lab
```

### 2. 编译所有项目
```bash
cd lexer && ./build.sh && cd ..
cd parser && ./build.sh && cd ..
cd codegen && ./build.sh && cd ..
```

### 3. 运行各项目
```bash
# 词法分析
./lexer/Main < test_program.c

# 语法分析 + 翻译
./parser/Main < test_program.c

# 目标代码生成
./parser/Main < test_program.c > quads.txt
./codegen/Main < quads.txt
```

## 支持的C语言子集语法

### 声明
```c
int a, b, c;
double x, y;
```

### 语句
```c
a = b + c * 2;
scanf(a, b);
printf(x, y);
if a > b then x = 1;
while i < 10 do { i = i + 1; }
```

### 表达式
- 算术：`+`, `-`, `*`, `/`
- 逻辑：`&&`, `||`, `!`
- 关系：`==`, `!=`, `<`, `<=`, `>`, `>=`

### 复合语句
```c
{
    a = 1;
    b = 2;
}
```

## 项目文件结构
```
├── lexer/
│   ├── main.cpp          # 词法分析器源码
│   ├── build.sh          # 编译脚本
│   └── README.md         # 独立README
├── parser/
│   ├── main.cpp          # 语法分析器源码
│   ├── build.sh          # 编译脚本
│   └── README.md         # 独立README
├── codegen/
│   ├── main.cpp          # 目标代码生成器源码
│   ├── build.sh          # 编译脚本
│   └── README.md         # 独立README
└── README.md             # 本文件（汇总）
```

## 技术栈对比

| 模块 | 数据结构 | 算法 |
|------|----------|------|
| 词法分析 | 字符串、Token向量 | 确定性有限自动机(DFA) |
| 语法分析 | LR(1)项目集、分析表 | LR(1)分析算法 |
| 语义翻译 | 符号表、四元式向量 | 语法制导翻译(SDT) + 回填 |
| 目标代码生成 | 基本块、寄存器描述符 | 寄存器分配算法 |

## 测试示例

### 输入程序
```c
int a, b;
double c;
a = 5;
b = a + 3;
c = b * 2.5;
printf(c);
```

### 词法分析器输出（Token）
```
int INTSYM
a IDENT
, COMMA
b IDENT
; SEMICOLON
double DOUBLESYM
c IDENT
; SEMICOLON
a IDENT
= AO
5 INT
; SEMICOLON
...
```

### 语法分析器输出（四元式）
```
符号表:
3
a 0 null 0
b 0 null 4
c 1 null 8

临时变量数: 3

四元式数: 8
0: (=, 5, -, T0_i)
1: (=, T0_i, -, a)
2: (=, 3, -, T1_i)
3: (+, a, T1_i, T2_i)
4: (=, T2_i, -, b)
5: (=, 2.5, -, T3_d)
6: (*, b, T3_d, T4_d)
7: (=, T4_d, -, c)
8: (W, -, -, c)
9: (End, -, -, -)
```

### 目标代码生成器输出（目标代码）
```asm
?0:
mov R0, 5
mov [ebp-12], R0
mov R1, [ebp-12]
add R1, 3
mov [ebp-12], R1
mov R2, 2.5
mov [ebp-16], R2
mov R0, [ebp-16]
mul R0, [ebp-12]
mov [ebp-8], R0
jmp ?write([ebp-8])
halt
```

## 常见问题

### Q: 编译时出现 C++ 标准库相关错误？
A: 确保使用 C++11 或更高版本编译。`build.sh` 中已指定 `-std=c++11`。

### Q: 目标代码生成器的输入格式是什么？
A: 必须是语法分析器的完整输出格式，包含符号表、临时变量数和四元式列表。

### Q: 目标代码中 `[ebp-offset]` 的含义？
A: 表示栈帧中相对 `ebp` 寄存器的偏移量，用于访问局部变量。

### Q: 如何调试语法分析错误？
A: 程序遇到语法错误会输出 `Syntax Error`。可以检查输入程序是否符合支持的语法子集。
