#include <stdio.h>

int total_count;
int local_count;
int failed_count;
char* test_class;

int ZERO, MINUS_ONE, ONE, INT_MAX, INT_MIN;
int TRUE, FALSE;

enum { A, B, C, D };
enum named { a = 3, b, c = 2, d = 1 };

void init() {
    // number
    ZERO = 0;
    MINUS_ONE = -1;
    ONE = 1;
    INT_MAX = 2147483647;
    INT_MIN = -2147483648;

    // boolean
    TRUE = 1;
    FALSE = 0;
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

void assert(char* type) {
    test_class = type;
    local_count = 1;
}

void analyze() {
    printf("+----+-------+-------+-------+-------+-------+-------+----+\n");
    if (failed_count) {
        printf("Tests run: %3d,  Failures: %3d\n", total_count, failed_count);
    } else {
        printf("OK (%3d tests)\n", total_count);
    }
    printf("+----+-------+-------+-------+-------+-------+-------+----+\n");
}

#pragma GCC diagnostic ignored "-Woverflow"
void test_char() {
    assert((char*)"char");
    // todo
}

void test_number() {
    int a, b, c;

    assert((char*)"decimal number");
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

    assert((char*)"octal number");
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

    assert((char*)"hexadecimal number");
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
    assert((char*)"enum");

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

    assert((char*)"operator =");
    a = 5;
    b = a;
    test(5, a);
    test(a, b);

    b = a = 6;
    test(6, a);
    test(a, b);

    assert((char*)"operator ++ --");
    a = 1;
    test(1, a++);
    test(2, a);
    test(3, ++a);
    test(3, a);
    test(3, a--);
    test(2, a);
    test(1, --a);
    test(1, a);

    assert((char*)"operator + - * / \%");
    a = 9;
    b = 4;

    test(13, a + b);
    test(5, a - b);
    test(36, a * b);
    test(2, a / b);
    test(1, a % b);

    assert((char*)"operator |");

    assert((char*)"operator ^");

    assert((char*)"operator &");

    assert((char*)"operator !");

    assert((char*)"operator < > <= >= != ==");
    a = b = 5;
    c = 10;

    test(TRUE, a == b);
    test(FALSE, a == c);
    test(FALSE, a > b);
    test(FALSE, a > c);
    test(TRUE, a < c);
    test(FALSE, a != b);
    test(TRUE, a != c);
    test(TRUE, a >= b);
    test(FALSE, a >= c);
    test(TRUE, a <= b);
    test(TRUE, a <= c);

    assert((char*)"operator ? :");
    a = 2;
    b = 3;

    test(2, TRUE ? a : b);
    test(3, FALSE ? a : b);

    assert((char*)"operator || &&");
    a = 0;
    b = 1;
    c = -1;
    test(1, a || b);
    test(0, a || a);
    test(1, b || c);
    test(1, c || c);  // failed

    test(FALSE, a && b);
    test(FALSE, a && a);
    test(TRUE, b && c);  // failed
    test(TRUE, c && c);  // failed

    assert((char*)"operator << >>");

    assert((char*)"operator & [] *");
}

void test_pointer() {
    int i, *p, **pp, ***ppp;

    assert((char*)"pointer");
    i = 0;
    p = &i;
    pp = &p;
    ppp = &pp;

    test((int)&i, (int)p);
    test((int)&p, (int)pp);
    test((int)&pp, (int)ppp);

    test(i, (int)*p);
    test((int)p, (int)*pp);
    test((int)pp, (int)*ppp);
    test(i, (int)***ppp);

    test(i, (int)*&*&*p);
}

void test_expression() {
    assert((char*)"expression");
}

void test_control_flows() {
    int a, b;
    assert((char*)"while loop");
    a = 0;
    b = 1;
    while (a++ < 20) {
        test(b, a);
        b = b + 1;
    }
    test(21, a);

    assert((char*)"if else");
    a = 0;
    b = 1;
    if (a) {
        test(TRUE, a);
    } else {
        test(FALSE, a);
    }

    if (b) {
        test(TRUE, b);
    } else {
        test(FALSE, b);
    }
}

int func1(int x) {
    return x * x;
}

void func2(int a, int b, int c) {
    test(2 * 3 * 4, a * b * c);
}

void func3() {
    printf("Hello world\n");
}

void test_function() {
    int a;
    assert((char*)"function");
    a = func1(3);
    func2(2, 3, 4);
    func3();
}

int factorial(int i) {
    if (i < 2) {
        return i;
    } else {
        return i * factorial(i - 1);
    }
}

void test_recursive() {
    assert((char*)"recursive");
    test(3628800, factorial(10));
}

int main() {
    total_count = failed_count = 0;
    init();
    test_char();
    test_number();
    test_enum();
    test_operator();
    test_pointer();
    test_expression();
    test_control_flows();
    test_function();
    test_recursive();
    analyze();
    return 0;
}
