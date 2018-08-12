#ifndef SGF_H_
#define SGF_H_

#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <istream>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace sgf {

/**
 * ...
 */
enum class ValueType { NUMBER, REAL, DOUBLE, COLOR, NONE, TEXT, SIMPLE_TEXT };

/**
 * ...
 */
class Value {
 public:
  Value(const std::string& v, ValueType t) : value_(v), type_(t) {}
  ValueType type() const { return type_; }
  const std::string& raw() const { return value_; }

 protected:
  std::string value_;
  ValueType type_;
};

/**
 * ...
 */
class Number : public Value {
 public:
  explicit Number(const std::string& v) : Value(v, ValueType::NUMBER) {}
  int parse() { return std::stoi(value_); }
};

/**
 * ...
 */
class Real : public Value {
 public:
  explicit Real(const std::string& v) : Value(v, ValueType::REAL) {}
  float parse() { return std::stof(value_); }
};

/**
 * ...
 */
class Non : public Value {
 public:
  explicit Non(const std::string& v) : Value(v, ValueType::NONE) {}
  std::string parse() { return value_; }
};

std::regex soft(R"(\\(?:\n\r?|\r\n?))");
std::regex white(R"(\s)");
std::regex escaped(R"(\\([\S\s]))");
std::regex escape(R"(([\]\\]))");

/**
 * ...
 */
class SimpleText : public Value {
 public:
  explicit SimpleText(const std::string& v)
      : Value(v, ValueType::SIMPLE_TEXT) {}
  std::string parse() {
    std::string temp = value_;
    // remove soft linebreaks
    temp = std::regex_replace(temp, soft, "");
    // convert white spaces other than linebreaks to space
    temp = std::regex_replace(temp, white, " ");
    // insert escaped chars verbatim
    temp = std::regex_replace(temp, escaped, "$1");
    return std::regex_replace(temp, escape, "\\$1");
  }
};

/**
 * ...
 */
class Text : public Value {
 public:
  explicit Text(const std::string& v) : Value(v, ValueType::TEXT) {}
  std::string parse() {
    // TODO: formatting and escaping
    return value_;
  }
};

/**
 * ...
 */
class Double : public Value {
 public:
  explicit Double(const std::string& v) : Value(v, ValueType::DOUBLE) {}
  int parse() { return std::stoi(value_); }
};

/**
 * ...
 */
class Color : public Value {
 public:
  explicit Color(const std::string& v) : Value(v, ValueType::COLOR) {}
  std::string parse() { return value_; }
};

/**
 * ...
 */
class Node {
 public:
  void add(const std::string& key, std::string& prop) {
    properties_[key].push_back(prop);
  };

  bool has(const std::string& key) {
    return properties_.find(key) != properties_.end();
  };

  std::vector<std::string> get(const std::string& key) {
    return properties_[key];
  };

  int size() { return properties_.size(); };

 private:
  std::map<std::string, std::vector<std::string>> properties_;
};

/**
 * ...
 */
class GameTree {
 public:
  GameTree(){};
  //GameTree(const GameTree& b){ nodes_ = b.nodes_; variations_ = b.variations_;};
  void addNode(Node node) { nodes_.push_back(node); };
  void addVariation(GameTree game) { variations_.push_back(game); };
  bool hasVariations() { return variations_.size() != 0; };

  std::vector<Node> search(const std::string& property);
  std::vector<Node>& nodes() { return nodes_; };
  std::vector<GameTree>& variations() { return variations_; };
  //~GameTree(){};
 private:
  std::vector<Node> nodes_;
  std::vector<GameTree> variations_;
};

namespace internal {

/**
 * ...
 */
#undef ERROR

enum class TokenType {
		ERROR,
		END_OF_FILE,
		SEMI_COLON,
		LPAREN,
		RPAREN,
		LBRACKET,
		RBRACKET,
		ESCAPE,
		VALUE
};

/**
 * ...
 */
class Token {
 public:
  Token(TokenType type) : type_(type) {}
  Token(TokenType type, char c) : type_(type), value_(std::string(1, c)) {}

  TokenType type() const { return type_; }
  const std::string& value() const { return value_; }

 private:
  TokenType type_;
  std::string value_;
};

/**
 * ...
 */
class Lexer {
 public:
  explicit Lexer(std::istream& is) : is_(is), lineNo_(1) {}

  /**
   * ...
   */
  Token nextToken() {
    char c;
    while (current(&c)) {
      next();
      switch (c) {
        case '(':
          return Token(TokenType::LPAREN);
        case ')':
          return Token(TokenType::RPAREN);
        case '[':
          return Token(TokenType::LBRACKET);
        case ']':
          return Token(TokenType::RBRACKET);
        case ';':
          return Token(TokenType::SEMI_COLON);
        case '\\':
          return Token(TokenType::ESCAPE);
        default:
          return Token(TokenType::VALUE, c);
      }
    }
    return Token(TokenType::END_OF_FILE);
  }

  /**
   * ...
   */
  std::string nextKey() {
    std::string key;
    char c;

    consumeWhile(isspace);
    while (current(&c) && isalnum(c)) {
      key += c;
      next();
    }
    consumeWhile(isspace);

    return key;
  }

  /**
   * ...
   */
  int consumeWhile(std::function<int(int)> f) {
    char x;
    int n = 0;
    while (current(&x) && f(x)) {
      n++;
      next();
    }
    return n;
  }

  int lineNo() const { return lineNo_; }

 private:
  /**
   * ...
   */
  bool current(char* c) {
    int x = is_.peek();
    if (x == EOF) {
      return false;
    }
    *c = static_cast<char>(x);
    return true;
  }

  /**
   * ...
   */
  void next() {
    int x = is_.get();
    if (x == '\n') {
      ++lineNo_;
    }
  }

  /**
   * ...
   */
  bool consume(char c) {
    char x;
    if (! current(&x) || x != c) {
      return false;
    }
    next();
    return true;
  }

  std::istream& is_;
  int lineNo_;
};

/**
 * ...
 */
class SGFParser {
 public:
  /**
   * ...
   */
  explicit SGFParser(std::istream& is) : lexer_(is), token_(TokenType::ERROR) {
    nextToken();
  }

  /**
   * ...
   */
  std::vector<GameTree> parse() {
    std::vector<GameTree> collection;
    while (token_.type() != TokenType::END_OF_FILE) {
      lexer_.consumeWhile(isspace);
      if (token_.type() == TokenType::LPAREN) {
        nextToken();
        GameTree g = parseGame();
        if (g.nodes().size() > 0) {
          collection.push_back(g);
        } else {
          return collection;
        }
      } else {
        break;
      }
    }
    return collection;
  }

 private:
  /**
   * ...
   */
  void throw_parse_exception(const std::string& err) {
    throw std::runtime_error{err + " at line " +
                             std::to_string(lexer_.lineNo())};
  }

  /**
   * ...
   */
  const Token& token() const { return token_; }

  /**
   * ...
   */
  void nextToken() { token_ = lexer_.nextToken(); }

  /**
   * ...
   */
  std::string nextKey() {
    std::string key = lexer_.nextKey();
    nextToken();
    return key;
  }

  /**
   * ...
   */
  GameTree parseGame() {
    GameTree game;
    while (token_.type() != TokenType::END_OF_FILE) {
      lexer_.consumeWhile(isspace);
      TokenType tt = token_.type();
      if (tt == TokenType::SEMI_COLON) {
        // We've found the start of a node.
        //std::cout << "Starting a node ..." << '\n';
        if (game.hasVariations()) {
          throw_parse_exception("A node was encountered after a variation!");
        }
        game.addNode(parseNode());
      } else if (tt == TokenType::LPAREN) {
        // We've found the start of a variation.
        //std::cout << "Starting a variation ..." << '\n';
        nextToken();
        parseVariations(game);
      } else {
        // We've found the end of a game.
        //std::cout << "Ending a game ..." << '\n';
        //std::cout << "GAME == " << game.nodes().size() << '\n';
        nextToken();
        return game;
      }
    }
    return game;
  }

  /**
   * ...
   */
  Node parseNode() {
    Node node;
    while (token_.type() != TokenType::END_OF_FILE) {
      std::string key = nextKey();
      if (token_.type() == TokenType::LBRACKET) {
        //std::cout << "KEY == " << key << '\n';
        for (auto& prop : parseProps()) {
          //std::cout << "ADDING " << prop << '\n';
          node.add(key, prop);
        }
      } else {
        break;
      }
    }
    //std::cout << "VALUE == " << node.size() << '\n';
    return node;
  }

  /**
   * ...
   */
  void parseVariations(GameTree& game) {
    while (token_.type() != TokenType::END_OF_FILE) {
      lexer_.consumeWhile(isspace);
      if (token_.type() == TokenType::RPAREN) {
        return;
      }
      game.addVariation(parseGame());
      nextToken();
    }
  }

  /**
   * ...
   */
  std::vector<std::string> parseProps() {
    std::vector<std::string> props;
    while (token_.type() != TokenType::END_OF_FILE) {
      lexer_.consumeWhile(isspace);
       if (token_.type() == TokenType::LBRACKET) {
        std::string prop;
        bool laste = false;
        while (laste || token_.type() != TokenType::RBRACKET) {
          prop += token_.value();
          laste = token_.type() == TokenType::ESCAPE;
          nextToken();
        }
        props.push_back(prop);
      } else {
        break;
      }
    }

    if (props.size() == 0) {
      throw_parse_exception("empty property");
    }
    return props;
  }

  Lexer lexer_;
  Token token_;
};

}  // namespace internal

/**
 * ...
 */
inline std::vector<GameTree> parse(const std::string& input_file) {
  std::ifstream ifs(input_file);
  if (! ifs.good()) {
    throw std::runtime_error{"bad input path"};
  }
  internal::SGFParser parser(ifs);
  return parser.parse();
}

}  // namespace sgf

#endif  // SGF_H_
