// PolandOS — PolskiScript — Polski jezyk programowania
// Custom programming language for PolandOS with Polish keywords
#ifndef LANG_POLSKISCRIPT_H
#define LANG_POLSKISCRIPT_H

#include "../../include/types.h"

// ═══════════════════════════════════════════════════════════════════════════
// Token Types — Typy tokenow
// ═══════════════════════════════════════════════════════════════════════════
typedef enum {
    // Literals
    TOK_LICZBA,          // Number (integer or float)
    TOK_TEKST,           // String literal
    TOK_PRAWDA,          // true (Boolean)
    TOK_FALSZ,           // false (Boolean)
    TOK_NIC,             // null/nil

    // Identifiers and operators
    TOK_IDENTYFIKATOR,   // Variable/function name
    TOK_PLUS,            // +
    TOK_MINUS,           // -
    TOK_MNOZENIE,        // *
    TOK_DZIELENIE,       // /
    TOK_MODULO,          // %
    TOK_ROWNE,           // ==
    TOK_NIEROWNE,        // !=
    TOK_MNIEJSZE,        // <
    TOK_WIEKSZE,         // >
    TOK_MNIEJSZE_ROWNE,  // <=
    TOK_WIEKSZE_ROWNE,   // >=
    TOK_PRZYPISANIE,     // =
    TOK_I,               // && (logical AND)
    TOK_LUB,             // || (logical OR)
    TOK_NIE,             // ! (logical NOT)

    // Keywords
    TOK_ZMIENNA,         // var — variable declaration
    TOK_FUNKCJA,         // func — function declaration
    TOK_ZWROC,           // return
    TOK_JESLI,           // if
    TOK_INACZEJ,         // else
    TOK_DOPOKI,          // while
    TOK_DLA,             // for
    TOK_DRUKUJ,          // print
    TOK_WCZYTAJ,         // input
    TOK_PRZERWIJ,        // break
    TOK_KONTYNUUJ,       // continue

    // Delimiters
    TOK_SREDNIKROPEK,    // ;
    TOK_PRZECINEK,       // ,
    TOK_KROPKA,          // .
    TOK_DWUKROPEK,       // :
    TOK_NAWIAS_LEWY,     // (
    TOK_NAWIAS_PRAWY,    // )
    TOK_KWADRAT_LEWY,    // [
    TOK_KWADRAT_PRAWY,   // ]
    TOK_KLAMRA_LEWA,     // {
    TOK_KLAMRA_PRAWA,    // }

    // Special
    TOK_KONIEC,          // End of input
    TOK_NIEZNANY         // Unknown/error token
} TokenType;

typedef struct {
    TokenType type;
    char value[256];     // Token string value
    int line;            // Line number
    int column;          // Column number
} Token;

// ═══════════════════════════════════════════════════════════════════════════
// Abstract Syntax Tree (AST) — Abstrakcyjne drzewo skladni
// ═══════════════════════════════════════════════════════════════════════════
typedef enum {
    AST_PROGRAM,
    AST_VAR_DECL,        // Variable declaration
    AST_FUNC_DECL,       // Function declaration
    AST_ASSIGN,          // Assignment
    AST_BINARY_OP,       // Binary operation (+, -, *, /, etc.)
    AST_UNARY_OP,        // Unary operation (-, !)
    AST_CALL,            // Function call
    AST_IF,              // If statement
    AST_WHILE,           // While loop
    AST_FOR,             // For loop
    AST_RETURN,          // Return statement
    AST_PRINT,           // Print statement
    AST_INPUT,           // Input statement
    AST_BREAK,           // Break statement
    AST_CONTINUE,        // Continue statement
    AST_BLOCK,           // Block of statements
    AST_IDENTIFIER,      // Variable/function reference
    AST_LITERAL_INT,     // Integer literal
    AST_LITERAL_FLOAT,   // Float literal
    AST_LITERAL_STRING,  // String literal
    AST_LITERAL_BOOL,    // Boolean literal
    AST_LITERAL_NULL     // Null literal
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    char *str_value;     // For identifiers, strings
    i64 int_value;       // For integers
    double float_value;  // For floats
    bool bool_value;     // For booleans
    struct ASTNode **children;  // Child nodes
    int child_count;
    int line;
    int column;
} ASTNode;

// ═══════════════════════════════════════════════════════════════════════════
// Value Types — Typy wartosci w interpreterze
// ═══════════════════════════════════════════════════════════════════════════
typedef enum {
    VAL_NULL,
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_STRING,
    VAL_FUNCTION
} ValueType;

typedef struct {
    ValueType type;
    union {
        i64 as_int;
        double as_float;
        bool as_bool;
        char *as_string;
        struct {
            char **params;
            int param_count;
            ASTNode *body;
        } as_function;
    };
} Value;

// ═══════════════════════════════════════════════════════════════════════════
// Environment — Srodowisko zmiennych i funkcji
// ═══════════════════════════════════════════════════════════════════════════
#define ENV_MAX_VARS 256

typedef struct Environment {
    char *names[ENV_MAX_VARS];
    Value values[ENV_MAX_VARS];
    int count;
    struct Environment *parent;  // For lexical scoping
} Environment;

// ═══════════════════════════════════════════════════════════════════════════
// Interpreter State — Stan interpretera
// ═══════════════════════════════════════════════════════════════════════════
typedef struct {
    Environment *global_env;
    Environment *current_env;
    bool break_flag;
    bool continue_flag;
    bool return_flag;
    Value return_value;
    char error[512];
    bool has_error;
} Interpreter;

// ═══════════════════════════════════════════════════════════════════════════
// Lexer — Analizator leksykalny
// ═══════════════════════════════════════════════════════════════════════════
typedef struct {
    const char *source;
    int pos;
    int line;
    int column;
    char current_char;
} Lexer;

void lexer_init(Lexer *lex, const char *source);
Token lexer_next_token(Lexer *lex);
const char *token_type_to_string(TokenType type);

// ═══════════════════════════════════════════════════════════════════════════
// Parser — Analizator skladniowy
// ═══════════════════════════════════════════════════════════════════════════
typedef struct {
    Lexer *lexer;
    Token current_token;
    char error[512];
    bool has_error;
} Parser;

void parser_init(Parser *parser, Lexer *lexer);
ASTNode *parser_parse_program(Parser *parser);
void ast_node_free(ASTNode *node);

// ═══════════════════════════════════════════════════════════════════════════
// Interpreter — Interpreter
// ═══════════════════════════════════════════════════════════════════════════
void interpreter_init(Interpreter *interp);
void interpreter_free(Interpreter *interp);
Value interpreter_eval(Interpreter *interp, ASTNode *node);
void interpreter_execute(Interpreter *interp, const char *source);

// ═══════════════════════════════════════════════════════════════════════════
// REPL — Read-Eval-Print Loop
// ═══════════════════════════════════════════════════════════════════════════
void polskiscript_repl(void);
void polskiscript_run_file(const char *filename);
void polskiscript_eval_string(const char *source);

// ═══════════════════════════════════════════════════════════════════════════
// Value Operations
// ═══════════════════════════════════════════════════════════════════════════
Value value_create_null(void);
Value value_create_int(i64 val);
Value value_create_float(double val);
Value value_create_bool(bool val);
Value value_create_string(const char *str);
void value_free(Value *val);
void value_print(const Value *val);
const char *value_type_name(ValueType type);

// ═══════════════════════════════════════════════════════════════════════════
// Environment Operations
// ═══════════════════════════════════════════════════════════════════════════
Environment *env_create(Environment *parent);
void env_free(Environment *env);
void env_define(Environment *env, const char *name, Value value);
Value *env_get(Environment *env, const char *name);
bool env_set(Environment *env, const char *name, Value value);

#endif // LANG_POLSKISCRIPT_H
