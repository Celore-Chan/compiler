#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

// 判断字母（A-Z, a-z）
bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
// 判断数字（0-9）
bool isDigit(char c) {
    return c >= '0' && c <= '9';
}
// 判断空白符
bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int main() {
    std::string input;
    char ch;
    while (std::cin.get(ch)) {
        input += ch;
    }

    // 清除所有回车符 \r，确保只处理LF（Linux评测环境）
    input.erase(std::remove(input.begin(), input.end(), '\r'), input.end());

    std::vector<std::pair<std::string, std::string>> tokens;
    std::size_t pos = 0;
    std::size_t len = input.size();

    while (pos < len) {
        char c = input[pos];

        // 1. 跳过空白
        if (isSpace(c)) {
            ++pos;
            continue;
        }

        // 2. 注释处理
        if (c == '/') {
            if (pos + 1 < len) {
                char next = input[pos + 1];
                if (next == '/') {          // 单行注释
                    pos += 2;
                    while (pos < len && input[pos] != '\n') ++pos;
                    continue;
                }
                if (next == '*') {          // 多行注释
                    pos += 2;
                    while (pos < len) {
                        if (input[pos] == '*' && pos + 1 < len && input[pos + 1] == '/') {
                            pos += 2;
                            break;
                        }
                        ++pos;
                    }
                    continue;
                }
            }
        }

        // 3. 数字（包括以点开头的浮点数）
        if (isDigit(c) || (c == '.' && pos + 1 < len && isDigit(input[pos + 1]))) {
            std::string num;
            while (pos < len && (isDigit(input[pos]) || input[pos] == '.')) {
                num += input[pos++];
            }

            int dotCount = 0;
            for (char d : num) if (d == '.') ++dotCount;

            // 错误优先级 1：多个小数点
            if (dotCount > 1) {
                std::cout << "Malformed number: More than one decimal point in a floating point number." << std::endl;
                return 0;
            }
            if (dotCount == 1) {
                // 错误优先级 2：小数点在开始或结尾
                if (num.front() == '.' || num.back() == '.') {
                    std::cout << "Malformed number: Decimal point at the beginning or end of a floating point number." << std::endl;
                    return 0;
                }
                // 错误优先级 3：整数部分前导零
                std::string intPart = num.substr(0, num.find('.'));
                if (intPart.length() > 1 && intPart[0] == '0') {
                    std::cout << "Malformed number: Leading zeros in an integer." << std::endl;
                    return 0;
                }
                tokens.push_back({num, "DOUBLE"});
            } else {
                // 整数前导零
                if (num.length() > 1 && num[0] == '0') {
                    std::cout << "Malformed number: Leading zeros in an integer." << std::endl;
                    return 0;
                }
                tokens.push_back({num, "INT"});
            }
            continue;
        }

        // 4. 标识符与关键字
        if (isAlpha(c)) {
            std::string ident;
            while (pos < len && (isAlpha(input[pos]) || isDigit(input[pos]))) {
                ident += input[pos++];
            }

            if (ident == "int") tokens.push_back({ident, "INTSYM"});
            else if (ident == "double") tokens.push_back({ident, "DOUBLESYM"});
            else if (ident == "scanf") tokens.push_back({ident, "SCANFSYM"});
            else if (ident == "printf") tokens.push_back({ident, "PRINTFSYM"});
            else if (ident == "if") tokens.push_back({ident, "IFSYM"});
            else if (ident == "then") tokens.push_back({ident, "THENSYM"});
            else if (ident == "while") tokens.push_back({ident, "WHILESYM"});
            else if (ident == "do") tokens.push_back({ident, "DOSYM"});
            else tokens.push_back({ident, "IDENT"});
            continue;
        }

        // 5. 运算符与分隔符
        if (c == '|') {
            if (pos + 1 < len && input[pos + 1] == '|') {
                tokens.push_back({"||", "LO"});
                pos += 2;
            } else {
                std::cout << "Unrecognizable characters." << std::endl;
                return 0;
            }
            continue;
        }
        if (c == '&') {
            if (pos + 1 < len && input[pos + 1] == '&') {
                tokens.push_back({"&&", "LO"});
                pos += 2;
            } else {
                std::cout << "Unrecognizable characters." << std::endl;
                return 0;
            }
            continue;
        }
        if (c == '=') {
            if (pos + 1 < len && input[pos + 1] == '=') {
                tokens.push_back({"==", "RO"});
                pos += 2;
            } else {
                tokens.push_back({"=", "AO"});
                ++pos;
            }
            continue;
        }
        if (c == '!') {
            if (pos + 1 < len && input[pos + 1] == '=') {
                tokens.push_back({"!=", "RO"});
                pos += 2;
            } else {
                tokens.push_back({"!", "LO"});
                ++pos;
            }
            continue;
        }
        if (c == '>') {
            if (pos + 1 < len && input[pos + 1] == '=') {
                tokens.push_back({">=", "RO"});
                pos += 2;
            } else {
                tokens.push_back({">", "RO"});
                ++pos;
            }
            continue;
        }
        if (c == '<') {
            if (pos + 1 < len && input[pos + 1] == '=') {
                tokens.push_back({"<=", "RO"});
                pos += 2;
            } else {
                tokens.push_back({"<", "RO"});
                ++pos;
            }
            continue;
        }

        // 单字符符号
        switch (c) {
            case '+': tokens.push_back({"+", "PLUS"}); ++pos; continue;
            case '-': tokens.push_back({"-", "MINUS"}); ++pos; continue;
            case '*': tokens.push_back({"*", "TIMES"}); ++pos; continue;
            case '/': tokens.push_back({"/", "DIVISION"}); ++pos; continue;
            case ',': tokens.push_back({",", "COMMA"}); ++pos; continue;
            case '(': tokens.push_back({"(", "BRACE"}); ++pos; continue;
            case ')': tokens.push_back({")", "BRACE"}); ++pos; continue;
            case '{': tokens.push_back({"{", "BRACE"}); ++pos; continue;
            case '}': tokens.push_back({"}", "BRACE"}); ++pos; continue;
            case ';': tokens.push_back({";", "SEMICOLON"}); ++pos; continue;
            default:
                std::cout << "Unrecognizable characters." << std::endl;
                return 0;
        }
    }

    // 输出所有token
    for (const auto &tok : tokens) {
        std::cout << tok.first << " " << tok.second << std::endl;
    }
    // 如果没有任何token（例如只有空白或注释），仍需输出一个空行
    if (tokens.empty()) {
        std::cout << std::endl;
    }

    return 0;
}
