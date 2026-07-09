// 002 int2048 canonical test driver (kurcalanamaz).
// Girdi: her satir "op a b".  op: + - * / % < > == != <= >=
// Cikti: her islem sonucu (aritmetik -> sayi; karsilastirma -> 0/1), satir satir.
#include "int2048.h"
#include <iostream>
#include <string>

int main() {
    std::string op;
    while (std::cin >> op) {
        sjtu::int2048 a, b;
        std::cin >> a >> b;
        if (op == "+") std::cout << (a + b);
        else if (op == "-") std::cout << (a - b);
        else if (op == "*") std::cout << (a * b);
        else if (op == "/") std::cout << (a / b);
        else if (op == "%") std::cout << (a % b);
        else if (op == "<") std::cout << (a < b);
        else if (op == ">") std::cout << (a > b);
        else if (op == "==") std::cout << (a == b);
        else if (op == "!=") std::cout << (a != b);
        else if (op == "<=") std::cout << (a <= b);
        else if (op == ">=") std::cout << (a >= b);
        std::cout << "\n";
    }
    return 0;
}
