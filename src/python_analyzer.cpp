#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <stack>
#include <cstring>

using namespace std;

// ============================================================================
// TOKEN TYPES
// ============================================================================
enum TokenType {
    TOK_KEYWORD, TOK_IDENTIFIER, TOK_NUMBER, TOK_STRING,
    TOK_OPERATOR, TOK_SYMBOL, TOK_INDENT, TOK_UNKNOWN
};

struct Token {
    string value;
    TokenType type;
    int line;
    int column;
};

// ============================================================================
// DIAGNOSTIC
// ============================================================================
enum DiagLevel { WARN_D, ERROR_D };
struct Diagnostic {
    DiagLevel level;
    int lineNumber;
    int column;
    string message;
    string recovery;
    string codeSnippet;
};

// ============================================================================
// SYMBOL TABLE
// ============================================================================
struct Symbol {
    string name;
    string inferredType;
    string value;
    int declaredLine;
    int usageCount;
    bool isParameter;
    bool isMutable;
    bool isDefined;
    Symbol() : declaredLine(0), usageCount(0), isParameter(false), 
               isMutable(false), isDefined(false) {}
};

struct ScopeInfo {
    string scopeName;
    string scopeType;
    int indentLevel;
    map<string, Symbol> symbols;
    vector<string> parameters;
    ScopeInfo* parent;
    vector<ScopeInfo*> children;
    
    ScopeInfo() : indentLevel(0), parent(NULL) {}
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

int getIndent(const string& line) {
    int c = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == ' ')  c++;
        else if (line[i] == '\t') c += 4;
        else break;
    }
    return c;
}

bool isKeyword(const string& w) {
    static set<string> kw;
    if (kw.empty()) {
        const char* k[] = {
            "if","else","elif","for","while","def","class","return",
            "import","from","as","pass","break","continue","lambda",
            "with","try","except","finally","raise","yield","global",
            "nonlocal","del","assert","and","or","not","in","is",
            "True","False","None"
        };
        for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i) kw.insert(k[i]);
    }
    return kw.count(w) > 0;
}

bool isBuiltin(const string& w) {
    static set<string> bi;
    if (bi.empty()) {
        const char* b[] = {
            "print","input","range","len","int","float","str","list",
            "dict","set","tuple","type","open","sum","max","min","abs",
            "enumerate","zip","map","filter","sorted","reversed","append",
            "extend","pop","get","items","keys","values","split","join",
            "strip","upper","lower","format","isinstance","hasattr"
        };
        for (size_t i = 0; i < sizeof(b) / sizeof(b[0]); ++i) bi.insert(b[i]);
    }
    return bi.count(w) > 0;
}

string tokenTypeName(TokenType t) {
    switch (t) {
        case TOK_KEYWORD:    return "keyword";
        case TOK_IDENTIFIER: return "identifier";
        case TOK_NUMBER:     return "number";
        case TOK_STRING:     return "string";
        case TOK_OPERATOR:   return "operator";
        case TOK_SYMBOL:     return "symbol";
        case TOK_INDENT:     return "indent";
        default:             return "unknown";
    }
}

string inferType(const string& val) {
    if (val.empty())  return "NoneType";
    if (val == "None")  return "NoneType";
    if (val == "True" || val == "False") return "bool";
    if ((val[0]=='"'  && val[val.size()-1]=='"') ||
        (val[0]=='\'' && val[val.size()-1]=='\'')) return "str";
    if (val[0] == '[') return "list";
    if (val[0] == '(' ) return "tuple";
    if (val[0] == '{' && val.find(':') != string::npos) return "dict";
    if (val[0] == '{' ) return "set";
    bool hasDot = false, isNum = true;
    for (size_t i = (val[0]=='-'?1:0); i < val.size(); ++i) {
        if (val[i] == '.') hasDot = true;
        else if (!isdigit((unsigned char)val[i])) { isNum = false; break; }
    }
    if (isNum && hasDot)  return "float";
    if (isNum && !hasDot && !val.empty()) return "int";
    return "object/ref";
}

// ============================================================================
// TOKENIZER
// ============================================================================
class Tokenizer {
private:
    string source;
    size_t pos;
    int lineNum;
    int columnNum;
    vector<Token> tokens;
    
public:
    Tokenizer(const string& src) : source(src), pos(0), lineNum(1), columnNum(1) {}
    
    vector<Token> tokenize() {
        while (pos < source.length()) {
            char current = source[pos];
            
            if (current == ' ' || current == '\t') {
                pos++; columnNum++;
                continue;
            }
            if (current == '\n') {
                pos++; lineNum++; columnNum = 1;
                continue;
            }
            if (current == '\r') {
                pos++; 
                if (pos < source.length() && source[pos] == '\n') {
                    pos++; lineNum++; columnNum = 1;
                }
                continue;
            }
            
            // Comments
            if (current == '#') {
                while (pos < source.length() && source[pos] != '\n') {
                    pos++;
                }
                continue;
            }
            
            Token tok;
            tok.line = lineNum;
            tok.column = columnNum;
            
            // Check for string prefixes (e.g. f"...", r"...", fr"...", etc.)
            size_t tempPos = pos;
            bool hasPrefix = false;
            if (isalpha((unsigned char)source[tempPos])) {
                char first = source[tempPos];
                char second = '\0';
                if (tempPos + 1 < source.length()) {
                    second = source[tempPos + 1];
                }
                
                bool isSingle = (first == 'f' || first == 'F' || first == 'r' || first == 'R' || 
                                 first == 'b' || first == 'B' || first == 'u' || first == 'U');
                bool isDouble = false;
                if (isSingle && second != '\0') {
                    isDouble = ((first == 'f' || first == 'F') && (second == 'r' || second == 'R')) ||
                               ((first == 'r' || first == 'R') && (second == 'f' || second == 'F')) ||
                               ((first == 'b' || first == 'B') && (second == 'r' || second == 'R')) ||
                               ((first == 'r' || first == 'R') && (second == 'b' || second == 'B'));
                }
                
                size_t quotePos = tempPos + (isDouble ? 2 : (isSingle ? 1 : 0));
                if (quotePos < source.length() && (source[quotePos] == '"' || source[quotePos] == '\'')) {
                    hasPrefix = true;
                }
            }

            // Strings
            if (current == '"' || current == '\'' || hasPrefix) {
                size_t quotePos = pos;
                while (source[quotePos] != '"' && source[quotePos] != '\'') {
                    quotePos++;
                }
                char quote = source[quotePos];
                
                while (pos <= quotePos) {
                    tok.value += source[pos];
                    pos++; columnNum++;
                }
                
                while (pos < source.length() && source[pos] != quote) {
                    if (source[pos] == '\n') {
                        tok.type = TOK_UNKNOWN;
                        tok.value = "UNTERMINATED_STRING";
                        tokens.push_back(tok);
                        return tokens;
                    }
                    if (source[pos] == '\\' && pos+1 < source.length()) {
                        tok.value += source[pos];
                        tok.value += source[pos+1];
                        pos += 2;
                        columnNum += 2;
                    } else {
                        tok.value += source[pos];
                        pos++; columnNum++;
                    }
                }
                
                if (pos < source.length()) {
                    tok.value += source[pos];
                    pos++; columnNum++;
                    tok.type = TOK_STRING;
                    tokens.push_back(tok);
                } else {
                    tok.type = TOK_UNKNOWN;
                    tok.value = "UNTERMINATED_STRING";
                    tokens.push_back(tok);
                }
                continue;
            }
            
            // Numbers
            if (isdigit((unsigned char)current) || (current == '-' && pos+1 < source.length() && isdigit((unsigned char)source[pos+1]))) {
                tok.value += current;
                pos++; columnNum++;
                bool hasDot = false;
                
                while (pos < source.length() && (isdigit((unsigned char)source[pos]) || source[pos] == '.')) {
                    if (source[pos] == '.') {
                        if (hasDot) break;
                        hasDot = true;
                    }
                    tok.value += source[pos];
                    pos++; columnNum++;
                }
                
                tok.type = TOK_NUMBER;
                tokens.push_back(tok);
                continue;
            }
            
            // Identifiers and keywords
            if (isalpha((unsigned char)current) || current == '_') {
                while (pos < source.length() && (isalnum((unsigned char)source[pos]) || source[pos] == '_')) {
                    tok.value += source[pos];
                    pos++; columnNum++;
                }
                
                if (isKeyword(tok.value)) {
                    tok.type = TOK_KEYWORD;
                } else {
                    tok.type = TOK_IDENTIFIER;
                }
                tokens.push_back(tok);
                continue;
            }
            
            // Operators
            string op = "";
            if (current == '=' || current == '+' || current == '-' || current == '*' ||
                current == '/' || current == '%' || current == '<' || current == '>' ||
                current == '!' || current == '&' || current == '|' || current == '^') {
                op += current;
                pos++; columnNum++;
                
                if (pos < source.length() && (source[pos] == '=' || source[pos] == '<' || source[pos] == '>')) {
                    op += source[pos];
                    pos++; columnNum++;
                }
                
                tok.value = op;
                tok.type = TOK_OPERATOR;
                tokens.push_back(tok);
                continue;
            }
            
            // Symbols
            if (current == '(' || current == ')' || current == '[' || current == ']' ||
                current == '{' || current == '}' || current == ',' || current == ':' ||
                current == '.' || current == ';') {
                tok.value += current;
                pos++; columnNum++;
                tok.type = TOK_SYMBOL;
                tokens.push_back(tok);
                continue;
            }
            
            // Unknown
            tok.value += current;
            pos++; columnNum++;
            tok.type = TOK_UNKNOWN;
            tokens.push_back(tok);
        }
        
        return tokens;
    }
};

// ============================================================================
// MAIN ANALYZER CLASS
// ============================================================================
class PythonAnalyzer {
private:
    vector<string> lines;
    vector<Token> allTokens;
    vector<Diagnostic> diagnostics;
    vector<ScopeInfo*> allScopes;
    ScopeInfo* currentScope;
    int scopeCounter;
    map<int, int> indentLevels;
    stack<ScopeInfo*> scopeStack;
    stack<int> indentStack;

    // ============================================================================
    // ERROR DETECTION FUNCTIONS
    // ============================================================================
    
    void addDiag(DiagLevel lv, int ln, int col, const string& msg, const string& rec="") {
        Diagnostic d; 
        d.level = lv; 
        d.lineNumber = ln; 
        d.column = col;
        d.message = msg; 
        d.recovery = rec;
        
        if (ln > 0 && ln <= (int)lines.size()) {
            d.codeSnippet = lines[ln-1];
        }
        diagnostics.push_back(d);
    }

    // --------------------------------------------------------------------------
    // f-string variable marking – all loops are index-based
    // --------------------------------------------------------------------------
    void markFStringVariables(const string& fstr) {
        size_t pos = fstr.find('{');
        while (pos != string::npos) {
            size_t end = fstr.find('}', pos);
            if (end == string::npos) break;
            string expr = fstr.substr(pos + 1, end - pos - 1);
            Tokenizer tok(expr);
            vector<Token> inner = tok.tokenize();
            for (size_t i = 0; i < inner.size(); ++i) {
                const Token& t = inner[i];
                if (t.type == TOK_IDENTIFIER) {
                    string name = t.value;
                    if (isBuiltin(name) || isKeyword(name) || name == "self") continue;
                    ScopeInfo* s = currentScope;
                    while (s) {
                        map<string, Symbol>::iterator it = s->symbols.find(name);
                        if (it != s->symbols.end()) {
                            it->second.usageCount++;
                            break;
                        }
                        s = s->parent;
                    }
                }
            }
            pos = fstr.find('{', pos + 1);
        }
    }

    // --------------------------------------------------------------------------
    // INDENTATION CHECK (fixed: multiple-of-4 is now a warning)
    // --------------------------------------------------------------------------
    void checkIndentation(const string& line, int lineNum, int prevIndent) {
        int currentIndent = getIndent(line);
        indentLevels[lineNum] = currentIndent;
        
        string clean = trim(line);
        if (clean.empty() || clean[0] == '#') return;
        
        // Now a style recommendation, not an error
        if (currentIndent % 4 != 0 && currentIndent > 0) {
            addDiag(WARN_D, lineNum, 0,
                "Indentation should be multiple of 4 spaces (style recommendation)",
                "Use 4 spaces per indentation level");
        }
        
        // Check for inconsistent indentation (dedent)
        if (currentIndent < indentStack.top()) {
            while (!indentStack.empty() && indentStack.top() > currentIndent) {
                indentStack.pop();
            }
            if (indentStack.empty() || indentStack.top() != currentIndent) {
                addDiag(ERROR_D, lineNum, 0,
                    "Inconsistent indentation",
                    "Indentation must match previous block level");
                indentStack.push(currentIndent);
            }
        } else if (currentIndent > indentStack.top()) {
            indentStack.push(currentIndent);
        }
        
        // Check for missing colon before indentation
        if (currentIndent > prevIndent && prevIndent >= 0) {
            int prevLine = lineNum - 1;
            while (prevLine > 0) {
                string prevClean = trim(lines[prevLine-1]);
                if (!prevClean.empty() && prevClean[0] != '#') {
                    if (prevClean[prevClean.size() - 1] != ':' && 
                        !(prevClean.find("for") == 0) &&
                        !(prevClean.find("if") == 0) &&
                        !(prevClean.find("while") == 0) &&
                        !(prevClean.find("def") == 0) &&
                        !(prevClean.find("class") == 0) &&
                        !(prevClean.find("try") == 0) &&
                        !(prevClean.find("except") == 0) &&
                        !(prevClean.find("elif") == 0) &&
                        !(prevClean.find("else") == 0) &&
                        !(prevClean.find("with") == 0)) {
                        addDiag(ERROR_D, lineNum, 0,
                            "Unexpected indentation",
                            "Add a colon ':' at the end of the previous line");
                        break;
                    }
                    break;
                }
                prevLine--;
            }
        }
    }

    void checkSyntaxErrors(const string& line, int lineNum) {
        string clean = trim(line);
        if (clean.empty() || clean[0] == '#') return;
        
        // Check for missing colon on block statements
        if ((clean.find("def ") == 0 || 
             clean.find("class ") == 0 ||
             clean.find("if ") == 0 ||
             clean.find("elif ") == 0 ||
             clean.find("else") == 0 ||
             clean.find("for ") == 0 ||
             clean.find("while ") == 0 ||
             clean.find("try:") == 0 ||
             clean.find("except") == 0 ||
             clean.find("with ") == 0)) {
            
            if (!clean.empty() && clean[clean.size() - 1] != ':') {
                size_t colonPos = clean.find(':');
                if (colonPos == string::npos) {
                    addDiag(ERROR_D, lineNum, 0,
                        "Missing colon ':' at end of statement",
                        "Add ':' at the end: " + clean + ":");
                }
            }
        }
        
        // Check for unmatched parentheses
        int parenBalance = 0;
        int bracketBalance = 0;
        int braceBalance = 0;
        
        for (size_t i = 0; i < clean.size(); ++i) {
            char c = clean[i];
            if (c == '(') parenBalance++;
            else if (c == ')') parenBalance--;
            else if (c == '[') bracketBalance++;
            else if (c == ']') bracketBalance--;
            else if (c == '{') braceBalance++;
            else if (c == '}') braceBalance--;
        }
        
        if (parenBalance > 0) {
            addDiag(ERROR_D, lineNum, 0,
                "Unmatched opening parenthesis '('",
                "Add closing parenthesis ')'");
        } else if (parenBalance < 0) {
            addDiag(ERROR_D, lineNum, 0,
                "Unmatched closing parenthesis ')'",
                "Remove extra ')' or add '('");
        }
        
        if (bracketBalance > 0) {
            addDiag(ERROR_D, lineNum, 0,
                "Unmatched opening bracket '['",
                "Add closing bracket ']'");
        } else if (bracketBalance < 0) {
            addDiag(ERROR_D, lineNum, 0,
                "Unmatched closing bracket ']'",
                "Remove extra ']' or add '['");
        }
        
        if (braceBalance > 0) {
            addDiag(ERROR_D, lineNum, 0,
                "Unmatched opening brace '{'",
                "Add closing brace '}'");
        } else if (braceBalance < 0) {
            addDiag(ERROR_D, lineNum, 0,
                "Unmatched closing brace '}'",
                "Remove extra '}' or add '{'");
        }
    }

    bool isAssignmentLHS(const vector<Token>& tokens, size_t index) {
        size_t eqIdx = string::npos;
        for (size_t j = 0; j < tokens.size(); ++j) {
            if (tokens[j].type == TOK_OPERATOR && tokens[j].value == "=") {
                eqIdx = j;
                break;
            }
        }
        if (eqIdx != string::npos && index < eqIdx) {
            return true;
        }
        return false;
    }

    bool isLoopVar(const vector<Token>& tokens, size_t index) {
        bool hasFor = false;
        for (size_t j = 0; j < index; ++j) {
            if (tokens[j].type == TOK_KEYWORD && tokens[j].value == "for") {
                hasFor = true;
                break;
            }
        }
        bool hasIn = false;
        for (size_t j = index + 1; j < tokens.size(); ++j) {
            if (tokens[j].type == TOK_KEYWORD && tokens[j].value == "in") {
                hasIn = true;
                break;
            }
        }
        return hasFor && hasIn;
    }

    void checkVariableDefinition(const string& line, int lineNum, ScopeInfo* scope) {
        string clean = trim(line);
        if (clean.empty() || clean[0] == '#') return;
        
        Tokenizer tokenizer(line);
        vector<Token> tokens = tokenizer.tokenize();
        
        // Scan for f-strings – use index loop
        for (size_t i = 0; i < tokens.size(); ++i) {
            const Token& tok = tokens[i];
            if (tok.type == TOK_STRING) {
                string val = tok.value;
                if (val.size() >= 2 && (val[0] == 'f' || val[0] == 'F')) {
                    markFStringVariables(val);
                }
            }
        }

        // Skip function definition lines, class definitions, and import/global lines
        if (!tokens.empty() && tokens[0].type == TOK_KEYWORD && 
            (tokens[0].value == "def" || tokens[0].value == "class" || 
             tokens[0].value == "import" || tokens[0].value == "from" ||
             tokens[0].value == "global" || tokens[0].value == "nonlocal")) {
            return;
        }
        
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i].type == TOK_IDENTIFIER) {
                string varName = tokens[i].value;
                
                if (isBuiltin(varName) || isKeyword(varName)) continue;
                if (varName == "self") continue;
                
                bool isAssignment = isAssignmentLHS(tokens, i);
                bool isForVar = isLoopVar(tokens, i);
                
                if (!isAssignment && !isForVar) {
                    bool isDefined = false;
                    ScopeInfo* s = scope;
                    while (s != NULL) {
                        if (s->symbols.find(varName) != s->symbols.end()) {
                            isDefined = true;
                            break;
                        }
                        s = s->parent;
                    }
                    
                    if (!isDefined) {
                        addDiag(ERROR_D, lineNum, tokens[i].column,
                            "Undefined variable: '" + varName + "'",
                            "Define '" + varName + "' before using it");
                    }
                }
            }
        }
    }

    void checkAssignmentErrors(const string& line, int lineNum) {
        string clean = trim(line);
        if (clean.empty() || clean[0] == '#') return;
        
        size_t eqPos = clean.find('=');
        if (eqPos != string::npos) {
            string lhs = trim(clean.substr(0, eqPos));
            
            if (!lhs.empty() && !isalpha((unsigned char)lhs[0]) && lhs[0] != '_') {
                addDiag(ERROR_D, lineNum, 0,
                    "Invalid assignment target: '" + lhs + "'",
                    "Use a valid variable name");
            }
            
            string rhs = trim(clean.substr(eqPos + 1));
            if (rhs.empty()) {
                addDiag(ERROR_D, lineNum, 0,
                    "Missing value in assignment",
                    "Provide a value: " + lhs + " = <value>");
            }
            
            if (isKeyword(lhs)) {
                addDiag(ERROR_D, lineNum, 0,
                    "Cannot assign to keyword: '" + lhs + "'",
                    "Use a different variable name");
            }
        }
    }

    void checkControlFlow(const string& line, int lineNum, bool& inLoop) {
        string clean = trim(line);
        if (clean.empty() || clean[0] == '#') return;
        
        if (clean.find("break") == 0 || clean.find("continue") == 0) {
            if (!inLoop) {
                addDiag(ERROR_D, lineNum, 0,
                    "'" + clean + "' outside loop",
                    "Use 'break' or 'continue' only inside loops");
            }
        }
    }

    void checkFunctionDefinition(const string& line, int lineNum) {
        string clean = trim(line);
        if (clean.empty() || clean[0] == '#') return;
        
        if (clean.find("def ") == 0) {
            size_t nameStart = clean.find("def ") + 4;
            size_t nameEnd = clean.find('(', nameStart);
            
            if (nameEnd == string::npos) {
                addDiag(ERROR_D, lineNum, 0,
                    "Invalid function definition",
                    "Use: def function_name(params):");
                return;
            }
            
            string funcName = trim(clean.substr(nameStart, nameEnd - nameStart));
            if (!funcName.empty() && !isalpha((unsigned char)funcName[0]) && funcName[0] != '_') {
                addDiag(ERROR_D, lineNum, 0,
                    "Invalid function name: '" + funcName + "'",
                    "Function names must start with a letter or underscore");
            }
            
            size_t paramStart = nameEnd + 1;
            size_t paramEnd = clean.find(')', paramStart);
            
            if (paramEnd == string::npos) {
                addDiag(ERROR_D, lineNum, 0,
                    "Unclosed parameter list",
                    "Add closing ')' for function parameters");
            }
        }
    }

    void checkImportErrors(const string& line, int lineNum) {
        string clean = trim(line);
        if (clean.empty() || clean[0] == '#') return;
        
        if (clean.find("import ") == 0) {
            string module = trim(clean.substr(6));
            if (module.empty()) {
                addDiag(ERROR_D, lineNum, 0,
                    "Missing module name in import",
                    "Use: import module_name");
            }
        }
        
        if (clean.find("from ") == 0) {
            size_t importPos = clean.find(" import ");
            if (importPos == string::npos) {
                addDiag(ERROR_D, lineNum, 0,
                    "Invalid from-import statement",
                    "Use: from module import name");
            }
        }
    }

    // ============================================================================
    // SYMBOL TABLE FUNCTIONS
    // ============================================================================
    
    void parseFunctionParams(const string& line, ScopeInfo* scope, int lineNum) {
        size_t op = line.find('('), cp = line.find(')');
        if (op == string::npos || cp == string::npos) return;
        string paramStr = line.substr(op+1, cp-op-1);
        if (paramStr.empty()) return;
        stringstream ss(paramStr);
        string p;
        while (getline(ss, p, ',')) {
            p = trim(p);
            size_t eq = p.find('=');
            string pname = (eq != string::npos) ? trim(p.substr(0,eq)) : p;
            string pval  = (eq != string::npos) ? trim(p.substr(eq+1)) : "?";
            size_t col = pname.find(':');
            if (col != string::npos) pname = trim(pname.substr(0,col));
            if (!pname.empty() && pname != "self") {
                Symbol sym;
                sym.name = pname;
                sym.value = (pval=="?") ? "<arg>" : pval;
                sym.inferredType = (pval=="?") ? "parameter" : inferType(pval);
                sym.declaredLine = lineNum;
                sym.isParameter = true;
                sym.isDefined = true;
                scope->symbols[sym.name] = sym;
                scope->parameters.push_back(pname);
            }
        }
    }

    void insertSymbol(const string& name, const string& val,
                      const string& type, int lineNum, bool isParam=false) {
        if (currentScope->symbols.count(name)) {
            currentScope->symbols[name].value = val;
            currentScope->symbols[name].inferredType = type;
            currentScope->symbols[name].isMutable = true;
            currentScope->symbols[name].usageCount++;
            currentScope->symbols[name].isDefined = true;
        } else {
            Symbol s;
            s.name = name; 
            s.value = val; 
            s.inferredType = type;
            s.declaredLine = lineNum; 
            s.isParameter = isParam;
            s.isDefined = true;
            currentScope->symbols[name] = s;
        }
    }

    string detectScopeType(const string& line) {
        if (line.size()>=3 && line.substr(0,3)=="def")   return "function";
        if (line.size()>=5 && line.substr(0,5)=="class") return "class";
        if (line.size()>=2 && line.substr(0,2)=="if")    return "conditional";
        if (line.size()>=4 && line.substr(0,4)=="elif")  return "conditional";
        if (line.size()>=4 && line.substr(0,4)=="else")  return "conditional";
        if (line.size()>=3 && line.substr(0,3)=="for")   return "loop";
        if (line.size()>=5 && line.substr(0,5)=="while") return "loop";
        if (line.size()>=3 && line.substr(0,3)=="try")   return "exception";
        if (line.size()>=6 && line.substr(0,6)=="except")return "exception";
        return "block";
    }

    void validateScopes() {
        for (size_t i = 0; i < allScopes.size(); ++i) {
            ScopeInfo* scope = allScopes[i];
            for (map<string, Symbol>::iterator it = scope->symbols.begin();
                 it != scope->symbols.end(); ++it) {
                Symbol& sym = it->second;
                if (sym.usageCount == 0 && !sym.isParameter) {
                    addDiag(WARN_D, sym.declaredLine, 0,
                        "Unused variable: '" + sym.name + "'",
                        "Consider removing or using this variable");
                }
            }
        }
    }

    // ============================================================================
    // DISPLAY HELPERS
    // ============================================================================
    void printLine(char c = '-', int w = 70) {
        cout << string(w, c) << "\n";
    }

    void printSection(const string& title) {
        printLine();
        cout << "  " << title << "\n";
        printLine();
    }

    // ============================================================================
    // DISPLAY FUNCTIONS
    // ============================================================================
    
    void displayTokenAnalysis(const string& code) {
        printLine('=', 70);
        cout << "  (i)  TOKEN ANALYSIS\n";
        printLine('=', 70);

        cout << "\n  Code:\n";
        stringstream ss(code);
        string ln;
        int lno = 1;
        while (getline(ss, ln)) {
            cout << "    " << setw(3) << lno++ << " | " << ln << "\n";
        }

        cout << "\n";
        printLine('-', 70);
        cout << "  Scanner reads tokens:\n";
        printLine('-', 70);
        cout << "\n";

        int prevLine = -1;
        for (size_t i = 0; i < allTokens.size(); ++i) {
            const Token& t = allTokens[i];
            if (t.line != prevLine) {
                if (prevLine != -1) cout << "\n";
                cout << "  [ Line " << t.line << " ]\n";
                prevLine = t.line;
            }
            cout << "    "
                      << left << setw(18) << t.value
                      << " -->  "
                      << tokenTypeName(t.type) 
                      << " (col " << t.column << ")\n";
        }
        cout << "\n";
    }

    void displaySymbolTables() {
        printLine('=', 70);
        cout << "  (ii) SYMBOL TABLES\n";
        printLine('=', 70);

        for (size_t i = 0; i < allScopes.size(); ++i) {
            ScopeInfo* sc = allScopes[i];
            cout << "\n";
            printLine('-', 70);
            cout << "  Scope  : " << sc->scopeName  << "\n";
            cout << "  Type   : " << sc->scopeType  << "\n";
            cout << "  Depth  : " << sc->indentLevel/4 << " level(s)\n";
            if (!sc->parameters.empty()) {
                cout << "  Params : ";
                for (size_t p = 0; p < sc->parameters.size(); ++p) {
                    cout << sc->parameters[p];
                    if (p+1 < sc->parameters.size()) cout << ", ";
                }
                cout << "\n";
            }
            printLine('-', 70);

            if (sc->symbols.empty()) {
                cout << "  [No variables declared in this scope]\n";
            } else {
                cout << left
                          << "  " << setw(16) << "Variable"
                          << setw(14) << "Type"
                          << setw(18) << "Value"
                          << setw(6)  << "Line"
                          << setw(6)  << "Uses"
                          << "Reassigned?\n";
                cout << "  " << string(66, '.') << "\n";

                for (map<string, Symbol>::iterator it = sc->symbols.begin();
                     it != sc->symbols.end(); ++it) {
                    const Symbol& s = it->second;
                    cout << "  "
                              << left << setw(16) << s.name
                              << setw(14) << s.inferredType
                              << setw(18) << s.value
                              << setw(6)  << s.declaredLine
                              << setw(6)  << s.usageCount
                              << (s.isMutable ? "Yes" : "No") << "\n";
                }
            }
        }
        cout << "\n";
    }

    void displayDiagnostics() {
        printLine('=', 70);
        cout << "  (iii) DIAGNOSTICS\n";
        printLine('=', 70);

        if (diagnostics.empty()) {
            cout << "\n  [No errors or warnings detected]\n\n";
            return;
        }

        vector<Diagnostic> errors, warnings;
        for (size_t i = 0; i < diagnostics.size(); ++i) {
            const Diagnostic& d = diagnostics[i];
            if (d.level == ERROR_D) errors.push_back(d);
            else warnings.push_back(d);
        }

        if (!errors.empty()) {
            cout << "\n  ERRORS (" << errors.size() << "):\n";
            printLine('-', 70);
            for (size_t i = 0; i < errors.size(); ++i) {
                const Diagnostic& d = errors[i];
                cout << "\n  [" << (i+1) << "] Line " << d.lineNumber;
                if (d.column > 0) cout << ", Col " << d.column;
                cout << "\n";
                cout << "      Error: " << d.message << "\n";
                if (!d.codeSnippet.empty()) {
                    cout << "      Code : " << trim(d.codeSnippet) << "\n";
                }
                if (!d.recovery.empty()) {
                    cout << "      Fix  : " << d.recovery << "\n";
                }
                cout << "  " << string(66, '.') << "\n";
            }
        }

        if (!warnings.empty()) {
            cout << "\n  WARNINGS (" << warnings.size() << "):\n";
            printLine('-', 70);
            for (size_t i = 0; i < warnings.size(); ++i) {
                const Diagnostic& d = warnings[i];
                cout << "\n  [" << (i+1) << "] Line " << d.lineNumber;
                if (d.column > 0) cout << ", Col " << d.column;
                cout << "\n";
                cout << "      Warning: " << d.message << "\n";
                if (!d.codeSnippet.empty()) {
                    cout << "      Code   : " << trim(d.codeSnippet) << "\n";
                }
                if (!d.recovery.empty()) {
                    cout << "      Fix    : " << d.recovery << "\n";
                }
                cout << "  " << string(66, '.') << "\n";
            }
        }
        cout << "\n";
    }

public:
    PythonAnalyzer(const string& script) : scopeCounter(0) {
        ScopeInfo* global = new ScopeInfo();
        global->scopeName = "global";
        global->scopeType = "global";
        global->indentLevel = 0;
        allScopes.push_back(global);
        currentScope = global;
        scopeStack.push(global);

        stringstream ss(script);
        string line;
        while (getline(ss, line)) lines.push_back(line);
    }

    ~PythonAnalyzer() {
        for (size_t i = 0; i < allScopes.size(); ++i) delete allScopes[i];
    }

    void analyze() {
        while (!indentStack.empty()) indentStack.pop();
        indentStack.push(0);

        // PASS 1: Tokenize all lines
        for (size_t i = 0; i < lines.size(); ++i) {
            string cl = trim(lines[i]);
            if (cl.empty() || cl[0]=='#') continue;
            
            Tokenizer tokenizer(lines[i]);
            vector<Token> toks = tokenizer.tokenize();
            for (size_t j = 0; j < toks.size(); ++j) {
                allTokens.push_back(toks[j]);
            }
        }

        // PASS 2: Comprehensive error checking
        bool inLoop = false;
        int prevIndent = 0;
        
        for (size_t i = 0; i < lines.size(); ++i) {
            string raw = lines[i];
            string clean = trim(raw);
            int lineNum = (int)i + 1;

            if (clean.empty() || clean[0] == '#') continue;

            checkIndentation(raw, lineNum, prevIndent);
            prevIndent = getIndent(raw);
            checkSyntaxErrors(raw, lineNum);
            checkAssignmentErrors(raw, lineNum);
            checkFunctionDefinition(raw, lineNum);
            checkImportErrors(raw, lineNum);
            checkControlFlow(raw, lineNum, inLoop);
            
            if (clean.find("for ") == 0 || clean.find("while ") == 0) {
                if (clean[clean.size() - 1] == ':') {
                    inLoop = true;
                }
            }
            if (clean == "else:" || clean == "elif ") {
                // else/elif don't start a new loop
            }
        }

        // PASS 3: Scope & symbol analysis
        for (size_t i = 0; i < lines.size(); ++i) {
            string raw = lines[i];
            string clean = trim(raw);
            int lineNum = (int)i + 1;

            if (clean.empty() || clean[0]=='#') continue;

            int indent = getIndent(raw);

            while (scopeStack.size() > 1 &&
                   indent <= scopeStack.top()->indentLevel) {
                scopeStack.pop();
            }
            currentScope = scopeStack.top();

            checkVariableDefinition(raw, lineNum, currentScope);

            // Block header
            if (!clean.empty() && clean[clean.size() - 1] == ':') {
                scopeCounter++;
                string stype = detectScopeType(clean);
                string label = clean.substr(0, clean.size()-1);
                if (label.size() > 30) label = label.substr(0,27) + "...";

                stringstream ss2;
                ss2 << "Block_" << scopeCounter << " [" << label << "]";

                ScopeInfo* ns = new ScopeInfo();
                ns->scopeName = ss2.str();
                ns->scopeType = stype;
                ns->indentLevel = indent + 4;
                ns->parent = currentScope;
                allScopes.push_back(ns);
                
                currentScope->children.push_back(ns);

                if (stype == "function")
                    parseFunctionParams(clean, ns, lineNum);

                scopeStack.push(ns);
                currentScope = ns;

                if (stype=="loop" && clean.size()>=3 && clean.substr(0,3)=="for") {
                    size_t fp = clean.find("for "), ip = clean.find(" in ");
                    if (fp != string::npos && ip != string::npos) {
                        string vpart = trim(clean.substr(fp+4, ip-fp-4));
                        stringstream sv(vpart);
                        string v;
                        while (getline(sv, v, ',')) {
                            v = trim(v);
                            if (!v.empty() && v != "self")
                                insertSymbol(v, "<loop_iter>", "loop_var", lineNum);
                        }
                    }
                }
                continue;
            }

            // Return
            if (clean.size()>=6 && clean.substr(0,6)=="return") {
                string rv = trim(clean.substr(6));
                insertSymbol("__return__", rv, inferType(rv), lineNum);
                continue;
            }

            // Import
            if ((clean.size()>=6 && clean.substr(0,6)=="import") ||
                (clean.size()>=4 && clean.substr(0,4)=="from")) {
                insertSymbol(clean, "<module>", "module", lineNum);
                continue;
            }

            // Assignment
            size_t eqPos = clean.find('=');
            if (eqPos != string::npos && eqPos > 0 &&
                eqPos+1 < clean.size() && clean[eqPos+1] != '=') {

                char prev = clean[eqPos-1];
                bool isAug = (prev=='+' || prev=='-' || prev=='*' ||
                              prev=='/' || prev=='%');
                string lhs = trim(clean.substr(0, isAug ? eqPos-1 : eqPos));
                string rhs = trim(clean.substr(eqPos+1));

                if (lhs.find(',') != string::npos) {
                    stringstream sv(lhs);
                    string v;
                    while (getline(sv, v, ',')) {
                        v = trim(v);
                        if (!v.empty() && (isalpha((unsigned char)v[0]) || v[0]=='_'))
                            insertSymbol(v, rhs, "unpacked", lineNum);
                    }
                } else if (!lhs.empty() && (isalpha((unsigned char)lhs[0]) || lhs[0]=='_') &&
                           lhs.find('.')==string::npos &&
                           lhs.find('[')==string::npos) {
                    string t = isAug ? "augmented" : inferType(rhs);
                    insertSymbol(lhs, rhs, t, lineNum);
                }
            }
        }

        validateScopes();
    }

    void display(const string& code) {
        cout << "\n";
        printLine('=', 70);
        cout << "   PYTHON SYMBOL TABLE ANALYZER  v3.0\n";
        cout << "   With Enhanced Error Detection\n";
        printLine('=', 70);
        cout << "\n";

        displayTokenAnalysis(code);
        displaySymbolTables();
        displayDiagnostics();

        printLine('=', 70);
        cout << "  Analysis Complete.\n";
        cout << "  Total Errors:   " << countErrors() << "\n";
        cout << "  Total Warnings: " << countWarnings() << "\n";
        printLine('=', 70);
        cout << "\n";
    }

    int countErrors() {
        int count = 0;
        for (size_t i = 0; i < diagnostics.size(); ++i) {
            if (diagnostics[i].level == ERROR_D) count++;
        }
        return count;
    }

    int countWarnings() {
        int count = 0;
        for (size_t i = 0; i < diagnostics.size(); ++i) {
            if (diagnostics[i].level == WARN_D) count++;
        }
        return count;
    }
};

// ============================================================================
// MAIN
// ============================================================================
void printBanner() {
    cout << "\n";
    cout << "======================================================================\n";
    cout << "        PYTHON ANALYZER v3.0 - Enhanced Error Detection\n";
    cout << "        Compiler Construction Project\n";
    cout << "======================================================================\n";
    cout << "  Features:\n";
    cout << "  - Syntax checking (colons, parentheses, brackets)\n";
    cout << "  - Variable definition validation\n";
    cout << "  - Indentation validation\n";
    cout << "  - Type inference and symbol tracking\n";
    cout << "  - Control flow validation (break/continue)\n";
    cout << "  - Unused variable detection\n";
    cout << "======================================================================\n";
    cout << "  Instructions:\n";
    cout << "  - Type or paste your Python code line by line\n";
    cout << "  - Type  END   on a new line to analyze\n";
    cout << "  - Type  HELP  to see an example\n";
    cout << "  - Type  EXIT  to quit\n";
    cout << "======================================================================\n";
}

void printHelp() {
    cout << "\n  Example code to try (with errors):\n";
    cout << "  ---------------------------------\n";
    cout << "  x = 10\n";
    cout << "  name = \"Alice\"\n";
    cout << "  items = [1, 2, 3]\n";
    cout << "  def greet(person, age=0):\n";
    cout << "      message = \"Hello\"\n";
    cout << "      return message\n";
    cout << "  for i in range(5)\n";
    cout << "      total = i * 2\n";
    cout << "  print(undefined_var)\n";
    cout << "  break\n";
    cout << "  ---------------------------------\n";
    cout << "  Then type END\n\n";
}

int main() {
    printBanner();

    while (true) {
        cout << "\n>>> Enter Python code (END to analyze, EXIT to quit):\n\n";
        string source = "", line;

        while (true) {
            cout << "  ";
            getline(cin, line);

            string up = line;
            for (size_t i = 0; i < up.length(); ++i) {
                up[i] = toupper((unsigned char)up[i]);
            }
            up = trim(up);

            if (up == "EXIT") {
                cout << "\n  Goodbye!\n\n";
                return 0;
            }
            if (up == "END")  break;
            if (up == "HELP") { printHelp(); continue; }

            source += line + "\n";
        }

        bool empty = true;
        for (size_t i = 0; i < source.size(); ++i) {
            if (!isspace((unsigned char)source[i])) { 
                empty = false; 
                break; 
            }
        }

        if (empty) {
            cout << "  [No code entered. Try again or type HELP]\n";
            continue;
        }

        PythonAnalyzer analyzer(source);
        analyzer.analyze();
        analyzer.display(source);
    }

    return 0;
}
