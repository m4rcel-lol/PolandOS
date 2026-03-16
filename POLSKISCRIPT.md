# PolskiScript — Polski Język Programowania

**PolskiScript** is a custom programming language built into PolandOS, featuring Polish keywords and a clean, simple syntax. It's a dynamically-typed, interpreted language designed specifically for the PolandOS kernel environment.

## Overview

PolskiScript brings the power of a full programming language to PolandOS with:
- **Polish keywords** — All control structures and built-in functions use Polish language
- **Dynamic typing** — Variables can hold integers, floats, strings, booleans, and functions
- **Interactive REPL** — Read-Eval-Print Loop for immediate feedback
- **Full interpreter** — Complete lexer, parser, and AST-based evaluator
- **Kernel integration** — Runs directly within the PolandOS kernel

## Language Features

### Data Types

PolskiScript supports the following types:
- `liczba` (number) — integers and floating-point numbers
- `tekst` (text) — string literals
- `prawda` / `falsz` — boolean values (true/false)
- `nic` — null/nil value
- `funkcja` — function objects

### Keywords

#### Declaration Keywords
- `zmienna` — variable declaration
- `funkcja` — function declaration

#### Control Flow
- `jesli` — if statement
- `inaczej` — else clause
- `dopoki` — while loop
- `dla` — for loop
- `zwroc` — return statement
- `przerwij` — break statement
- `kontynuuj` — continue statement

#### Built-in Functions
- `drukuj()` — print output to console
- `wczytaj()` — read input from user

### Operators

#### Arithmetic Operators
- `+` — addition
- `-` — subtraction
- `*` — multiplication
- `/` — division
- `%` — modulo

#### Comparison Operators
- `==` — equal to
- `!=` — not equal to
- `<` — less than
- `>` — greater than
- `<=` — less than or equal to
- `>=` — greater than or equal to

#### Logical Operators
- `&&` — logical AND
- `||` — logical OR
- `!` — logical NOT

#### Assignment
- `=` — assignment operator

## Syntax Examples

### Variables

```polskiscript
// Variable declaration
zmienna x = 42;
zmienna nazwa = "PolandOS";
zmienna flaga = prawda;

// Variable assignment
x = x + 10;
nazwa = "Orzel";
```

### Arithmetic

```polskiscript
zmienna a = 10;
zmienna b = 20;
zmienna suma = a + b;
zmienna iloczyn = a * b;
zmienna reszta = b % a;

drukuj(suma, iloczyn, reszta);
```

### Conditional Statements

```polskiscript
zmienna wiek = 25;

jesli (wiek >= 18) {
    drukuj("Pelnoletni");
} inaczej {
    drukuj("Niepelnoletni");
}
```

### While Loops

```polskiscript
zmienna i = 0;
dopoki (i < 10) {
    drukuj("Liczba:", i);
    i = i + 1;
}
```

### Functions

```polskiscript
funkcja fibonacci(n) {
    jesli (n <= 1) {
        zwroc n;
    }
    zwroc fibonacci(n - 1) + fibonacci(n - 2);
}

zmienna wynik = fibonacci(10);
drukuj("Fibonacci(10) =", wynik);
```

### Complex Example - Prime Number Checker

```polskiscript
funkcja czy_pierwsza(n) {
    jesli (n <= 1) {
        zwroc falsz;
    }

    zmienna i = 2;
    dopoki (i * i <= n) {
        jesli (n % i == 0) {
            zwroc falsz;
        }
        i = i + 1;
    }

    zwroc prawda;
}

zmienna liczba = 17;
jesli (czy_pierwsza(liczba)) {
    drukuj(liczba, "jest liczba pierwsza");
} inaczej {
    drukuj(liczba, "nie jest liczba pierwsza");
}
```

### Factorial Calculator

```polskiscript
funkcja silnia(n) {
    jesli (n <= 1) {
        zwroc 1;
    }
    zwroc n * silnia(n - 1);
}

zmienna n = 5;
drukuj("Silnia z", n, "to", silnia(n));
```

### Fibonacci Sequence Generator

```polskiscript
funkcja drukuj_fibonacci(max) {
    zmienna a = 0;
    zmienna b = 1;

    dopoki (a <= max) {
        drukuj(a);
        zmienna temp = a + b;
        a = b;
        b = temp;
    }
}

drukuj_fibonacci(100);
```

## Implementation Details

### Architecture

PolskiScript is implemented in three main phases:

1. **Lexical Analysis (Lexer)**
   - Tokenizes source code into lexical tokens
   - Recognizes keywords, identifiers, literals, and operators
   - Handles single-line comments (`//`)

2. **Syntactic Analysis (Parser)**
   - Builds an Abstract Syntax Tree (AST)
   - Recursive descent parser with operator precedence
   - Validates syntax and reports errors with line numbers

3. **Evaluation (Interpreter)**
   - Traverses and evaluates the AST
   - Dynamic variable environment with lexical scoping
   - Runtime error handling

### Memory Management

- Uses PolandOS kernel heap (`kmalloc`/`kfree`)
- Automatic memory cleanup for AST nodes
- Value types are copied (no garbage collection needed for simple types)
- String values are heap-allocated and manually freed

### Limitations

- **No arrays/lists** — Only scalar types supported
- **No objects/classes** — Functional programming only
- **Limited recursion depth** — Stack-based, avoid deep recursion
- **No standard library** — Only `drukuj()` and `wczytaj()` built-ins
- **Single-threaded** — No concurrency support
- **No file I/O** — No filesystem operations yet
- **Fixed environment size** — Maximum 256 variables per scope

## Using PolskiScript

### Interactive REPL

To start the PolskiScript REPL:

```c
polskiscript_repl();
```

The REPL provides an interactive prompt where you can type PolskiScript code and see immediate results.

### Executing from String

To execute PolskiScript code from a C string:

```c
const char *code =
    "zmienna x = 42; "
    "drukuj(\"Wynik:\", x * 2);";

polskiscript_eval_string(code);
```

### Error Handling

PolskiScript reports errors with descriptive messages:

```
[Blad parsowania] Oczekiwano SREDNIKROPEK, otrzymano KONIEC w linii 5
[Blad wykonania] Niezdefiniowana zmienna: foo
```

## Language Grammar

### Formal Grammar (EBNF-like)

```
program       → statement* EOF
statement     → varDecl | funcDecl | printStmt | ifStmt | whileStmt |
                returnStmt | breakStmt | continueStmt | exprStmt | block
varDecl       → "zmienna" IDENTIFIER ( "=" expression )? ";"
funcDecl      → "funkcja" IDENTIFIER "(" parameters? ")" block
printStmt     → "drukuj" "(" arguments? ")" ";"
ifStmt        → "jesli" "(" expression ")" block ( "inaczej" block )?
whileStmt     → "dopoki" "(" expression ")" block
returnStmt    → "zwroc" expression? ";"
breakStmt     → "przerwij" ";"
continueStmt  → "kontynuuj" ";"
exprStmt      → expression ";"
block         → "{" statement* "}"

expression    → logical
logical       → comparison ( ( "&&" | "||" ) comparison )*
comparison    → term ( ( "==" | "!=" | "<" | ">" | "<=" | ">=" ) term )*
term          → factor ( ( "+" | "-" ) factor )*
factor        → unary ( ( "*" | "/" | "%" ) unary )*
unary         → ( "-" | "!" ) unary | call
call          → primary ( "(" arguments? ")" )?
primary       → NUMBER | STRING | "prawda" | "falsz" | "nic" |
                IDENTIFIER | "(" expression ")"

parameters    → IDENTIFIER ( "," IDENTIFIER )*
arguments     → expression ( "," expression )*
```

## Future Enhancements

Planned features for future versions:

- **Arrays and lists** — Support for collection types
- **String operations** — Concatenation, substring, length
- **File I/O** — Read and write files when VFS is implemented
- **Standard library** — Math functions, string utilities
- **Module system** — Import/export between scripts
- **Better error messages** — Stack traces, better diagnostics
- **Optimization** — Bytecode compilation, JIT?
- **Debugger** — Step-through debugging support

## Integration with PolandOS

PolskiScript is fully integrated into the PolandOS kernel:

### Kernel Integration Points

```c
// In kernel/kmain.c or kernel/shell/shell.c
#include "lang/polskiscript.h"

// Execute a script
polskiscript_eval_string("drukuj(\"Witaj PolandOS!\");");

// Start REPL
polskiscript_repl();
```

### Shell Integration

PolskiScript can be invoked from the PolandOS shell:

```
>>> skrypt moj_program.ps
>>> repl
```

## Example Programs

### Hello World

```polskiscript
drukuj("Witaj, PolandOS!");
```

### Sum of First N Numbers

```polskiscript
funkcja suma_n(n) {
    zmienna suma = 0;
    zmienna i = 1;

    dopoki (i <= n) {
        suma = suma + i;
        i = i + 1;
    }

    zwroc suma;
}

drukuj("Suma liczb od 1 do 100:", suma_n(100));
```

### GCD (Greatest Common Divisor)

```polskiscript
funkcja nwd(a, b) {
    dopoki (b != 0) {
        zmienna temp = b;
        b = a % b;
        a = temp;
    }
    zwroc a;
}

drukuj("NWD(48, 18) =", nwd(48, 18));
```

### Power Function

```polskiscript
funkcja potega(podstawa, wykladnik) {
    zmienna wynik = 1;
    zmienna i = 0;

    dopoki (i < wykladnik) {
        wynik = wynik * podstawa;
        i = i + 1;
    }

    zwroc wynik;
}

drukuj("2^10 =", potega(2, 10));
```

## Technical Specifications

### Token Types
- 40+ token types including keywords, operators, and literals
- Line and column tracking for error messages
- Support for integers, floats, strings, and identifiers

### AST Node Types
- 20+ AST node types for complete language coverage
- Tree-based representation of program structure
- Efficient traversal and evaluation

### Value System
- Tagged union type for runtime values
- Support for integers (i64), floats (double), booleans, strings, and functions
- Reference counting could be added for advanced memory management

### Environment
- Lexically-scoped variable environments
- Parent pointer for scope chain lookup
- Support for nested scopes and closures

## Contributing

PolskiScript is part of PolandOS. To contribute:

1. Write tests for your features
2. Follow the existing code style
3. Document new features in this file
4. Submit a pull request

## License

PolskiScript © 2024 — Part of PolandOS. Original work, written from scratch.

**For Poland. For freedom. For code.**

---

> "Polska nigdy nie zginie — Poland is not yet lost. Write a language."
