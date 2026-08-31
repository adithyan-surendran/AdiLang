#ifndef ADILANG_LEXER_H
#define ADILANG_LEXER_H

#include <string>
#include <vector>

enum class TokenType {
    // Literals
    IDENTIFIER,
    NUMBER,
    STRING,

    // Keywords
    LET,
    PRINT,
    IF,
    ELSE,
    WHILE,
    FOR,
    BREAK,
    CONTINUE,
    TRUE,
    FALSE,
    FN,
    RETURN,
    STRUCT,

    // Operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,

    EQUAL,
    PLUS_EQUAL,
    MINUS_EQUAL,
    STAR_EQUAL,
    SLASH_EQUAL,
    PERCENT_EQUAL,
    EQUAL_EQUAL,

    GREATER,
    LESS,
    GREATER_EQUAL,
    LESS_EQUAL,

    // Symbols
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    COMMA,
    SEMICOLON,
    DOT,
    // Logical Operators
    AND_AND,      // &&
    OR_OR,        // ||
    BANG,         // !
    BANG_EQUAL,   // !=

    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

class Lexer {
private:
    std::string source;
    std::vector<Token> tokens;

    size_t start = 0;
    size_t current = 0;

    int line = 1;
    int column = 1;

    void scanToken();
    void identifier();
    void number();
    void string();

    bool isAtEnd() const;
    char advance();
    char peek();
    char peekNext();
    bool match(char expected);

    void addToken(TokenType type);
    void addToken(TokenType type, std::string value);

public:
    Lexer(const std::string& source);

    std::vector<Token> scanTokens();
};

#endif