# 词法分析器

## 项目简介
本项目实现了一个C语言子集的词法分析器，用于识别输入源程序中的词法单元（Token）。该分析器支持整数、浮点数、标识符、关键字、运算符和分隔符的识别，并能检测词法错误。

## 功能特性
- **数字识别**：支持整数（INT）和浮点数（DOUBLE），能检测以下错误：
  - 多个小数点
  - 小数点位于开头或结尾
  - 整数部分前导零
- **标识符与关键字**：识别 `int`, `double`, `scanf`, `printf`, `if`, `then`, `while`, `do` 等关键字，其他标识符标记为 `IDENT`
- **注释处理**：支持单行注释 `//` 和多行注释 `/* */`
- **运算符识别**：支持 `+`, `-`, `*`, `/`, `=`, `==`, `!=`, `>`, `>=`, `<`, `<=`, `&&`, `||`, `!`
- **分隔符识别**：支持 `,`, `(`, `)`, `{`, `}`, `;`
- **错误处理**：对不可识别字符输出错误信息

## 编译与运行

### 编译
```bash
g++ main.cpp -o lexer
# 或直接运行 build.sh
./build.sh
```

### 运行
```bash
./lexer < input.txt
```

### 输入格式
输入为源代码文本，通过标准输入读取。

### 输出格式
每行输出一个Token，格式为：
```
<字面量> <类别>
```
例如：
```
int INTSYM
main IDENT
( BRACE
) BRACE
```

如果没有任何Token（只有空白或注释），输出一个空行。

### 错误输出
遇到错误时输出以下信息之一并退出：
- `Malformed number: More than one decimal point in a floating point number.`
- `Malformed number: Decimal point at the beginning or end of a floating point number.`
- `Malformed number: Leading zeros in an integer.`
- `Unrecognizable characters.`

## 词法单元类别表
| 类别 | 说明 |
|------|------|
| INTSYM | 关键字 `int` |
| DOUBLESYM | 关键字 `double` |
| SCANFSYM | 关键字 `scanf` |
| PRINTFSYM | 关键字 `printf` |
| IFSYM | 关键字 `if` |
| THENSYM | 关键字 `then` |
| WHILESYM | 关键字 `while` |
| DOSYM | 关键字 `do` |
| IDENT | 标识符 |
| INT | 整数字面量 |
| DOUBLE | 浮点数字面量 |
| LO | 逻辑运算符 (`&&`, `\|\|`, `!`) |
| RO | 关系运算符 (`==`, `!=`, `>`, `>=`, `<`, `<=`) |
| AO | 赋值运算符 (`=`) |
| PLUS | `+` |
| MINUS | `-` |
| TIMES | `*` |
| DIVISION | `/` |
| COMMA | `,` |
| BRACE | `(`, `)`, `{`, `}` |
| SEMICOLON | `;` |

## 文件结构
```
├── main.cpp      # 主程序源码
├── build.sh      # 编译脚本
└── README.md     # 本文件
```

## 技术要点
- 基于状态机原理，逐字符扫描输入
- 使用 `std::cin.get()` 逐字符读取，处理LF换行符
- 消除回车符 `\r` 以适配Linux评测环境