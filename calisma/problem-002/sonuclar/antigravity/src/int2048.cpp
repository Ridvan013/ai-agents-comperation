#include "int2048.h"

namespace sjtu {

// Math constant
const double PI = 3.14159265358979323846;

void change(std::vector<std::complex<double>> &y, int len) {
    std::vector<int> rev(len);
    for (int i = 0; i < len; ++i) {
        rev[i] = rev[i >> 1] >> 1;
        if (i & 1) rev[i] |= (len >> 1);
    }
    for (int i = 0; i < len; ++i) {
        if (i < rev[i]) {
            std::complex<double> t = y[i];
            y[i] = y[rev[i]];
            y[rev[i]] = t;
        }
    }
}

void fft(std::vector<std::complex<double>> &y, int len, int on) {
    change(y, len);
    for (int h = 2; h <= len; h <<= 1) {
        std::complex<double> wn = std::polar(1.0, on * 2 * PI / h);
        for (int j = 0; j < len; j += h) {
            std::complex<double> w(1, 0);
            for (int k = j; k < j + h / 2; ++k) {
                std::complex<double> u = y[k];
                std::complex<double> t = w * y[k + h / 2];
                y[k] = u + t;
                y[k + h / 2] = u - t;
                w = w * wn;
            }
        }
    }
    if (on == -1) {
        for (int i = 0; i < len; ++i) {
            y[i] = std::complex<double>(y[i].real() / len, y[i].imag() / len);
        }
    }
}

void int2048::trim() {
    while (a.size() > 1 && a.back() == 0) a.pop_back();
    if (a.size() == 1 && a[0] == 0) sign = false;
}

int2048::int2048() : sign(false) { a.push_back(0); }

int2048::int2048(long long v) {
    if (v < 0) { sign = true; v = -v; } else { sign = false; }
    if (v == 0) a.push_back(0);
    while (v > 0) { a.push_back(v % 10000); v /= 10000; }
}

int2048::int2048(const std::string &s) { read(s); }

int2048::int2048(const int2048 &o) : a(o.a), sign(o.sign) {}

void int2048::read(const std::string &s) {
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

void int2048::print() {
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

int int2048::cmp_abs(const int2048 &o) const {
    if (a.size() != o.a.size()) return a.size() < o.a.size() ? -1 : 1;
    for (int i = (int)a.size() - 1; i >= 0; --i) {
        if (a[i] != o.a[i]) return a[i] < o.a[i] ? -1 : 1;
    }
    return 0;
}

void int2048::add_abs(const int2048 &o) {
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

void int2048::sub_abs(const int2048 &o) {
    int borrow = 0;
    for (int i = 0; i < (int)a.size(); ++i) {
        long long diff = a[i] - borrow - (i < (int)o.a.size() ? o.a[i] : 0);
        if (diff < 0) { diff += 10000; borrow = 1; }
        else { borrow = 0; }
        a[i] = diff;
    }
    trim();
}

int2048 &int2048::add(const int2048 &B) {
    if (sign == B.sign) { add_abs(B); }
    else {
        if (cmp_abs(B) >= 0) { sub_abs(B); }
        else {
            int2048 t = B;
            t.sub_abs(*this);
            *this = t;
        }
    }
    return *this;
}

int2048 add(int2048 A, const int2048 &B) {
    A.add(B);
    return A;
}

int2048 &int2048::minus(const int2048 &B) {
    int2048 t = B;
    t.sign = !t.sign;
    if (t.a.size() == 1 && t.a[0] == 0) t.sign = false;
    add(t);
    return *this;
}

int2048 minus(int2048 A, const int2048 &B) {
    A.minus(B);
    return A;
}

int2048 int2048::operator+() const {
    return *this;
}

int2048 int2048::operator-() const {
    int2048 res = *this;
    res.sign = !res.sign;
    if (res.a.size() == 1 && res.a[0] == 0) res.sign = false;
    return res;
}

int2048 &int2048::operator=(const int2048 &o) {
    if (this == &o) return *this;
    a = o.a;
    sign = o.sign;
    return *this;
}

int2048 &int2048::operator+=(const int2048 &B) { return add(B); }
int2048 operator+(int2048 A, const int2048 &B) { return A.add(B); }

int2048 &int2048::operator-=(const int2048 &B) { return minus(B); }
int2048 operator-(int2048 A, const int2048 &B) { return A.minus(B); }

int2048 &int2048::operator*=(const int2048 &o) {
    if ((a.size() == 1 && a[0] == 0) || (o.a.size() == 1 && o.a[0] == 0)) {
        *this = int2048(0);
        return *this;
    }
    bool nsign = (sign != o.sign);
    if (a.size() < 50 && o.a.size() < 50) {
        std::vector<int> res_a(a.size() + o.a.size(), 0);
        for (int i = 0; i < (int)a.size(); ++i) {
            long long carry = 0;
            for (int j = 0; j < (int)o.a.size(); ++j) {
                long long cur = res_a[i+j] + a[i] * 1LL * o.a[j] + carry;
                res_a[i+j] = cur % 10000;
                carry = cur / 10000;
            }
            res_a[i + o.a.size()] += carry;
        }
        a = res_a;
    } else {
        int n = a.size();
        int m = o.a.size();
        int len = 1;
        while (len < (n + m) * 2) len <<= 1;
        std::vector<std::complex<double>> fa(len), fb(len);
        for (int i = 0; i < n; ++i) {
            fa[2*i] = std::complex<double>(a[i] % 100, 0);
            fa[2*i+1] = std::complex<double>(a[i] / 100, 0);
        }
        for (int i = 0; i < m; ++i) {
            fb[2*i] = std::complex<double>(o.a[i] % 100, 0);
            fb[2*i+1] = std::complex<double>(o.a[i] / 100, 0);
        }
        fft(fa, len, 1);
        fft(fb, len, 1);
        for (int i = 0; i < len; ++i) fa[i] *= fb[i];
        fft(fa, len, -1);

        a.assign(len / 2 + 1, 0);
        long long carry = 0;
        for (int i = 0; i < len; ++i) {
            long long v = (long long)(fa[i].real() + 0.5) + carry;
            long long rem = v % 100;
            carry = v / 100;
            if (i % 2 == 0) a[i / 2] = rem;
            else a[i / 2] += rem * 100;
        }
        while (carry > 0) {
            int i = len;
            long long rem = carry % 100;
            carry /= 100;
            if (i % 2 == 0) a.push_back(rem);
            else a.back() += rem * 100;
            len++;
        }
    }
    sign = nsign;
    trim();
    return *this;
}

int2048 operator*(int2048 A, const int2048 &B) {
    A *= B;
    return A;
}

void int2048::div_abs(const int2048 &A, const int2048 &B, int2048 &Q, int2048 &R) {
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

int2048 &int2048::operator/=(const int2048 &o) {
    int2048 abs_A = *this; abs_A.sign = false;
    int2048 abs_B = o; abs_B.sign = false;
    int2048 Q, R;
    div_abs(abs_A, abs_B, Q, R);
    
    if (sign == o.sign) {
        *this = Q;
        this->sign = false;
    } else {
        bool is_R_zero = (R.a.size() == 1 && R.a[0] == 0);
        if (is_R_zero) {
            *this = Q;
            this->sign = true;
        } else {
            *this = Q + int2048(1);
            this->sign = true;
        }
    }
    trim();
    return *this;
}

int2048 operator/(int2048 A, const int2048 &B) {
    A /= B;
    return A;
}

int2048 &int2048::operator%=(const int2048 &o) {
    int2048 Q = *this / o;
    *this = *this - Q * o;
    return *this;
}

int2048 operator%(int2048 A, const int2048 &B) {
    A %= B;
    return A;
}

std::istream &operator>>(std::istream &is, int2048 &b) {
    std::string s;
    is >> s;
    b.read(s);
    return is;
}

std::ostream &operator<<(std::ostream &os, const int2048 &b) {
    if (b.sign) os << '-';
    os << b.a.back();
    for (int i = (int)b.a.size() - 2; i >= 0; --i) {
        int v = b.a[i];
        if (v < 1000) os << '0';
        if (v < 100) os << '0';
        if (v < 10) os << '0';
        os << v;
    }
    return os;
}

bool operator==(const int2048 &A, const int2048 &B) {
    if (A.sign != B.sign) return false;
    if (A.a.size() != B.a.size()) return false;
    for (int i = 0; i < (int)A.a.size(); ++i) {
        if (A.a[i] != B.a[i]) return false;
    }
    return true;
}

bool operator!=(const int2048 &A, const int2048 &B) { return !(A == B); }

bool operator<(const int2048 &A, const int2048 &B) {
    if (A.sign != B.sign) return A.sign; // A negative -> A < B
    if (A.sign == false) {
        if (A.a.size() != B.a.size()) return A.a.size() < B.a.size();
        for (int i = (int)A.a.size() - 1; i >= 0; --i) {
            if (A.a[i] != B.a[i]) return A.a[i] < B.a[i];
        }
        return false;
    } else {
        if (A.a.size() != B.a.size()) return A.a.size() > B.a.size();
        for (int i = (int)A.a.size() - 1; i >= 0; --i) {
            if (A.a[i] != B.a[i]) return A.a[i] > B.a[i];
        }
        return false;
    }
}

bool operator>(const int2048 &A, const int2048 &B) { return B < A; }
bool operator<=(const int2048 &A, const int2048 &B) { return !(B < A); }
bool operator>=(const int2048 &A, const int2048 &B) { return !(A < B); }

} // namespace sjtu
