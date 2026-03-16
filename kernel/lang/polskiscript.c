// PolandOS — PolskiScript Implementation
// Full custom programming language with Polish keywords
#include "polskiscript.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../arch/x86_64/mm/heap.h"

// ═══════════════════════════════════════════════════════════════════════════
// LEXER IMPLEMENTATION — Analizator leksykalny
// ═══════════════════════════════════════════════════════════════════════════

static void lexer_advance(Lexer *lex) {
    if (lex->source[lex->pos] == '\0') {
        lex->current_char = '\0';
        return;
    }

    if (lex->current_char == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }

    lex->pos++;
    lex->current_char = lex->source[lex->pos];
}

static char lexer_peek(Lexer *lex) {
    return lex->source[lex->pos + 1];
}

static void lexer_skip_whitespace(Lexer *lex) {
    while (lex->current_char == ' ' || lex->current_char == '\t' ||
           lex->current_char == '\r' || lex->current_char == '\n') {
        lexer_advance(lex);
    }
}

static void lexer_skip_comment(Lexer *lex) {
    if (lex->current_char == '/' && lexer_peek(lex) == '/') {
        while (lex->current_char != '\n' && lex->current_char != '\0') {
            lexer_advance(lex);
        }
    }
}

static Token lexer_make_token(TokenType type, const char *value, int line, int col) {
    Token tok;
    tok.type = type;
    tok.line = line;
    tok.column = col;
    strncpy(tok.value, value, 255);
    tok.value[255] = '\0';
    return tok;
}

static Token lexer_number(Lexer *lex) {
    int start_line = lex->line;
    int start_col = lex->column;
    char num[256] = {0};
    int i = 0;
    bool is_float = false;

    while ((lex->current_char >= '0' && lex->current_char <= '9') ||
           lex->current_char == '.') {
        if (lex->current_char == '.') {
            if (is_float) break;  // Second dot - invalid
            is_float = true;
        }
        if (i < 255) num[i++] = lex->current_char;
        lexer_advance(lex);
    }

    return lexer_make_token(TOK_LICZBA, num, start_line, start_col);
}

static Token lexer_string(Lexer *lex) {
    int start_line = lex->line;
    int start_col = lex->column;
    char str[256] = {0};
    int i = 0;

    lexer_advance(lex);  // Skip opening quote

    while (lex->current_char != '"' && lex->current_char != '\0') {
        if (i < 255) str[i++] = lex->current_char;
        lexer_advance(lex);
    }

    if (lex->current_char == '"') {
        lexer_advance(lex);  // Skip closing quote
    }

    return lexer_make_token(TOK_TEKST, str, start_line, start_col);
}

static Token lexer_identifier(Lexer *lex) {
    int start_line = lex->line;
    int start_col = lex->column;
    char id[256] = {0};
    int i = 0;

    while ((lex->current_char >= 'a' && lex->current_char <= 'z') ||
           (lex->current_char >= 'A' && lex->current_char <= 'Z') ||
           (lex->current_char >= '0' && lex->current_char <= '9') ||
           lex->current_char == '_') {
        if (i < 255) id[i++] = lex->current_char;
        lexer_advance(lex);
    }

    // Check for keywords
    if (strcmp(id, "zmienna") == 0) return lexer_make_token(TOK_ZMIENNA, id, start_line, start_col);
    if (strcmp(id, "funkcja") == 0) return lexer_make_token(TOK_FUNKCJA, id, start_line, start_col);
    if (strcmp(id, "zwroc") == 0) return lexer_make_token(TOK_ZWROC, id, start_line, start_col);
    if (strcmp(id, "jesli") == 0) return lexer_make_token(TOK_JESLI, id, start_line, start_col);
    if (strcmp(id, "inaczej") == 0) return lexer_make_token(TOK_INACZEJ, id, start_line, start_col);
    if (strcmp(id, "dopoki") == 0) return lexer_make_token(TOK_DOPOKI, id, start_line, start_col);
    if (strcmp(id, "dla") == 0) return lexer_make_token(TOK_DLA, id, start_line, start_col);
    if (strcmp(id, "drukuj") == 0) return lexer_make_token(TOK_DRUKUJ, id, start_line, start_col);
    if (strcmp(id, "wczytaj") == 0) return lexer_make_token(TOK_WCZYTAJ, id, start_line, start_col);
    if (strcmp(id, "przerwij") == 0) return lexer_make_token(TOK_PRZERWIJ, id, start_line, start_col);
    if (strcmp(id, "kontynuuj") == 0) return lexer_make_token(TOK_KONTYNUUJ, id, start_line, start_col);
    if (strcmp(id, "prawda") == 0) return lexer_make_token(TOK_PRAWDA, id, start_line, start_col);
    if (strcmp(id, "falsz") == 0) return lexer_make_token(TOK_FALSZ, id, start_line, start_col);
    if (strcmp(id, "nic") == 0) return lexer_make_token(TOK_NIC, id, start_line, start_col);

    return lexer_make_token(TOK_IDENTYFIKATOR, id, start_line, start_col);
}

void lexer_init(Lexer *lex, const char *source) {
    lex->source = source;
    lex->pos = 0;
    lex->line = 1;
    lex->column = 1;
    lex->current_char = source[0];
}

Token lexer_next_token(Lexer *lex) {
    while (lex->current_char != '\0') {
        if (lex->current_char == ' ' || lex->current_char == '\t' ||
            lex->current_char == '\r' || lex->current_char == '\n') {
            lexer_skip_whitespace(lex);
            continue;
        }

        if (lex->current_char == '/' && lexer_peek(lex) == '/') {
            lexer_skip_comment(lex);
            continue;
        }

        // Numbers
        if (lex->current_char >= '0' && lex->current_char <= '9') {
            return lexer_number(lex);
        }

        // Strings
        if (lex->current_char == '"') {
            return lexer_string(lex);
        }

        // Identifiers and keywords
        if ((lex->current_char >= 'a' && lex->current_char <= 'z') ||
            (lex->current_char >= 'A' && lex->current_char <= 'Z') ||
            lex->current_char == '_') {
            return lexer_identifier(lex);
        }

        // Two-character operators
        int line = lex->line;
        int col = lex->column;
        char ch = lex->current_char;
        char next = lexer_peek(lex);

        if (ch == '=' && next == '=') {
            lexer_advance(lex); lexer_advance(lex);
            return lexer_make_token(TOK_ROWNE, "==", line, col);
        }
        if (ch == '!' && next == '=') {
            lexer_advance(lex); lexer_advance(lex);
            return lexer_make_token(TOK_NIEROWNE, "!=", line, col);
        }
        if (ch == '<' && next == '=') {
            lexer_advance(lex); lexer_advance(lex);
            return lexer_make_token(TOK_MNIEJSZE_ROWNE, "<=", line, col);
        }
        if (ch == '>' && next == '=') {
            lexer_advance(lex); lexer_advance(lex);
            return lexer_make_token(TOK_WIEKSZE_ROWNE, ">=", line, col);
        }
        if (ch == '&' && next == '&') {
            lexer_advance(lex); lexer_advance(lex);
            return lexer_make_token(TOK_I, "&&", line, col);
        }
        if (ch == '|' && next == '|') {
            lexer_advance(lex); lexer_advance(lex);
            return lexer_make_token(TOK_LUB, "||", line, col);
        }

        // Single-character tokens
        TokenType type = TOK_NIEZNANY;
        char val[2] = {ch, '\0'};

        switch (ch) {
            case '+': type = TOK_PLUS; break;
            case '-': type = TOK_MINUS; break;
            case '*': type = TOK_MNOZENIE; break;
            case '/': type = TOK_DZIELENIE; break;
            case '%': type = TOK_MODULO; break;
            case '<': type = TOK_MNIEJSZE; break;
            case '>': type = TOK_WIEKSZE; break;
            case '=': type = TOK_PRZYPISANIE; break;
            case '!': type = TOK_NIE; break;
            case ';': type = TOK_SREDNIKROPEK; break;
            case ',': type = TOK_PRZECINEK; break;
            case '.': type = TOK_KROPKA; break;
            case ':': type = TOK_DWUKROPEK; break;
            case '(': type = TOK_NAWIAS_LEWY; break;
            case ')': type = TOK_NAWIAS_PRAWY; break;
            case '[': type = TOK_KWADRAT_LEWY; break;
            case ']': type = TOK_KWADRAT_PRAWY; break;
            case '{': type = TOK_KLAMRA_LEWA; break;
            case '}': type = TOK_KLAMRA_PRAWA; break;
        }

        lexer_advance(lex);
        return lexer_make_token(type, val, line, col);
    }

    return lexer_make_token(TOK_KONIEC, "", lex->line, lex->column);
}

const char *token_type_to_string(TokenType type) {
    switch (type) {
        case TOK_LICZBA: return "LICZBA";
        case TOK_TEKST: return "TEKST";
        case TOK_PRAWDA: return "PRAWDA";
        case TOK_FALSZ: return "FALSZ";
        case TOK_NIC: return "NIC";
        case TOK_IDENTYFIKATOR: return "IDENTYFIKATOR";
        case TOK_ZMIENNA: return "ZMIENNA";
        case TOK_FUNKCJA: return "FUNKCJA";
        case TOK_ZWROC: return "ZWROC";
        case TOK_JESLI: return "JESLI";
        case TOK_INACZEJ: return "INACZEJ";
        case TOK_DOPOKI: return "DOPOKI";
        case TOK_DLA: return "DLA";
        case TOK_DRUKUJ: return "DRUKUJ";
        case TOK_KONIEC: return "KONIEC";
        default: return "NIEZNANY";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// AST NODE OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════

static ASTNode *ast_node_create(ASTNodeType type) {
    ASTNode *node = (ASTNode *)kmalloc(sizeof(ASTNode));
    if (!node) return NULL;

    memset(node, 0, sizeof(ASTNode));
    node->type = type;
    node->str_value = NULL;
    node->children = NULL;
    node->child_count = 0;
    return node;
}

static void ast_node_add_child(ASTNode *parent, ASTNode *child) {
    if (!parent || !child) return;

    parent->children = (ASTNode **)krealloc(parent->children,
        sizeof(ASTNode *) * (parent->child_count + 1));
    parent->children[parent->child_count++] = child;
}

void ast_node_free(ASTNode *node) {
    if (!node) return;

    if (node->str_value) kfree(node->str_value);

    for (int i = 0; i < node->child_count; i++) {
        ast_node_free(node->children[i]);
    }

    if (node->children) kfree(node->children);
    kfree(node);
}

// ═══════════════════════════════════════════════════════════════════════════
// PARSER IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════

static void parser_advance(Parser *p) {
    p->current_token = lexer_next_token(p->lexer);
}

static bool parser_expect(Parser *p, TokenType type) {
    if (p->current_token.type != type) {
        ksnprintf(p->error, 512, "Oczekiwano %s, otrzymano %s w linii %d",
                 token_type_to_string(type),
                 token_type_to_string(p->current_token.type),
                 p->current_token.line);
        p->has_error = true;
        return false;
    }
    parser_advance(p);
    return true;
}

// Forward declarations
static ASTNode *parser_parse_statement(Parser *p);
static ASTNode *parser_parse_expression(Parser *p);

static ASTNode *parser_parse_primary(Parser *p) {
    ASTNode *node = NULL;

    if (p->current_token.type == TOK_LICZBA) {
        // Check if it's a float or int
        bool is_float = false;
        for (int i = 0; p->current_token.value[i]; i++) {
            if (p->current_token.value[i] == '.') {
                is_float = true;
                break;
            }
        }

        if (is_float) {
            node = ast_node_create(AST_LITERAL_FLOAT);
            // Simple float parsing
            double val = 0.0;
            double frac = 0.1;
            bool after_dot = false;
            for (int i = 0; p->current_token.value[i]; i++) {
                if (p->current_token.value[i] == '.') {
                    after_dot = true;
                } else if (after_dot) {
                    val += (p->current_token.value[i] - '0') * frac;
                    frac *= 0.1;
                } else {
                    val = val * 10 + (p->current_token.value[i] - '0');
                }
            }
            node->float_value = val;
        } else {
            node = ast_node_create(AST_LITERAL_INT);
            i64 val = 0;
            for (int i = 0; p->current_token.value[i]; i++) {
                val = val * 10 + (p->current_token.value[i] - '0');
            }
            node->int_value = val;
        }
        parser_advance(p);
    }
    else if (p->current_token.type == TOK_TEKST) {
        node = ast_node_create(AST_LITERAL_STRING);
        node->str_value = (char *)kmalloc(strlen(p->current_token.value) + 1);
        strcpy(node->str_value, p->current_token.value);
        parser_advance(p);
    }
    else if (p->current_token.type == TOK_PRAWDA) {
        node = ast_node_create(AST_LITERAL_BOOL);
        node->bool_value = true;
        parser_advance(p);
    }
    else if (p->current_token.type == TOK_FALSZ) {
        node = ast_node_create(AST_LITERAL_BOOL);
        node->bool_value = false;
        parser_advance(p);
    }
    else if (p->current_token.type == TOK_NIC) {
        node = ast_node_create(AST_LITERAL_NULL);
        parser_advance(p);
    }
    else if (p->current_token.type == TOK_IDENTYFIKATOR) {
        node = ast_node_create(AST_IDENTIFIER);
        node->str_value = (char *)kmalloc(strlen(p->current_token.value) + 1);
        strcpy(node->str_value, p->current_token.value);
        parser_advance(p);

        // Function call
        if (p->current_token.type == TOK_NAWIAS_LEWY) {
            ASTNode *call = ast_node_create(AST_CALL);
            ast_node_add_child(call, node);
            parser_advance(p);  // Skip (

            // Parse arguments
            if (p->current_token.type != TOK_NAWIAS_PRAWY) {
                do {
                    if (p->current_token.type == TOK_PRZECINEK) {
                        parser_advance(p);
                    }
                    ASTNode *arg = parser_parse_expression(p);
                    if (arg) ast_node_add_child(call, arg);
                } while (p->current_token.type == TOK_PRZECINEK);
            }

            parser_expect(p, TOK_NAWIAS_PRAWY);
            node = call;
        }
    }
    else if (p->current_token.type == TOK_NAWIAS_LEWY) {
        parser_advance(p);
        node = parser_parse_expression(p);
        parser_expect(p, TOK_NAWIAS_PRAWY);
    }

    return node;
}

static ASTNode *parser_parse_unary(Parser *p) {
    if (p->current_token.type == TOK_MINUS || p->current_token.type == TOK_NIE) {
        Token op = p->current_token;
        parser_advance(p);
        ASTNode *node = ast_node_create(AST_UNARY_OP);
        node->str_value = (char *)kmalloc(strlen(op.value) + 1);
        strcpy(node->str_value, op.value);
        ASTNode *operand = parser_parse_unary(p);
        ast_node_add_child(node, operand);
        return node;
    }

    return parser_parse_primary(p);
}

static ASTNode *parser_parse_factor(Parser *p) {
    ASTNode *left = parser_parse_unary(p);

    while (p->current_token.type == TOK_MNOZENIE ||
           p->current_token.type == TOK_DZIELENIE ||
           p->current_token.type == TOK_MODULO) {
        Token op = p->current_token;
        parser_advance(p);
        ASTNode *right = parser_parse_unary(p);

        ASTNode *node = ast_node_create(AST_BINARY_OP);
        node->str_value = (char *)kmalloc(strlen(op.value) + 1);
        strcpy(node->str_value, op.value);
        ast_node_add_child(node, left);
        ast_node_add_child(node, right);
        left = node;
    }

    return left;
}

static ASTNode *parser_parse_term(Parser *p) {
    ASTNode *left = parser_parse_factor(p);

    while (p->current_token.type == TOK_PLUS || p->current_token.type == TOK_MINUS) {
        Token op = p->current_token;
        parser_advance(p);
        ASTNode *right = parser_parse_factor(p);

        ASTNode *node = ast_node_create(AST_BINARY_OP);
        node->str_value = (char *)kmalloc(strlen(op.value) + 1);
        strcpy(node->str_value, op.value);
        ast_node_add_child(node, left);
        ast_node_add_child(node, right);
        left = node;
    }

    return left;
}

static ASTNode *parser_parse_comparison(Parser *p) {
    ASTNode *left = parser_parse_term(p);

    while (p->current_token.type == TOK_MNIEJSZE ||
           p->current_token.type == TOK_WIEKSZE ||
           p->current_token.type == TOK_MNIEJSZE_ROWNE ||
           p->current_token.type == TOK_WIEKSZE_ROWNE ||
           p->current_token.type == TOK_ROWNE ||
           p->current_token.type == TOK_NIEROWNE) {
        Token op = p->current_token;
        parser_advance(p);
        ASTNode *right = parser_parse_term(p);

        ASTNode *node = ast_node_create(AST_BINARY_OP);
        node->str_value = (char *)kmalloc(strlen(op.value) + 1);
        strcpy(node->str_value, op.value);
        ast_node_add_child(node, left);
        ast_node_add_child(node, right);
        left = node;
    }

    return left;
}

static ASTNode *parser_parse_logical(Parser *p) {
    ASTNode *left = parser_parse_comparison(p);

    while (p->current_token.type == TOK_I || p->current_token.type == TOK_LUB) {
        Token op = p->current_token;
        parser_advance(p);
        ASTNode *right = parser_parse_comparison(p);

        ASTNode *node = ast_node_create(AST_BINARY_OP);
        node->str_value = (char *)kmalloc(strlen(op.value) + 1);
        strcpy(node->str_value, op.value);
        ast_node_add_child(node, left);
        ast_node_add_child(node, right);
        left = node;
    }

    return left;
}

static ASTNode *parser_parse_expression(Parser *p) {
    return parser_parse_logical(p);
}

static ASTNode *parser_parse_block(Parser *p) {
    ASTNode *block = ast_node_create(AST_BLOCK);
    parser_expect(p, TOK_KLAMRA_LEWA);

    while (p->current_token.type != TOK_KLAMRA_PRAWA &&
           p->current_token.type != TOK_KONIEC) {
        ASTNode *stmt = parser_parse_statement(p);
        if (stmt) ast_node_add_child(block, stmt);
        if (p->has_error) break;
    }

    parser_expect(p, TOK_KLAMRA_PRAWA);
    return block;
}

static ASTNode *parser_parse_statement(Parser *p) {
    // Variable declaration
    if (p->current_token.type == TOK_ZMIENNA) {
        parser_advance(p);
        ASTNode *node = ast_node_create(AST_VAR_DECL);

        if (p->current_token.type != TOK_IDENTYFIKATOR) {
            p->has_error = true;
            return NULL;
        }

        node->str_value = (char *)kmalloc(strlen(p->current_token.value) + 1);
        strcpy(node->str_value, p->current_token.value);
        parser_advance(p);

        if (p->current_token.type == TOK_PRZYPISANIE) {
            parser_advance(p);
            ASTNode *expr = parser_parse_expression(p);
            ast_node_add_child(node, expr);
        }

        parser_expect(p, TOK_SREDNIKROPEK);
        return node;
    }

    // Print statement
    if (p->current_token.type == TOK_DRUKUJ) {
        parser_advance(p);
        ASTNode *node = ast_node_create(AST_PRINT);
        parser_expect(p, TOK_NAWIAS_LEWY);

        if (p->current_token.type != TOK_NAWIAS_PRAWY) {
            do {
                if (p->current_token.type == TOK_PRZECINEK) parser_advance(p);
                ASTNode *arg = parser_parse_expression(p);
                if (arg) ast_node_add_child(node, arg);
            } while (p->current_token.type == TOK_PRZECINEK);
        }

        parser_expect(p, TOK_NAWIAS_PRAWY);
        parser_expect(p, TOK_SREDNIKROPEK);
        return node;
    }

    // If statement
    if (p->current_token.type == TOK_JESLI) {
        parser_advance(p);
        ASTNode *node = ast_node_create(AST_IF);
        parser_expect(p, TOK_NAWIAS_LEWY);
        ASTNode *cond = parser_parse_expression(p);
        parser_expect(p, TOK_NAWIAS_PRAWY);

        ASTNode *then_block = parser_parse_block(p);
        ast_node_add_child(node, cond);
        ast_node_add_child(node, then_block);

        if (p->current_token.type == TOK_INACZEJ) {
            parser_advance(p);
            ASTNode *else_block = parser_parse_block(p);
            ast_node_add_child(node, else_block);
        }

        return node;
    }

    // While loop
    if (p->current_token.type == TOK_DOPOKI) {
        parser_advance(p);
        ASTNode *node = ast_node_create(AST_WHILE);
        parser_expect(p, TOK_NAWIAS_LEWY);
        ASTNode *cond = parser_parse_expression(p);
        parser_expect(p, TOK_NAWIAS_PRAWY);
        ASTNode *body = parser_parse_block(p);

        ast_node_add_child(node, cond);
        ast_node_add_child(node, body);
        return node;
    }

    // Return statement
    if (p->current_token.type == TOK_ZWROC) {
        parser_advance(p);
        ASTNode *node = ast_node_create(AST_RETURN);
        if (p->current_token.type != TOK_SREDNIKROPEK) {
            ASTNode *expr = parser_parse_expression(p);
            ast_node_add_child(node, expr);
        }
        parser_expect(p, TOK_SREDNIKROPEK);
        return node;
    }

    // Break/Continue
    if (p->current_token.type == TOK_PRZERWIJ) {
        parser_advance(p);
        parser_expect(p, TOK_SREDNIKROPEK);
        return ast_node_create(AST_BREAK);
    }

    if (p->current_token.type == TOK_KONTYNUUJ) {
        parser_advance(p);
        parser_expect(p, TOK_SREDNIKROPEK);
        return ast_node_create(AST_CONTINUE);
    }

    // Assignment or expression statement
    if (p->current_token.type == TOK_IDENTYFIKATOR) {
        Token id = p->current_token;
        parser_advance(p);

        if (p->current_token.type == TOK_PRZYPISANIE) {
            parser_advance(p);
            ASTNode *node = ast_node_create(AST_ASSIGN);
            node->str_value = (char *)kmalloc(strlen(id.value) + 1);
            strcpy(node->str_value, id.value);
            ASTNode *expr = parser_parse_expression(p);
            ast_node_add_child(node, expr);
            parser_expect(p, TOK_SREDNIKROPEK);
            return node;
        } else {
            // Function call as statement - backtrack
            Lexer saved = *p->lexer;
            p->lexer->pos = id.column - 1;
            p->lexer->current_char = p->lexer->source[p->lexer->pos];
            parser_advance(p);
            ASTNode *expr = parser_parse_expression(p);
            parser_expect(p, TOK_SREDNIKROPEK);
            return expr;
        }
    }

    // Block
    if (p->current_token.type == TOK_KLAMRA_LEWA) {
        return parser_parse_block(p);
    }

    return NULL;
}

void parser_init(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->has_error = false;
    parser->error[0] = '\0';
    parser_advance(parser);
}

ASTNode *parser_parse_program(Parser *parser) {
    ASTNode *program = ast_node_create(AST_PROGRAM);

    while (parser->current_token.type != TOK_KONIEC) {
        ASTNode *stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_node_add_child(program, stmt);
        }
        if (parser->has_error) {
            ast_node_free(program);
            return NULL;
        }
    }

    return program;
}

// ═══════════════════════════════════════════════════════════════════════════
// VALUE OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════

Value value_create_null(void) {
    Value v;
    v.type = VAL_NULL;
    return v;
}

Value value_create_int(i64 val) {
    Value v;
    v.type = VAL_INT;
    v.as_int = val;
    return v;
}

Value value_create_float(double val) {
    Value v;
    v.type = VAL_FLOAT;
    v.as_float = val;
    return v;
}

Value value_create_bool(bool val) {
    Value v;
    v.type = VAL_BOOL;
    v.as_bool = val;
    return v;
}

Value value_create_string(const char *str) {
    Value v;
    v.type = VAL_STRING;
    v.as_string = (char *)kmalloc(strlen(str) + 1);
    strcpy(v.as_string, str);
    return v;
}

void value_free(Value *val) {
    if (val->type == VAL_STRING && val->as_string) {
        kfree(val->as_string);
    }
}

void value_print(const Value *val) {
    switch (val->type) {
        case VAL_NULL:
            kprintf("nic");
            break;
        case VAL_INT:
            kprintf("%lld", val->as_int);
            break;
        case VAL_FLOAT:
            kprintf("%lld.%03lld", (i64)val->as_float,
                   (i64)((val->as_float - (i64)val->as_float) * 1000));
            break;
        case VAL_BOOL:
            kprintf("%s", val->as_bool ? "prawda" : "falsz");
            break;
        case VAL_STRING:
            kprintf("%s", val->as_string);
            break;
        case VAL_FUNCTION:
            kprintf("<funkcja>");
            break;
    }
}

const char *value_type_name(ValueType type) {
    switch (type) {
        case VAL_NULL: return "nic";
        case VAL_INT: return "liczba";
        case VAL_FLOAT: return "liczba";
        case VAL_BOOL: return "bool";
        case VAL_STRING: return "tekst";
        case VAL_FUNCTION: return "funkcja";
        default: return "nieznany";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ENVIRONMENT OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════

Environment *env_create(Environment *parent) {
    Environment *env = (Environment *)kmalloc(sizeof(Environment));
    memset(env, 0, sizeof(Environment));
    env->parent = parent;
    env->count = 0;
    return env;
}

void env_free(Environment *env) {
    if (!env) return;

    for (int i = 0; i < env->count; i++) {
        if (env->names[i]) kfree(env->names[i]);
        value_free(&env->values[i]);
    }

    kfree(env);
}

void env_define(Environment *env, const char *name, Value value) {
    if (env->count >= ENV_MAX_VARS) return;

    // Check if already defined in this scope
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            value_free(&env->values[i]);
            env->values[i] = value;
            return;
        }
    }

    env->names[env->count] = (char *)kmalloc(strlen(name) + 1);
    strcpy(env->names[env->count], name);
    env->values[env->count] = value;
    env->count++;
}

Value *env_get(Environment *env, const char *name) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            return &env->values[i];
        }
    }

    if (env->parent) {
        return env_get(env->parent, name);
    }

    return NULL;
}

bool env_set(Environment *env, const char *name, Value value) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            value_free(&env->values[i]);
            env->values[i] = value;
            return true;
        }
    }

    if (env->parent) {
        return env_set(env->parent, name, value);
    }

    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// INTERPRETER IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════

void interpreter_init(Interpreter *interp) {
    interp->global_env = env_create(NULL);
    interp->current_env = interp->global_env;
    interp->break_flag = false;
    interp->continue_flag = false;
    interp->return_flag = false;
    interp->return_value = value_create_null();
    interp->has_error = false;
    interp->error[0] = '\0';
}

void interpreter_free(Interpreter *interp) {
    env_free(interp->global_env);
    value_free(&interp->return_value);
}

Value interpreter_eval(Interpreter *interp, ASTNode *node) {
    if (!node || interp->has_error) return value_create_null();

    switch (node->type) {
        case AST_LITERAL_INT:
            return value_create_int(node->int_value);

        case AST_LITERAL_FLOAT:
            return value_create_float(node->float_value);

        case AST_LITERAL_STRING:
            return value_create_string(node->str_value);

        case AST_LITERAL_BOOL:
            return value_create_bool(node->bool_value);

        case AST_LITERAL_NULL:
            return value_create_null();

        case AST_IDENTIFIER: {
            Value *val = env_get(interp->current_env, node->str_value);
            if (!val) {
                ksnprintf(interp->error, 512, "Niezdefiniowana zmienna: %s", node->str_value);
                interp->has_error = true;
                return value_create_null();
            }
            return *val;
        }

        case AST_VAR_DECL: {
            Value val = value_create_null();
            if (node->child_count > 0) {
                val = interpreter_eval(interp, node->children[0]);
            }
            env_define(interp->current_env, node->str_value, val);
            return value_create_null();
        }

        case AST_ASSIGN: {
            Value val = interpreter_eval(interp, node->children[0]);
            if (!env_set(interp->current_env, node->str_value, val)) {
                ksnprintf(interp->error, 512, "Niezdefiniowana zmienna: %s", node->str_value);
                interp->has_error = true;
            }
            return val;
        }

        case AST_BINARY_OP: {
            Value left = interpreter_eval(interp, node->children[0]);
            Value right = interpreter_eval(interp, node->children[1]);
            char op = node->str_value[0];

            // Arithmetic operations
            if (op == '+') {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_int(left.as_int + right.as_int);
                if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                    double l = (left.type == VAL_FLOAT) ? left.as_float : (double)left.as_int;
                    double r = (right.type == VAL_FLOAT) ? right.as_float : (double)right.as_int;
                    return value_create_float(l + r);
                }
            }
            if (op == '-') {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_int(left.as_int - right.as_int);
                if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                    double l = (left.type == VAL_FLOAT) ? left.as_float : (double)left.as_int;
                    double r = (right.type == VAL_FLOAT) ? right.as_float : (double)right.as_int;
                    return value_create_float(l - r);
                }
            }
            if (op == '*') {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_int(left.as_int * right.as_int);
                if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                    double l = (left.type == VAL_FLOAT) ? left.as_float : (double)left.as_int;
                    double r = (right.type == VAL_FLOAT) ? right.as_float : (double)right.as_int;
                    return value_create_float(l * r);
                }
            }
            if (op == '/') {
                if (left.type == VAL_INT && right.type == VAL_INT) {
                    if (right.as_int == 0) {
                        ksnprintf(interp->error, 512, "Dzielenie przez zero");
                        interp->has_error = true;
                        return value_create_null();
                    }
                    return value_create_int(left.as_int / right.as_int);
                }
            }
            if (op == '%') {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_int(left.as_int % right.as_int);
            }

            // Comparison operations
            if (strcmp(node->str_value, "==") == 0) {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_bool(left.as_int == right.as_int);
                return value_create_bool(false);
            }
            if (strcmp(node->str_value, "!=") == 0) {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_bool(left.as_int != right.as_int);
                return value_create_bool(true);
            }
            if (op == '<') {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_bool(left.as_int < right.as_int);
            }
            if (op == '>') {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_bool(left.as_int > right.as_int);
            }
            if (strcmp(node->str_value, "<=") == 0) {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_bool(left.as_int <= right.as_int);
            }
            if (strcmp(node->str_value, ">=") == 0) {
                if (left.type == VAL_INT && right.type == VAL_INT)
                    return value_create_bool(left.as_int >= right.as_int);
            }

            // Logical operations
            if (strcmp(node->str_value, "&&") == 0) {
                bool l = (left.type == VAL_BOOL) ? left.as_bool : (left.as_int != 0);
                bool r = (right.type == VAL_BOOL) ? right.as_bool : (right.as_int != 0);
                return value_create_bool(l && r);
            }
            if (strcmp(node->str_value, "||") == 0) {
                bool l = (left.type == VAL_BOOL) ? left.as_bool : (left.as_int != 0);
                bool r = (right.type == VAL_BOOL) ? right.as_bool : (right.as_int != 0);
                return value_create_bool(l || r);
            }

            value_free(&left);
            value_free(&right);
            return value_create_null();
        }

        case AST_UNARY_OP: {
            Value operand = interpreter_eval(interp, node->children[0]);
            if (node->str_value[0] == '-') {
                if (operand.type == VAL_INT)
                    return value_create_int(-operand.as_int);
                if (operand.type == VAL_FLOAT)
                    return value_create_float(-operand.as_float);
            }
            if (node->str_value[0] == '!') {
                bool val = (operand.type == VAL_BOOL) ? operand.as_bool : (operand.as_int != 0);
                return value_create_bool(!val);
            }
            value_free(&operand);
            return value_create_null();
        }

        case AST_PRINT: {
            for (int i = 0; i < node->child_count; i++) {
                Value val = interpreter_eval(interp, node->children[i]);
                value_print(&val);
                if (i < node->child_count - 1) kprintf(" ");
                value_free(&val);
            }
            kprintf("\n");
            return value_create_null();
        }

        case AST_IF: {
            Value cond = interpreter_eval(interp, node->children[0]);
            bool is_true = (cond.type == VAL_BOOL) ? cond.as_bool : (cond.as_int != 0);
            value_free(&cond);

            if (is_true) {
                return interpreter_eval(interp, node->children[1]);
            } else if (node->child_count > 2) {
                return interpreter_eval(interp, node->children[2]);
            }
            return value_create_null();
        }

        case AST_WHILE: {
            while (true) {
                Value cond = interpreter_eval(interp, node->children[0]);
                bool is_true = (cond.type == VAL_BOOL) ? cond.as_bool : (cond.as_int != 0);
                value_free(&cond);

                if (!is_true) break;

                interpreter_eval(interp, node->children[1]);

                if (interp->break_flag) {
                    interp->break_flag = false;
                    break;
                }
                if (interp->continue_flag) {
                    interp->continue_flag = false;
                    continue;
                }
                if (interp->return_flag) break;
            }
            return value_create_null();
        }

        case AST_RETURN: {
            if (node->child_count > 0) {
                interp->return_value = interpreter_eval(interp, node->children[0]);
            } else {
                interp->return_value = value_create_null();
            }
            interp->return_flag = true;
            return value_create_null();
        }

        case AST_BREAK:
            interp->break_flag = true;
            return value_create_null();

        case AST_CONTINUE:
            interp->continue_flag = true;
            return value_create_null();

        case AST_BLOCK: {
            for (int i = 0; i < node->child_count; i++) {
                interpreter_eval(interp, node->children[i]);
                if (interp->return_flag || interp->break_flag || interp->continue_flag)
                    break;
            }
            return value_create_null();
        }

        case AST_PROGRAM: {
            for (int i = 0; i < node->child_count; i++) {
                interpreter_eval(interp, node->children[i]);
                if (interp->has_error) break;
            }
            return value_create_null();
        }

        default:
            return value_create_null();
    }
}

void interpreter_execute(Interpreter *interp, const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);

    Parser parser;
    parser_init(&parser, &lexer);

    ASTNode *program = parser_parse_program(&parser);

    if (parser.has_error) {
        kprintf("[Blad parsowania] %s\n", parser.error);
        return;
    }

    if (program) {
        interpreter_eval(interp, program);

        if (interp->has_error) {
            kprintf("[Blad wykonania] %s\n", interp->error);
        }

        ast_node_free(program);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// REPL and High-Level API
// ═══════════════════════════════════════════════════════════════════════════

void polskiscript_eval_string(const char *source) {
    Interpreter interp;
    interpreter_init(&interp);
    interpreter_execute(&interp, source);
    interpreter_free(&interp);
}

void polskiscript_repl(void) {
    kprintf("\n");
    kprintf("╔══════════════════════════════════════════════════════════════╗\n");
    kprintf("║          PolskiScript REPL — Polski jezyk programowania     ║\n");
    kprintf("║                   PolandOS v0.0.1                           ║\n");
    kprintf("╚══════════════════════════════════════════════════════════════╝\n");
    kprintf("\nWpisz 'wyjdz' aby zakonczyc.\n\n");

    Interpreter interp;
    interpreter_init(&interp);

    char input[512];
    int pos = 0;

    while (true) {
        kprintf(">>> ");
        pos = 0;

        // Simple input reading (would need keyboard integration)
        // For now, just show the prompt
        // In a real implementation, would read from keyboard buffer

        break;  // Exit for now
    }

    interpreter_free(&interp);
}
