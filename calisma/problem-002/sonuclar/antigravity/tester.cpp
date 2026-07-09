#include "src/include/int2048.h"
#include <iostream>
#include <string>

using namespace std;
using namespace sjtu;

int main() {
    int2048 a("12345678901234567890");
    int2048 b("98765432109876543210");
    cout << "a+b: "; (a + b).print(); cout << "\n";
    cout << "a-b: "; (a - b).print(); cout << "\n";
    cout << "b-a: "; (b - a).print(); cout << "\n";
    
    int2048 c("10"), d("-3");
    cout << "10 / -3: "; (c / d).print(); cout << "\n";
    cout << "10 % -3: "; (c % d).print(); cout << "\n";
    
    int2048 e("-10"), f("3");
    cout << "-10 / 3: "; (e / f).print(); cout << "\n";
    cout << "-10 % 3: "; (e % f).print(); cout << "\n";
    
    int2048 g("-10"), h("-3");
    cout << "-10 / -3: "; (g / h).print(); cout << "\n";
    cout << "-10 % -3: "; (g % h).print(); cout << "\n";

    int2048 m1("1000000000000000000000000000000");
    int2048 m2("2000000000000000000000000000000");
    cout << "m1*m2: "; (m1 * m2).print(); cout << "\n";
    
    cout << "Done\n";
    return 0;
}
