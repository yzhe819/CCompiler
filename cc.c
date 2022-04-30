#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// related to source code
int token;            // current token
int token_val;        // value of current token (mainly for number)
char *src, *old_src;  // pointer to source code
int poolsize;         // the default size of the data/text/stack
int line;             // line number

// related to virtual machine stacks and segments
int *text,      // text segment
    *old_text,  // for dump text segment
    *stack;     // stack
char* data;     // data segment

// related to main function and debug
int* idmain;  // the `main` function
int debug;    // active debug model

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

int *current_id,  // current parsed ID
    *symbols;     // symbol table

// fields of identifier
enum { Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize };

// types of variable/function
enum { CHAR, INT, PTR };

int basetype;   // the type of a declaration, make it global for convenience
int expr_type;  // the type of an expression

int index_of_bp;  // index of bp pointer on stack

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
        } else if ((token >= 'a' && token <= 'z') ||
                   (token >= 'A' && token <= 'Z') || (token == '_')) {
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
            while (current_id[Token]) {
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
        } else if (token >= '0' && token <= '9') {
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

// used for check and automatically go next
void match(int tk) {
    if (token == tk) {
        next();
    } else {
        printf("%d: expected token: %d\n", line, tk);
        exit(-1);
    }
}

// for parsing an expression
// todo need to double check
void expression(int level) {
    int* id;
    int tmp;
    int* addr;

    if (token == Num) {
        match(Num);
        // emit code
        *++text = IMM;
        *++text = token_val;
        expr_type = INT;
    } else if (token == '"') {
        *++text = IMM;
        *++text = token_val;

        match('"');  // start to store the string
        // use the following while loop to support multi line string
        while (token == '"') {  // store the rest string
            match('"');
        }

        // append the end of string character '\0', all the data are default
        // to 0, so just move data one position forward.
        data = (char*)(((int)data + sizeof(int)) & (-sizeof(int)));
        expr_type = PTR;
    } else if (token == Sizeof) {
        // sizeof is actually an unary operator
        // now only `sizeof(int)`, `sizeof(char)` and `sizeof(*...)` are
        // supported.
        match(Sizeof);
        match('(');
        expr_type = INT;

        if (token == Int) {
            match(Int);
        } else if (token == Char) {
            match(Char);
            expr_type = Char;
        }

        while (token == Mul) {
            match(Mul);
            expr_type = expr_type + PTR;
        }

        match(')');

        // emit code
        *++text = IMM;
        *++text = (expr_type == Char) ? sizeof(char) : sizeof(int);

        expr_type = INT;
    } else if (token == Id) {
        // there are several type when occurs to Id
        // but this is unit, so it can only be
        // 1. function call
        // 2. Enum variable
        // 3. global/local variable
        match(Id);

        id = current_id;

        if (token == '(') {
            // function call
            match('(');

            // pass the arguments
            tmp = 0;
            while (token != ')') {
                expression(Assign);
                *++text = PUSH;
                tmp++;

                if (token == ',') {
                    match(',');
                }
            }

            match(')');

            // emit code
            if (id[Class] == Sys) {
                // system function
                *++text = id[Value];
            } else if (id[Class] == Fun) {
                // function call
                *++text = CALL;
                *++text = id[Value];
            } else {
                printf("%d: bad function call\n", line);
                exit(-1);
            }

            // clean the stack for arguments
            if (tmp > 0) {
                *++text = ADJ;
                *++text = tmp;
            }
            expr_type = id[Type];
        } else if (id[Class] == Num) {
            // enum variable
            *++text = IMM;
            *++text = id[Value];
            expr_type = INT;
        } else {
            // variable
            if (id[Class] == Loc) {
                *++text = LEA;
                *++text = index_of_bp - id[Value];
            } else if (id[Class] == Glo) {
                *++text = IMM;
                *++text = id[Value];
            } else {
                printf("%d: undefined variable\n", line);
                exit(-1);
            }

            //⑥
            // emit code, default behaviour is to load the value of the
            // address which is stored in `ax`
            expr_type = id[Type];
            *++text = (expr_type == Char) ? LC : LI;
        }
    }
    // ! follow the tutorial
    else if (token == '(') {
        // cast or parenthesis
        match('(');
        if (token == Int || token == Char) {
            tmp = (token == Char) ? CHAR : INT;  // cast type
            match(token);
            while (token == Mul) {
                match(Mul);
                tmp = tmp + PTR;
            }

            match(')');

            expression(Inc);  // cast has precedence as Inc(++)

            expr_type = tmp;
        } else {
            // normal parenthesis
            expression(Assign);
            match(')');
        }
    } else if (token == Mul) {
        // dereference *<addr>
        match(Mul);
        expression(Inc);  // dereference has the same precedence as Inc(++)

        if (expr_type >= PTR) {
            expr_type = expr_type - PTR;
        } else {
            printf("%d: bad dereference\n", line);
            exit(-1);
        }

        *++text = (expr_type == CHAR) ? LC : LI;
    } else if (token == And) {
        // get the address of
        match(And);
        expression(Inc);  // get the address of
        if (*text == LC || *text == LI) {
            text--;
        } else {
            printf("%d: bad address of\n", line);
            exit(-1);
        }

        expr_type = expr_type + PTR;
    } else if (token == '!') {
        // not
        match('!');
        expression(Inc);

        // emit code, use <expr> == 0
        *++text = PUSH;
        *++text = IMM;
        *++text = 0;
        *++text = EQ;

        expr_type = INT;
    } else if (token == '~') {
        // bitwise not
        match('~');
        expression(Inc);

        // emit code, use <expr> XOR -1
        *++text = PUSH;
        *++text = IMM;
        *++text = -1;
        *++text = XOR;

        expr_type = INT;
    } else if (token == Add) {
        // +var, do nothing
        match(Add);
        expression(Inc);

        expr_type = INT;
    } else if (token == Sub) {
        // -var
        match(Sub);

        if (token == Num) {
            *++text = IMM;
            *++text = -token_val;
            match(Num);
        } else {
            *++text = IMM;
            *++text = -1;
            *++text = PUSH;
            expression(Inc);
            *++text = MUL;
        }

        expr_type = INT;
    } else if (token == Inc || token == Dec) {
        tmp = token;
        match(token);
        expression(Inc);
        // ①
        if (*text == LC) {
            *text = PUSH;  // to duplicate the address
            *++text = LC;
        } else if (*text == LI) {
            *text = PUSH;
            *++text = LI;
        } else {
            printf("%d: bad lvalue of pre-increment\n", line);
            exit(-1);
        }
        *++text = PUSH;
        *++text = IMM;
        // ②
        *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
        *++text = (tmp == Inc) ? ADD : SUB;
        *++text = (expr_type == CHAR) ? SC : SI;
    } else {
        printf("%d: bad expression\n", line);
        exit(-1);
    }

    // ! follow the tutorial
    // binary operator and postfix operators.
    {
        while (token >= level) {
            // handle according to current operator's precedence
            tmp = expr_type;
            if (token == Assign) {
                // var = expr;
                match(Assign);
                if (*text == LC || *text == LI) {
                    *text = PUSH;  // save the lvalue's pointer
                } else {
                    printf("%d: bad lvalue in assignment\n", line);
                    exit(-1);
                }
                expression(Assign);

                expr_type = tmp;
                *++text = (expr_type == CHAR) ? SC : SI;
            } else if (token == Cond) {
                // expr ? a : b;
                match(Cond);
                *++text = JZ;
                addr = ++text;
                expression(Assign);
                if (token == ':') {
                    match(':');
                } else {
                    printf("%d: missing colon in conditional\n", line);
                    exit(-1);
                }
                *addr = (int)(text + 3);
                *++text = JMP;
                addr = ++text;
                expression(Cond);
                *addr = (int)(text + 1);
            } else if (token == Lor) {
                // logic or
                match(Lor);
                *++text = JNZ;
                addr = ++text;
                expression(Lan);
                *addr = (int)(text + 1);
                expr_type = INT;
            } else if (token == Lan) {
                // logic and
                match(Lan);
                *++text = JZ;
                addr = ++text;
                expression(Or);
                *addr = (int)(text + 1);
                expr_type = INT;
            } else if (token == Or) {
                // bitwise or
                match(Or);
                *++text = PUSH;
                expression(Xor);
                *++text = OR;
                expr_type = INT;
            } else if (token == Xor) {
                // bitwise xor
                match(Xor);
                *++text = PUSH;
                expression(And);
                *++text = XOR;
                expr_type = INT;
            } else if (token == And) {
                // bitwise and
                match(And);
                *++text = PUSH;
                expression(Eq);
                *++text = AND;
                expr_type = INT;
            } else if (token == Eq) {
                // equal ==
                match(Eq);
                *++text = PUSH;
                expression(Ne);
                *++text = EQ;
                expr_type = INT;
            } else if (token == Ne) {
                // not equal !=
                match(Ne);
                *++text = PUSH;
                expression(Lt);
                *++text = NE;
                expr_type = INT;
            } else if (token == Lt) {
                // less than
                match(Lt);
                *++text = PUSH;
                expression(Shl);
                *++text = LT;
                expr_type = INT;
            } else if (token == Gt) {
                // greater than
                match(Gt);
                *++text = PUSH;
                expression(Shl);
                *++text = GT;
                expr_type = INT;
            } else if (token == Le) {
                // less than or equal to
                match(Le);
                *++text = PUSH;
                expression(Shl);
                *++text = LE;
                expr_type = INT;
            } else if (token == Ge) {
                // greater than or equal to
                match(Ge);
                *++text = PUSH;
                expression(Shl);
                *++text = GE;
                expr_type = INT;
            } else if (token == Shl) {
                // shift left
                match(Shl);
                *++text = PUSH;
                expression(Add);
                *++text = SHL;
                expr_type = INT;
            } else if (token == Shr) {
                // shift right
                match(Shr);
                *++text = PUSH;
                expression(Add);
                *++text = SHR;
                expr_type = INT;
            } else if (token == Add) {
                // add
                match(Add);
                *++text = PUSH;
                expression(Mul);

                expr_type = tmp;
                if (expr_type > PTR) {
                    // pointer type, and not `char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                *++text = ADD;
            } else if (token == Sub) {
                // sub
                match(Sub);
                *++text = PUSH;
                expression(Mul);
                if (tmp > PTR && tmp == expr_type) {
                    // pointer subtraction
                    *++text = SUB;
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = DIV;
                    expr_type = INT;
                } else if (tmp > PTR) {
                    // pointer movement
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                    *++text = SUB;
                    expr_type = tmp;
                } else {
                    // numeral subtraction
                    *++text = SUB;
                    expr_type = tmp;
                }
            } else if (token == Mul) {
                // multiply
                match(Mul);
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
                expr_type = tmp;
            } else if (token == Div) {
                // divide
                match(Div);
                *++text = PUSH;
                expression(Inc);
                *++text = DIV;
                expr_type = tmp;
            } else if (token == Mod) {
                // Modulo
                match(Mod);
                *++text = PUSH;
                expression(Inc);
                *++text = MOD;
                expr_type = tmp;
            } else if (token == Inc || token == Dec) {
                // postfix inc(++) and dec(--)
                // we will increase the value to the variable and decrease it
                // on `ax` to get its original value.
                if (*text == LI) {
                    *text = PUSH;
                    *++text = LI;
                } else if (*text == LC) {
                    *text = PUSH;
                    *++text = LC;
                } else {
                    printf("%d: bad value in increment\n", line);
                    exit(-1);
                }

                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? ADD : SUB;
                *++text = (expr_type == CHAR) ? SC : SI;
                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? SUB : ADD;
                match(token);
            } else if (token == Brak) {
                // array access var[xx]
                match(Brak);
                *++text = PUSH;
                expression(Assign);
                match(']');

                if (tmp > PTR) {
                    // pointer, `not char *`
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                } else if (tmp < PTR) {
                    printf("%d: pointer type expected\n", line);
                    exit(-1);
                }
                expr_type = tmp - PTR;
                *++text = ADD;
                *++text = (expr_type == CHAR) ? LC : LI;
            } else {
                printf("%d: compiler error, token = %d\n", line, token);
                exit(-1);
            }
        }
    }
}

void function_parameter() {
    // parameter_decl ::= type {'*'} id {',' type {'*'} id}
    int type;
    int params;
    params = 0;

    while (token != ')') {
        type = INT;  // init type
        if (token == Int) {
            match(Int);
        } else if (token == Char) {
            type = CHAR;
            match(Char);
        }

        // pointer type
        while (token == Mul) {
            match(Mul);
            type = type + PTR;
        }

        // parameter name
        if (token != Id) {
            printf("%d: bad parameter declaration\n", line);
            exit(-1);
        }
        // declarated the local variable
        if (current_id[Class] == Loc) {
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }

        match(Id);

        // store the local variable
        current_id[BClass] = current_id[Class];
        current_id[Class] = Loc;
        current_id[BType] = current_id[Type];
        current_id[Type] = type;
        current_id[BValue] = current_id[Value];
        current_id[Value] = params++;  // index of current parameter

        if (token == ',') {
            match(',');
        }
    }

    // ??
    index_of_bp = params + 1;
}

// used to handle statement
void statement() {
    // only have following six kinds of statement for us
    // 1. if (...) <statement> [else <statement>]
    // 2. while (...) <statement>
    // 3. { <statement> }
    // 4. return xxx;
    // 5. <empty statement>;
    // 6. expression; (expression end with semicolon)

    int *a, *b;  // bess for branch control

    if (token == If) {
        // if (...) <statement> [else <statement>]
        match(If);
        match('(');
        expression(Assign);  // parse condition
        match(')');

        *++text = JZ;
        b = ++text;

        statement();  // parse statement
        if (token == Else) {
            match(Else);
            // jmp b
            *b = (int)(text + 3);
            *++text = JMP;
            b = ++text;

            statement();
        }

        *b = (int)(text + 1);
    } else if (token == While) {
        // while (...) <statement>
        match(While);
        a = text + 1;
        match('(');
        expression(Assign);
        match(')');

        *++text = JZ;
        b = ++text;

        statement();

        *++text = JMP;
        *++text = (int)a;
        *b = (int)(text + 1);
    } else if (token == Return) {
        // return xxx;
        match(Return);

        if (token != ';') {
            expression(Assign);
        }

        match(';');

        // emit code for return
        *++text = LEV;
    } else if (token == '{') {
        // { <statement> ... }
        match('{');

        while (token != '}') {
            statement();
        }

        match('}');
    } else if (token == ';') {
        // empty statement
        match(';');
    } else {
        // a = b; or function_call();
        expression(Assign);
        match(';');
    }
}

void function_body() {
    // body_decl ::= {variable_decl}, {statement}
    // type func_name (...) {...}
    //                   -->|   |<--

    // ... {
    // 1. local declarations
    // 2. statements
    // }

    int pos_local;  // position of local variables on the stack.
    int type;
    pos_local = index_of_bp;  // new base pointer

    while (token == Int || token == Char) {
        // local variable declaration
        basetype = (token == Int) ? INT : CHAR;
        match(token);

        while (token != ';') {
            type = basetype;
            while (token == Mul) {
                match(Mul);
                type = type + PTR;
            }

            if (token != Id) {
                // invalid declaration
                printf("%d: bad local declaration\n", line);
                exit(-1);
            }
            if (current_id[Class] == Loc) {
                // identifier exists
                printf("%d: duplicate local declaration\n", line);
                exit(-1);
            }
            match(Id);

            // store the local variable
            current_id[BClass] = current_id[Class];
            current_id[Class] = Loc;
            current_id[BType] = current_id[Type];
            current_id[Type] = type;
            current_id[BValue] = current_id[Value];
            current_id[Value] = ++pos_local;  // index of current parameter

            if (token == ',') {
                match(',');
            }
        }
        match(';');
    }

    // rext is the memory address of function, which is in
    // code segment (also used in global declaration)

    *++text = ENT;                      // start the function body
    *++text = pos_local - index_of_bp;  // local variable size

    // statements
    while (token != '}') {
        statement();
    }

    // emit code for leaving the sub function
    *++text = LEV;
}

void function_declaration() {
    // function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'
    // type func_name (...) {...}
    //               | this part

    match('(');
    function_parameter();
    match(')');

    match('{');
    function_body();
    // not match the final }, while leave the while loop in global declartion to
    // handle it
    // match('}');

    // unwind local variable declarations for all local variables.
    current_id = symbols;
    // need to do this operation for all the global value
    while (current_id[Token]) {
        if (current_id[Class] == Loc) {
            // recover the global value
            current_id[Class] = current_id[BClass];
            current_id[Type] = current_id[BType];
            current_id[Value] = current_id[BValue];
        }
        current_id = current_id + IdSize;
    }
}

void enum_declaration() {
    // parse enum [id] { a = 1, b = 3, ...}
    int i;
    i = 0;
    while (token != '}') {
        if (token != Id) {
            printf("%d: bad enum identifier %d\n", line, token);
            exit(-1);
        }
        next();
        if (token == Assign) {
            // like {a=10}
            next();
            if (token != Num) {
                printf("%d: bad enum initializer\n", line);
                exit(-1);
            }
            i = token_val;
            next();
        }

        current_id[Class] = Num;
        current_id[Type] = INT;
        current_id[Value] = i++;

        if (token == ',') {
            next();
        }
    }
}

void global_declaration() {
    // EBNF
    // global_declaration ::= enum_decl | variable_decl | function_decl
    // enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'} '}'
    // variable_decl ::= type {'*'} id { ',' {'*'} id } ';'
    // function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'

    int type;  // temp, actual type for variable
    int i;     // temp

    basetype = INT;

    if (token == Enum) {
        // parse enum
        // enum [id] { a = 10, b = 20, ... }
        match(Enum);
        if (token != '{') {
            match(Id);  // skip the [id] part
        }

        if (token == '{') {
            match('{');
            enum_declaration();
            match('}');
        }

        match(';');
        return;
    }

    if (token == Int) {
        // parse number information
        match(Int);
    } else if (token == Char) {
        match(Char);
        basetype = CHAR;
    }

    // parse the common seperated variable declaration
    while (token != ';' && token != '}') {
        type = basetype;
        // parse pointer type, note that there may exist `int ****x;`
        while (token == Mul) {
            match(Mul);
            // identify the pointer of int or char
            type = type + PTR;
        }

        if (token != Id) {
            // invalid
            printf("%d: bad global declaration\n", line);
            exit(-1);
        }

        // this current_id will be changed by next function
        // it will automatically move to next identifier space or the existing
        // identifier in symbol table
        if (current_id[Class]) {
            // identifier exists
            printf("%d: duplicate global declaration\n", line);
            exit(-1);
        }

        match(Id);
        current_id[Type] = type;

        if (token == '(') {
            // find the matching (, this is a function
            current_id[Class] = Fun;
            current_id[Value] =
                (int)(text + 1);  // the memory address of function, which is in
                                  // code segment
            function_declaration();
        } else {
            // variable declaration
            current_id[Class] = Glo;        // global variable
            current_id[Value] = (int)data;  // assign memory address
            data = data + sizeof(int);
        }

        if (token == ',') {
            match(',');
        }
    }
    next();  // ; }
}

// for syntax analysis entry, analyze the entire C language program
void program() {
    next();  // get next token
    while (token > 0) {
        global_declaration();
    }
}

// the entry point of the virtual machine, used to interpret the object code
int eval() {
    int op, *tmp;
    cycle = 0;
    while (1) {
        cycle++;
        op = *pc++;  // get next operation code

        // print debug info
        if (debug) {
            printf("%d> %.4s", cycle,
                   & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                   "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                   "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT"[op * 5]);
            if (op <= ADJ)
                printf(" %d\n", *pc);
            else
                printf("\n");
        }

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
            *(int*)*sp++ = ax;
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
            sp = sp + *pc++;
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

// the main function
int main(int argc, char** argv) {
    int i, fd;
    int* tmp;

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

    if (!(pc = (int*)idmain[Value])) {
        printf("main() not defined\n");
        return -1;
    }

    // setup stack
    sp = (int*)((int)stack + poolsize);
    *--sp = EXIT;  // call exit if main returns
    *--sp = PUSH;
    tmp = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)tmp;

    return eval();
}