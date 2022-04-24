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

/**
 * @brief for lexical analysis, get the next token, it will automatically ignore
 * whitespace characters
 */
void next() {
    token = *src++;
    return;
}

/**
 * @brief for parsing an expression
 * @param level
 */
void expression(int level) {
    next();  // get next token
    while (token > 0) {
        printf("token is %c\n", token);
        next();
    }
}

/**
 * @brief for syntax analysis entry, analyze the entire C language program
 */
void program() {
    next();  // get next token
    while (token > 0) {
        printf("token is: %c\n", token);
        next();
    }
}

/**
 * @brief the entry point of the virtual machine,
 * used to interpret the object code
 */
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
        /**
         * return from subroutine
         * else if (op == REF) {
         *     pc = (int*)*sp++;
         * }
         */
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

/**
 * @brief the main function
 */
int main(int argc, char* argv[]) {
    int i, fd;
    argc--;
    argv++;

    poolsize = 256 * 1024;

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

    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);

    // initialize the registers
    // the stack starts from high address to low address
    // bp and sp will start from the highest address
    bp = sp = (int*)((int)stack + poolsize);
    ax = 0;

    // ! only for testing
    i = 0;
    text[i++] = IMM;
    text[i++] = 10;
    text[i++] = PUSH;
    text[i++] = IMM;
    text[i++] = 20;
    text[i++] = ADD;
    text[i++] = PUSH;
    text[i++] = EXIT;

    pc = text;
    // ! only for testing

    program();
    return eval();
}