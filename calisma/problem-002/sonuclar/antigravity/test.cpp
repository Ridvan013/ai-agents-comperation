#include <iostream>
#include <string>
#include <vector>
#include <complex>

namespace sjtu {
class int2048 {
private:
    std::vector<int> a;
    bool sign;
    void trim() {
        while (a.size() > 1 && a.back() == 0) a.pop_back();
        if (a.size() == 1 && a[0] == 0) sign = false;
    }
public:
    int2048() : sign(false) { a.push_back(0); }
    int2048(long long v) {
        if (v < 0) { sign = true; v = -v; } else { sign = false; }
        if (v == 0) a.push_back(0);
        while (v > 0) { a.push_back(v % 10000); v /= 10000; }
    }
    int2048(const std::string &s) { read(s); }
    void read(const std::string &s) {
        a.clear();
        sign = false;
        int start = 0;
        if (s.length() > 0 && s[0] == '-') { sign = true; start = 1; }
        else if (s.length() > 0 && s[0] == '+') { start = 1; }
        for (int i = (int)s.length() - 1; i >= start; i -= 4) {
            int val = 0;
            int p = 1;
            for (int j = 0; j < 4 && i - j >= start; ++j) {
                val += (s[i - j] - '0') * p;
                p *= 10;
            }
            a.push_back(val);
        }
        if (a.empty()) a.push_back(0);
        trim();
    }
    void print() const {
        if (sign) std::cout << '-';
        std::cout << a.back();
        for (int i = (int)a.size() - 2; i >= 0; --i) {
            int v = a[i];
            if (v < 1000) std::cout << '0';
            if (v < 100) std::cout << '0';
            if (v < 10) std::cout << '0';
            std::cout << v;
        }
    }
    int cmp_abs(const int2048 &o) const {
        if (a.size() != o.a.size()) return a.size() < o.a.size() ? -1 : 1;
        for (int i = (int)a.size() - 1; i >= 0; --i) {
            if (a[i] != o.a[i]) return a[i] < o.a[i] ? -1 : 1;
        }
        return 0;
    }
    void sub_abs(const int2048 &o) {
        int borrow = 0;
        for (int i = 0; i < (int)a.size(); ++i) {
            long long diff = a[i] - borrow - (i < (int)o.a.size() ? o.a[i] : 0);
            if (diff < 0) { diff += 10000; borrow = 1; }
            else { borrow = 0; }
            a[i] = diff;
        }
        trim();
    }
    void add_abs(const int2048 &o) {
        int carry = 0;
        int n = a.size(), m = o.a.size();
        int sz = n > m ? n : m;
        for (int i = 0; i < sz || carry; ++i) {
            if (i == (int)a.size()) a.push_back(0);
            long long sum = a[i] + carry + (i < m ? o.a[i] : 0);
            a[i] = sum % 10000;
            carry = sum / 10000;
        }
        trim();
    }
    
    // We only need basic operators for testing
    friend int2048 operator+(int2048 A, const int2048 &B) {
        if (A.sign == B.sign) {
            A.add_abs(B);
        } else {
            if (A.cmp_abs(B) >= 0) {
                A.sub_abs(B);
            } else {
                int2048 t = B;
                t.sub_abs(A);
                A = t;
            }
        }
        return A;
    }
    friend int2048 operator-(int2048 A, const int2048 &B) {
        int2048 t = B;
        t.sign = !t.sign;
        if (t.a.size() == 1 && t.a[0] == 0) t.sign = false;
        return A + t;
    }
    int2048 operator*(const int2048 &o) const {
        int2048 res(0); // placeholder
        return res; // just for now
    }
    void div_abs(const int2048 &A, const int2048 &B, int2048 &Q, int2048 &R) {
        if (A.cmp_abs(B) < 0) { Q = int2048(0); R = A; return; }
        Q.a.assign(A.a.size() - B.a.size() + 1, 0);
        Q.sign = false;
        int2048 rem(0);
        for (int i = (int)A.a.size() - 1; i >= 0; --i) {
            if (!(rem.a.size() == 1 && rem.a[0] == 0)) rem.a.insert(rem.a.begin(), A.a[i]);
            else rem.a[0] = A.a[i];
            
            int l = 0, r = 9999, ans = 0;
            while (l <= r) {
                int mid = l + (r - l) / 2;
                int2048 guess = B;
                long long carry = 0;
                for (int j = 0; j < (int)guess.a.size(); ++j) {
                    long long cur = guess.a[j] * 1LL * mid + carry;
                    guess.a[j] = cur % 10000;
                    carry = cur / 10000;
                }
                if (carry > 0) guess.a.push_back(carry);
                if (guess.cmp_abs(rem) <= 0) { ans = mid; l = mid + 1; }
                else r = mid - 1;
            }
            if (i < (int)Q.a.size()) Q.a[i] = ans;
            if (ans > 0) {
                int2048 guess = B;
                long long carry = 0;
                for (int j = 0; j < (int)guess.a.size(); ++j) {
                    long long cur = guess.a[j] * 1LL * ans + carry;
                    guess.a[j] = cur % 10000;
                    carry = cur / 10000;
                }
                if (carry > 0) guess.a.push_back(carry);
                rem.sub_abs(guess);
            }
        }
        Q.trim();
        R = rem;
    }
};
}

int main() {
    sjtu::int2048 A("10"), B("3");
    sjtu::int2048 Q, R;
    A.div_abs(A, B, Q, R);
    Q.print(); std::cout << " "; R.print(); std::cout << "\n";
    return 0;
}
