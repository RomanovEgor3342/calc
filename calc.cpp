#include "calc.hpp"
#include <stack>
#include <cctype>
#include <map>
#include <cstring>
#include <vector>

int get_priority(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

// для корректного выполнения вычислений используется обратная польская запись
std::vector<std::string> to_rpn(const std::string& expr) {
    // преобразуем к ОПЗ
    std::vector<std::string> output;
    std::stack<char> ops;
    size_t i = 0;

    while (i < expr.size()) {
        if (std::isdigit(expr[i])) {
            std::string num;
            while (i < expr.size() && (std::isdigit(expr[i]) || expr[i] == '.')) {
                num += expr[i++];
            }
            output.push_back(num);
        } else if (strchr("+-*/", expr[i])) {
            char op = expr[i++];
            while (!ops.empty() && get_priority(ops.top()) >= get_priority(op)) {
                output.push_back(std::string(1, ops.top()));
                ops.pop();
            }
            ops.push(op);
        } else {
            ++i; // пропускаем пробелы или невалидные символы
        }
    }

    while (!ops.empty()) {
        output.push_back(std::string(1, ops.top()));
        ops.pop();
    }

    return output;
}

double eval_rpn(const std::vector<std::string>& tokens) {
    //вычисляем выражение записанное в Форме ОПЗ
    std::stack<double> stk;
    for (const auto& token : tokens) {
        if (std::isdigit(token[0])) {
            stk.push(std::stod(token));
        } else if (token.size() == 1 && strchr("+-*/", token[0])) {
            double b = stk.top(); stk.pop();
            double a = stk.top(); stk.pop();
            switch (token[0]) {
                case '+': stk.push(a + b); break;
                case '-': stk.push(a - b); break;
                case '*': stk.push(a * b); break;
                case '/': stk.push(a / b); break;
            }
        }
    }
    return stk.top();
}

double eval_expr(const std::string& expr) {
    // вычисления выражения, записанного в виде строки
    auto rpn = to_rpn(expr);
    return eval_rpn(rpn);
}