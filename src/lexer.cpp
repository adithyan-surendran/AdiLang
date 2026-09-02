#include "lexer.h"
#include <cctype>
#include <iostream>

Lexer::Lexer(const std::string& source)
    : source(source) {
}

std::vector<Token> Lexer::scanTokens() {

    while (current < source.length()) {

        start = current;
        scanToken();
    }

    tokens.push_back({
        TokenType::END_OF_FILE,
        "",
        line,
        column
    });
    return tokens;
}

char Lexer::advance() {

    char c = source[current++];

    if (c == '\n') {
        line++;
        column = 1;
    }
    else {
        column++;
    }

    return c;
}

char Lexer::peek() {

    if (current >= source.length())
        return '\0';

    return source[current];
}

char Lexer::peekNext() {

    if (current + 1 >= source.length())
        return '\0';

    return source[current + 1];
}

bool Lexer::match(char expected) {

    if (peek() != expected)
        return false;

    current++;
    return true;
}

void Lexer::addToken(TokenType type) {

    std::string text =
        source.substr(start, current - start);

    tokens.push_back({
        type,
        text,
        line,
        column
    });
}

void Lexer::addToken(
    TokenType type,
    std::string value
) {

    tokens.push_back({
        type,
        value,
        line,
        column
    });
}   
void Lexer::scanToken() {

    char c = advance();

    switch (c) {

        case '(':
            addToken(TokenType::LEFT_PAREN);
            break;

        case ')':
            addToken(TokenType::RIGHT_PAREN);
            break;

        case '{':
            addToken(TokenType::LEFT_BRACE);
            break;

        case '}':
            addToken(TokenType::RIGHT_BRACE);
            break;

        case '[':
            addToken(TokenType::LEFT_BRACKET);
            break;
            
        case ']':
            addToken(TokenType::RIGHT_BRACKET);
            break;

        case ',':
            addToken(TokenType::COMMA);
            break;

        case ';':
            addToken(TokenType::SEMICOLON);
            break;

        case '+':
            addToken(match('=') ? TokenType::PLUS_EQUAL : TokenType::PLUS);
            break;

        case '-':
            addToken(match('=') ? TokenType::MINUS_EQUAL : TokenType::MINUS);
            break;

        case '*':
            addToken(match('=') ? TokenType::STAR_EQUAL : TokenType::STAR);
            break;

        case '/':
            if (match('=')) {
                addToken(TokenType::SLASH_EQUAL);
            } else if (match('/')) {
                while (peek() != '\n' && !isAtEnd()) advance();
            } else {
                addToken(TokenType::SLASH);
            }
            break;

        case '%':
            addToken(match('=') ? TokenType::PERCENT_EQUAL : TokenType::PERCENT);
            break;

        case '=':

            if (match('=')) {
                addToken(TokenType::EQUAL_EQUAL);
            } else {
                addToken(TokenType::EQUAL);
            }

            break;

        case '>':

            if (match('=')) {
                addToken(TokenType::GREATER_EQUAL);
            } else {
                addToken(TokenType::GREATER);
            }

            break;
        

        case '<':

            if (match('=')) {
                addToken(TokenType::LESS_EQUAL);
            } else {
                addToken(TokenType::LESS);
            }

            break;

        case '"':
            string();
            break;

        case ' ':
        case '\r':
        case '\t':
        case '\n':
            // Ignore whitespace
            break;
        case '!':
            if (match('=')) {
                addToken(TokenType::BANG_EQUAL);
            } else {
                addToken(TokenType::BANG);
            }
            break;

        case '&':
            if (match('&')) {
                addToken(TokenType::AND_AND);
            } else {
                std::cerr << "Error at line " << line << ": Unexpected character '&'\n";
            }
            break;

        case '|':
            if (match('|')) {
                addToken(TokenType::OR_OR);
            } else {
                std::cerr << "Error at line " << line << ": Unexpected character '|'\n";
            }
            break;
            
        case '.':
            addToken(TokenType::DOT);
            break;

        default:

            if (std::isdigit(c)) {

                number();

            }
            else if (std::isalpha(c) || c == '_') {

                identifier();

            }
            else {

                std::cerr
                    << "Error at line "
                    << line
                    << ", column "
                    << column
                    << ": Unexpected character '"
                    << c
                    << "'\n";
            }

            break;
    }
}
void Lexer::number() {

    while (std::isdigit(peek())) {
        advance();
    }

    // Decimal part
    if (peek() == '.' &&
        std::isdigit(peekNext())) {

        advance();

        while (std::isdigit(peek())) {
            advance();
        }
    }

    addToken(TokenType::NUMBER);
}
void Lexer::string() {

    while (peek() != '"' &&
           peek() != '\0') {

        advance();
    }

    if (peek() == '"')
        advance();

    std::string value =
        source.substr(start + 1,
                      current - start - 2);

    addToken(TokenType::STRING, value);
}
void Lexer::identifier() {

    while (std::isalnum(peek()) ||
           peek() == '_') {

        advance();
    }

    std::string text =
        source.substr(start,
                      current - start);

    if (text == "let")
        addToken(TokenType::LET);

    else if (text == "print")
        addToken(TokenType::PRINT);

    else if (text == "if")
        addToken(TokenType::IF);

    else if (text == "else")
        addToken(TokenType::ELSE);

    else if (text == "while")           
        addToken(TokenType::WHILE);

    else if (text == "for")          
        addToken(TokenType::FOR);

    else if (text == "break")          
        addToken(TokenType::BREAK);

    else if (text == "continue")          
        addToken(TokenType::CONTINUE);

    else if (text == "fn")
        addToken(TokenType::FN);

    else if (text == "return")
        addToken(TokenType::RETURN);
        
    else if (text == "true")
        addToken(TokenType::TRUE);

    else if (text == "false")
        addToken(TokenType::FALSE);

    else if (text == "struct")
        addToken(TokenType::STRUCT);

    else if (text == "super")
        addToken(TokenType::SUPER);

    else
        addToken(TokenType::IDENTIFIER);
}
bool Lexer::isAtEnd() const {
    return current >= source.length();
}