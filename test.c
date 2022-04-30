#include <stdio.h>

int total_count;
int local_count;
int failed_count;
char* test_class;

int ZERO, MINUS_ONE, ONE, INT_MAX, INT_MIN;

enum { A, B, C, D };
enum named { a = 3, b, c = 2, d = 1 };

void init() {
    // number
    ZERO = 0;
    MINUS_ONE = -1;
    ONE = 1;
    INT_MAX = 2147483647;
    INT_MIN = -2147483648;
}

void test(int expected, int actual) {
    if (actual != expected)
        failed_count++;
    printf("test %3d %s (test for %s %2d), ", ++total_count,
           (expected == actual ? "passed" : "failed"), test_class,
           local_count++);
    printf("expected: %d, actual: %d%s\n", expected, actual,
           expected != actual ? " ============> failed" : "");
}

void before(char* type) {
    test_class = type;
    local_count = 1;
}

#pragma GCC diagnostic ignored "-Woverflow"
void test_number() {
    int a, b, c;

    before((char*)"decimal number");
    a = 0;

    test(ZERO, a);
    test(ZERO, -a);

    b = 2147483647;
    c = 2147483648;

    test(INT_MAX, b);
    test(INT_MIN, c);

    b = -2147483648;
    c = -2147483649;

    test(INT_MIN, b);
    test(INT_MAX, c);

    b = 1;
    c = -1;

    test(ONE, b);
    test(MINUS_ONE, c);
    test(MINUS_ONE, -b);
    test(ONE, -c);

    a = 9;
    b = 10;
    c = 11;

    test(9, a);
    test(a + 1, b);
    test(a + 2, c);

    before((char*)"octal number");
    a = 00;

    test(ZERO, a);
    test(ZERO, -a);

    b = 017777777777;
    c = 020000000000;

    test(INT_MAX, b);
    test(INT_MIN, c);

    b = -020000000000;
    c = -020000000001;

    test(INT_MIN, b);
    test(INT_MAX, c);

    b = 01;
    c = -01;

    test(ONE, b);
    test(MINUS_ONE, c);
    test(MINUS_ONE, -b);
    test(ONE, -c);

    a = 07;
    b = 010;
    c = 011;

    test(7, a);
    test(a + 1, b);
    test(a + 2, c);

    before((char*)"hexadecimal number");
    a = 0x0;

    test(ZERO, a);
    test(ZERO, -a);

    b = 0x7fffffff;
    c = 0x80000000;

    test(INT_MAX, b);
    test(INT_MIN, c);

    b = -0x80000000;
    c = -0x80000001;

    test(INT_MIN, b);
    test(INT_MAX, c);

    b = 0x1;
    c = -0x1;

    test(ONE, b);
    test(MINUS_ONE, c);
    test(MINUS_ONE, -b);
    test(ONE, -c);

    a = 0xf;
    b = 0x10;
    c = 0x11;

    test(15, a);
    test(a + 1, b);
    test(a + 2, c);

    // check the capital support
    a = 0X0;

    test(ZERO, a);
    test(ZERO, -a);

    b = 0X7FFFFFFF;
    c = 0X80000000;

    test(INT_MAX, b);
    test(INT_MIN, c);

    b = -0X80000000;
    c = -0X80000001;

    test(INT_MIN, b);
    test(INT_MAX, c);

    b = 0X1;
    c = -0X1;

    test(ONE, b);
    test(MINUS_ONE, c);
    test(MINUS_ONE, -b);
    test(ONE, -c);

    a = 0Xf;
    b = 0X10;
    c = 0X11;

    test(15, a);
    test(a + 1, b);
    test(a + 2, c);
}

void test_enum() {
    before((char*)"enum");

    test(0, A);
    test(1, B);
    test(2, C);
    test(3, D);

    test(3, a);
    test(4, b);
    test(2, c);
    test(1, d);
}

void test_operator() {
    int a, b, c, d;

    before((char*)"operator =");
    a = 5;
    b = a;
    test(5, a);
    test(a, b);

    b = a = 6;
    test(6, a);
    test(a, b);

    before((char*)"operator ++ --");
    a = 1;
    test(1, a++);
    test(2, a);
    test(3, ++a);
    test(3, a);
    test(3, a--);
    test(2, a);
    test(1, --a);
    test(1, a);

    before((char*)"operator + - * / \%");
    a = 9;
    b = 4;

    test(13, a + b);
    test(5, a - b);
    test(36, a * b);
    test(2, a / b);
    test(1, a % b);

    before((char*)"operator < > <= >= != ==");
    a = b = 5;
    c = 10;

    test(1, a == b);
    test(0, a == c);
    test(0, a > b);
    test(0, a > c);
    test(1, a < c);
    test(0, a != b);
    test(1, a != c);
    test(1, a >= b);
    test(0, a >= c);
    test(1, a <= b);
    test(1, a <= c);
}

void test_expression() {
    before((char*)"expression");
}

void test_pointer() {
    before((char*)"pointer");
}

void test_recursive() {
    before((char*)"recursive");
}

void test_function() {
    before((char*)"function");
}

void test_control_flows() {
    before((char*)"control flows");
}

int main() {
    total_count = failed_count = 0;
    init();
    test_number();
    test_enum();
    test_operator();
    test_expression();
    test_pointer();
    test_recursive();
    test_function();
    test_control_flows();
    return 0;
}
