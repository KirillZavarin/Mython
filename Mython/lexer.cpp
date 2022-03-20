#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input) {
        std::string line;
        while (std::getline(input, line)) {
            std::string_view line_view(line);
            if (IgnoreLine(line_view)) {
                continue;
            }
            AddTokensFromString(line_view);
        }
        while (indentation_level_ > 0) {
            token_flow_.push_back(token_type::Dedent{});
            indentation_level_--;
        }
        token_flow_.push_back(token_type::Eof{});
        current_token = 0;
    }

    bool Lexer::IgnoreLine(std::string_view& line) const {
        if (line.size() == 0) {
            return true;
        }
        size_t pos = line.find_first_not_of(' ');
        if (pos == line.npos || line[pos] == '#') {
            return true;
        }
        return false;
    }

    void Lexer::AddTokensFromString(std::string_view& line) {
        AddIndent(line);

        std::string_view name_token;
        
        while(!IgnoreLine(line)) {
        //for (size_t i = 0; i < line.size(); i++) {
            if (line.front() == ' ') {       
                line.remove_prefix(line.find_first_not_of(' '));
                continue;
            }

            if (isdigit(line.front())) {
                AddNumber(line);
                continue;
            }

            if (isprint(line.front())) {
                AddSymvol(line);
                continue;
            }
        }

        token_flow_.push_back(parse::Token(parse::token_type::Newline{}));
    }

    void Lexer::AddIndent(std::string_view& line) {
        size_t lvl_indent = line.find_first_not_of(' ');

        if (lvl_indent % 2 != 0) {
            throw parse::LexerError("incorrect number of margins"s);
        }

        int delta_indentation = (lvl_indent / 2) - indentation_level_;

        for (int i = std::abs(delta_indentation); i > 0; i--) {
            token_flow_.push_back(delta_indentation > 0 ? parse::Token(parse::token_type::Indent{}) : parse::Token(parse::token_type::Dedent{}));
        }

        indentation_level_ = (lvl_indent / 2);

        line.remove_prefix(lvl_indent);
    }

    void Lexer::AddNumber(std::string_view& line) {
        std::istringstream ss = std::istringstream(std::string(line));
        int number = ReadNumber(ss);
        token_flow_.push_back(parse::Token(parse::token_type::Number{ number }));
        line.remove_prefix(std::to_string(number).size());
    }

    void Lexer::AddSymvol(std::string_view& line) {
        switch (line.front()) {
        case '=': case '!': case '<': case'>': {
            AddComparisonOperator(line);
            break;
            [[fallthrough]];
        }
        case '*': case '/': case '+': case '-': case '(': case ')':
        case ',': case '.': case ':': case ';': case '\t': case '\n': {
            token_flow_.push_back(token_type::Char{ line.front() });
            line.remove_prefix(1);
            break;
        }
        case '\'': case '\"': {
            AddString(line);
            break;
        }
        default: {
            AddId(line);
        }
        }
    }

    void Lexer::AddComparisonOperator(std::string_view& line) {
        switch (line.front()) {
        case '=': {
            if (line[1] == '=') {
                token_flow_.push_back(token_type::Eq{});
                line.remove_prefix(2);
            }
            else {
                token_flow_.push_back(token_type::Char{ '=' });
                line.remove_prefix(1);
            }
            break;
        }
        case '!': {
            if (line[1] == '=') {
                token_flow_.push_back(token_type::NotEq{});
                line.remove_prefix(2);
            }
            break;
        }
        case '<': {
            if (line[1] == '=') {
                token_flow_.push_back(token_type::LessOrEq{});
                line.remove_prefix(2);
            }
            else {
                token_flow_.push_back(token_type::Char{ '<' });
                line.remove_prefix(1);
            }
            break;
        }
        case '>': {
            if (line[1] == '=') {
                token_flow_.push_back(token_type::GreaterOrEq{});
                line.remove_prefix(2);
            }
            else {
                token_flow_.push_back(token_type::Char{ '>' });
                line.remove_prefix(1);
            }
            break;
        }
        }
    }

    void Lexer::AddString(std::string_view& line) {
        char type_quotation_marks = line.front();
        line.remove_prefix(1);
        size_t pos_end_string = 0u;
        std::string s = ""s;
        while (true) {
            const char ch = line[pos_end_string];
            if (ch == type_quotation_marks) {
                ++pos_end_string;
                break;
            }
            else if (ch == '\\') {
                ++pos_end_string;
                const char escaped_char = line[pos_end_string];
                switch (escaped_char) {
                case 'n':
                    s.push_back('\n');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case '"':
                    s.push_back('"');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                case '\'':
                    s.push_back('\'');
                    break;
                }
            }
            else {
                s.push_back(ch);
            }
            ++pos_end_string;
        }
        //std::string str(line.substr(0, pos_end_string));
        token_flow_.push_back(token_type::String{ s });
        line.remove_prefix(pos_end_string);
    }

    void Lexer::AddId(std::string_view& line) {
        size_t pos_end_id = 0;      

        while (true) {
            if (pos_end_id == line.size() || !(isalnum(line[pos_end_id]) || line[pos_end_id] == '_')) {
                break;
            }
            pos_end_id++;
        }

        std::string s = std::string(line.substr(0, pos_end_id));

        if (s == "class"s) {
            token_flow_.push_back(token_type::Class({}));
        }
        else if (s == "return"s) {
            token_flow_.push_back(token_type::Return({}));
        }
        else if (s == "if"s) {
            token_flow_.push_back(token_type::If({}));
        }
        else if (s == "else"s) {
            token_flow_.push_back(token_type::Else({}));
        }
        else if (s == "def"s) {
            token_flow_.push_back(token_type::Def({}));
        }
        else if (s == "print"s) {
            token_flow_.push_back(token_type::Print({}));
        }
        else if (s == "or"s) {
            token_flow_.push_back(token_type::Or({}));
        }
        else if (s == "None"s) {
            token_flow_.push_back(token_type::None({}));
        }
        else if (s == "and"s) {
            token_flow_.push_back(token_type::And({}));
        }
        else if (s == "not"s) {
            token_flow_.push_back(token_type::Not({}));
        }
        else if (s == "True"s) {
            token_flow_.push_back(token_type::True({}));
        }
        else if (s == "False"s) {
            token_flow_.push_back(token_type::False({}));
        }
        else {
            token_flow_.push_back(token_type::Id{ s });
        }
        line.remove_prefix(pos_end_id);
    }

    int Lexer::ReadNumber(std::istringstream& in) {
        int result;
        in >> result;
        return result;
    }

    const Token& Lexer::CurrentToken() const {
        return token_flow_[current_token];       
    }

    Token Lexer::NextToken() {
        current_token < token_flow_.size() - 1 ? current_token++ : current_token = (token_flow_.size() - 1);
        return CurrentToken();
    }

}  // namespace parse