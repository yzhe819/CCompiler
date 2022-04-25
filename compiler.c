#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// related to source code
int token;            // current token
char *src, *old_src;  // pointer to source code
int poolsize;         // the default size of the data/text/stack
int line;             // line number

// related to virtual machine stacks and segments
int *text,      // text segment
    *old_text,  // for dump text segment
    *stack;     // stack
char* data;     // data segment

// related to virtual machine registers
// bp: base pointer
// sp: stack pointer
// pc: program counter, points to the next instruction
// ax: normal register, used for storing the calculated result
int *pc, *bp, *sp, ax, cycle;

// instructions, which is based on x86-64
// more details can be found on eval function
enum {
    LEA,
    IMM,
    JMP,
    CALL,
    JZ,
    JNZ,
    ENT,
    ADJ,
    LEV,
    LI,
    LC,
    SI,
    SC,
    PUSH,
    OR,
    XOR,
    AND,
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    SHL,
    SHR,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    OPEN,
    READ,
    CLOS,
    PRTF,
    MALC,
    MSET,
    MCMP,
    EXIT
};

// tokens and classes (operators last and in precedence order)
enum {
    Num = 128,
    Fun,
    Sys,
    Glo,
    Loc,
    Id,
    Char,
    Else,
    Enum,
    If,
    Int,
    Return,
    Sizeof,
    While,
    Assign,
    Cond,
    Lor,
    Lan,
    Or,
    Xor,
    And,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Shl,
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Inc,
    Dec,
    Brak
};

// lexical analysis will store all the scanned identifier into a symbol table,
// and use this table to check each new identifier
// ! the below this the structure of symbol table
// struct identifier {
//     int token;   // the return token, such as Id, if, while...
//     int hash;    // the hash value used for speed up the table search
//     char* name;  // the string of this identifier
//     int class;   // number, global or local variable
//     int type;    // type, int, char or pointer
//     int value;   // if is a function, it will store the address
//     int Bclass;  // temperamentally storing the class of global variable
//     int Btype;   // temperamentally storing the type of global variable
//     int Bvalue;  // temperamentally storing the value of global variable
// };

// because not support struct, we use array to set up symbol table
// Symbol table:
// ----+-----+----+----+----+-----+-----+-----+------+------+----
// .. |token|hash|name|type|class|value|btype|bclass|bvalue| ..
// ----+-----+----+----+----+-----+-----+-----+------+------+----
//     |<---       one single identifier                --->|

int token_val;    // value of current token (mainly for number)
int *current_id,  // current parsed ID
    *symbols;     // symbol table

// fields of identifier
enum { Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize };

// for lexical analysis, get the next token, it will automatically ignore
// whitespace characters
void next() {
    char* last_pos;
    int hash;

    while (token = *src) {
        ++src;
        // parse token here
        if (token == '\n') {
            // new line
            ++line;
        } else if (token == '#') {
            // skip macro, eg: # include <stdio.h>
            while (*src != 0 && *src != '\n') {
                src++;
            }
        } else if ((*src >= 'a' && *src <= 'z') ||
                   (*src >= 'A' && *src <= 'Z') || (*src == '_')) {
            // parse identifier
            last_pos = src - 1;
            // init the hash value
            hash = token;

            while ((*src >= 'a' && *src <= 'z') ||
                   (*src >= 'A' && *src <= 'Z') ||
                   (*src >= '0' && *src <= '9') || (*src == '_')) {
                // calculate the hash by each char
                //  147 is from c4
                hash = hash * 147 + *src;
                src++;
            }

            // looking for existing identifier from symbol table
            current_id = symbols;
            // this while loop is used to check all the existing token
            while (current_id[token]) {
                // check the hash value and each char
                if (current_id[Hash] == hash &&
                    !memcmp((char*)current_id[Name], last_pos,
                            src - last_pos)) {
                    // found one, return
                    token = current_id[Token];
                    return;
                }
                // move to next identifier
                current_id = current_id + IdSize;
            }

            // store this new identifier
            current_id[Name] = (int)last_pos;  // store the start pointer
            current_id[Hash] = hash;           // store the hash value
            token = current_id[Token] = Id;    // this is an identifier

            return;
        } else if (*src >= '0' && *src <= '9') {
            // parse number, three kinds: dec(123) hex(0x123) oct(017)
            token_val = token - '0';
            if (token_val > 0) {
                // des, start with [1-9]
                while (*src >= '0' && *src <= '9') {
                    token_val = token_val * 10 + *src++ - '0';
                }
            } else {
                // start with number 0
                if (*src == 'x' || *src == 'X') {
                    // hex
                    token = *++src;
                    while ((token >= '0' && token <= '9') ||
                           (token >= 'a' && token <= 'f') ||
                           (token >= 'A' && token <= 'F')) {
                        token_val = token_val * 16 + (token & 15) +
                                    (token >= 'A' ? 9 : 0);
                        token = *++src;
                    }
                } else {
                    // oct
                    while (*src >= '0' && *src <= '7') {
                        token_val = token_val * 8 + *src++ - '0';
                    }
                }
            }

            token = Num;
            return;
        } else if (token == '"' || token == '\'') {
            // parse string and char
            last_pos = data;  // the address of data segment
            // 0 means the EOL
            while (*src != 0 && *src != token) {
                token_val = *src++;
                if (token_val == '\\') {
                    // escape character
                    // only support \n now
                    token_val = *src++;
                    if (token_val == 'n') {
                        token_val = '\n';
                    }
                }

                // store this string into data segment
                if (token == '"') {
                    *data++ = token_val;
                }
            }

            src++;
            // for single character, return num token
            if (token == '"') {
                token_val = (int)last_pos;
            } else {
                token = Num;
            }
            return;
        } else if (token == '/') {
            if (*src == '/') {
                // only support "//" type comments
                // skip comment
                while (*src != 0 && *src != '\n') {
                    ++src;
                }
            } else {
                // divide operator
                token = Div;
                return;
            }
        } else if (token == '=') {
            // parse '==' and '='
            if (*src == '=') {
                src++;
                token = Eq;
            } else {
                token = Assign;
            }
            return;
        } else if (token == '+') {
            // parse '+' and '++'
            if (*src == '+') {
                src++;
                token = Inc;
            } else {
                token = Add;
            }
            return;
        } else if (token == '-') {
            // parse '-' and '--'
            if (*src == '-') {
                src++;
                token = Dec;
            } else {
                token = Sub;
            }
            return;
        } else if (token == '!') {
            // parse '!='
            if (*src == '=') {
                src++;
                token = Ne;
            }
            return;
        } else if (token == '<') {
            // parse '<=', '<<' or '<'
            if (*src == '=') {
                src++;
                token = Le;
            } else if (*src == '<') {
                src++;
                token = Shl;
            } else {
                token = Lt;
            }
            return;
        } else if (token == '>') {
            // parse '>=', '>>' or '>'
            if (*src == '=') {
                src++;
                token = Ge;
            } else if (*src == '>') {
                src++;
                token = Shr;
            } else {
                token = Gt;
            }
            return;
        } else if (token == '|') {
            // parse '|' or '||'
            if (*src == '|') {
                src++;
                token = Lor;
            } else {
                token = Or;
            }
            return;
        } else if (token == '&') {
            // parse '&' and '&&'
            if (*src == '&') {
                src++;
                token = Lan;
            } else {
                token = And;
            }
            return;
        } else if (token == '^') {
            token = Xor;
            return;
        } else if (token == '%') {
            token = Mod;
            return;
        } else if (token == '*') {
            token = Mul;
            return;
        } else if (token == '[') {
            token = Brak;
            return;
        } else if (token == '?') {
            token = Cond;
            return;
        } else if (token == '~' || token == ';' || token == '{' ||
                   token == '}' || token == '(' || token == ')' ||
                   token == ']' || token == ',' || token == ':') {
            // directly return the character as token;
            return;
        }
    }
    return;
}

// for parsing an expression
void expression(int level) {
    // do nothing
}

// for syntax analysis entry, analyze the entire C language program
void program() {
    next();  // get next token
    while (token > 0) {
        printf("token is: %3d('%c')\n", token, token);
        next();
    }
}

// the entry point of the virtual machine, used to interpret the object code
int eval() {
    int op, *tmp;
    while (1) {
        op = *pc++;  // get next operation code

        if (op == IMM) {
            ax = *pc++;  // load immediate value to ax
        } else if (op == LC) {
            ax = *(char*)ax;  // load character to ax, address in ax
        } else if (op == LI) {
            ax = *(int*)ax;  // load int to ax, address in ax
        } else if (op == SC) {
            // save character to address, value in ax, address on stack
            // sp++ is equal to stack pop
            ax = *(char*)* sp++ = ax;
        } else if (op == SI) {
            // save integer to address, value in ax, address on stack
            ax = *(int*)sp++ = ax;
        } else if (op == PUSH) {
            *--sp = ax;  // push the current value into the stack
        } else if (op == JMP) {
            // pc is used to store the position of next instruction
            // jump to the next instruction
            pc = (int*)*pc;
        } else if (op == JZ) {
            // jump if ax is equal to zero
            pc = ax ? pc + 1 : (int*)*pc;
        } else if (op == JNZ) {
            // jump if ax is not equal to zero
            pc = ax ? (int*)*pc : pc + 1;
        } else if (op == CALL) {
            *--sp = (int)(pc + 1);  // store following address into stack
            pc = (int*)*pc;         // call subroutine to function address
        }
        // return from subroutine, replaced by LEV
        // else if (op == REF) {
        //     pc = (int*)*sp++;
        // }
        else if (op == ENT) {
            // make new call frame
            *--sp = (int)bp;  // store the current base pointer
            bp = sp;          // base pointer will be the current stack pointer
            sp = sp - *pc++;  // set some place for local variable
        } else if (op == ADJ) {
            // remove argument from frame
            sp = sp + *sp++;
        } else if (op == LEV) {
            // restore call frame and PC
            // no need additional REF instruction
            sp = bp;           // reset the sp
            bp = (int*)*sp++;  // recover the bp from stack
            pc = (int*)*sp++;  // ?? where is the pc info come from
        } else if (op == LEA) {
            // load address for the arguments
            ax = (int)(bp + *pc++);
        }
        // operator instruction set, from c4
        // The first parameter is placed at the top of the stack, and the second
        // parameter is placed in ax
        else if (op == OR) {
            ax = *sp++ | ax;
        } else if (op == XOR) {
            ax = *sp++ ^ ax;
        } else if (op == AND) {
            ax = *sp++ & ax;
        } else if (op == EQ) {
            ax = *sp++ == ax;
        } else if (op == NE) {
            ax = *sp++ != ax;
        } else if (op == LT) {
            ax = *sp++ < ax;
        } else if (op == LE) {
            ax = *sp++ <= ax;
        } else if (op == GT) {
            ax = *sp++ > ax;
        } else if (op == GE) {
            ax = *sp++ >= ax;
        } else if (op == SHL) {
            ax = *sp++ << ax;
        } else if (op == SHR) {
            ax = *sp++ >> ax;
        } else if (op == ADD) {
            ax = *sp++ + ax;
        } else if (op == SUB) {
            ax = *sp++ - ax;
        } else if (op == MUL) {
            ax = *sp++ * ax;
        } else if (op == DIV) {
            ax = *sp++ / ax;
        } else if (op == MOD) {
            ax = *sp++ % ax;
        }
        // some build in function
        else if (op == EXIT) {
            printf("exit(%d)\n", *sp);
            return *sp;
        } else if (op == OPEN) {
            ax = open((char*)sp[1], sp[0]);
        } else if (op == CLOS) {
            ax = close(*sp);
        } else if (op == READ) {
            ax = read(sp[2], (char*)sp[1], *sp);
        } else if (op == PRTF) {
            tmp = sp + pc[1];
            ax = printf((char*)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5],
                        tmp[-6]);
        } else if (op == MALC) {
            ax = (int)malloc(*sp);
        } else if (op == MSET) {
            ax = (int)memset((char*)sp[2], sp[1], *sp);
        } else if (op == MCMP) {
            ax = memcmp((char*)sp[2], (char*)sp[1], *sp);
        } else {
            printf("unknown instruction:%d\n", op);
            return -1;
        }
    }
    return 0;
}

// types of variable/function
enum { CHAR, INT, PTR };
int* idmain;  // the `main` function

// the main function
int main(int argc, char* argv[]) {
    int i, fd;
    argc--;
    argv++;

    poolsize = 256 * 1024;
    line = 1;

    // allocate memory for initializing the virtual machine
    if (!(text = old_text = malloc(poolsize))) {
        printf("could not malloc(%d) for text segment area\n", poolsize);
        return -1;
    }
    if (!(data = malloc(poolsize))) {
        printf("could not malloc(%d) for data segment area\n", poolsize);
        return -1;
    }
    if (!(stack = malloc(poolsize))) {
        printf("could not malloc(%d) for stack segment area\n", poolsize);
        return -1;
    }
    if (!(symbols = malloc(poolsize))) {
        printf("could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }

    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    memset(symbols, 0, poolsize);

    // initialize the registers
    // the stack starts from high address to low address
    // bp and sp will start from the highest address
    bp = sp = (int*)((int)stack + poolsize);
    ax = 0;

    // init the keyword in symbol table
    src =
        "char else enum if int return sizeof while "
        "open read close printf malloc memset memcmp exit void main";

    // add keywords to symbol table
    i = Char;
    while (i <= While) {
        next();
        current_id[Token] = i++;
    }

    // add library to symbol table
    i = OPEN;
    while (i <= EXIT) {
        next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }

    next();
    current_id[Token] = Char;  // handle void type
    next();
    idmain = current_id;  // keep track of main

    // open the source file
    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    // init the segment and stack space
    if (!(src = old_src = malloc(poolsize))) {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }

    // read the source file
    if ((i = read(fd, src, poolsize - 1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0;  // add EOF character
    close(fd);

    program();

    return eval();
}