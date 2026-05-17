#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cctype>
#include <unordered_map>
#include <fstream>

using namespace std;

// ==========================================
// 1. ARCHITECTURE & OPCODES
// ==========================================
enum OpCode {
    OP_PUSH,        
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_EQUAL, OP_LESS,    // Relational
    OP_PRINT, OP_INPUT,   // I/O
    OP_SET_VAR, OP_GET_VAR,
    OP_JUMP_IF_FALSE, OP_JUMP, // Control Flow
    OP_HALT
};

// ==========================================
// 2. LEXER (Scanner)
// ==========================================
enum TokenType { 
    T_NUM, T_ID, T_LET, T_PRINT, T_INPUT,
    T_TRUE, T_FALSE, T_IF, T_ELSE, T_WHILE,
    T_PLUS, T_MINUS, T_MUL, T_DIV, 
    T_ASSIGN, T_EQEQ, T_LESS, 
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_SEMI, T_EOF, T_ERR 
};

struct Token {
    TokenType type;
    int value; 
    string text;
};

class Lexer {
    string source;
    size_t pos = 0;

public:
    Lexer(string src) : source(src) {}

    Token nextToken() {
        while (pos < source.length() && isspace(source[pos])) pos++;
        if (pos >= source.length()) return {T_EOF, 0, ""};

        char c = source[pos];

        if (isdigit(c)) {
            int val = 0;
            while (pos < source.length() && isdigit(source[pos])) {
                val = val * 10 + (source[pos] - '0');
                pos++;
            }
            return {T_NUM, val, ""};
        }

        if (isalpha(c) || c == '_') {
            string word = "";
            while (pos < source.length() && (isalnum(source[pos]) || source[pos] == '_')) {
                word += source[pos++];
            }
            if (word == "let") return {T_LET, 0, word};
            if (word == "print") return {T_PRINT, 0, word};
            if (word == "input") return {T_INPUT, 0, word};
            if (word == "true") return {T_TRUE, 1, word};
            if (word == "false") return {T_FALSE, 0, word};
            if (word == "if") return {T_IF, 0, word};
            if (word == "else") return {T_ELSE, 0, word};
            if (word == "while") return {T_WHILE, 0, word};
            return {T_ID, 0, word};
        }

        pos++;
        switch (c) {
            case '+': return {T_PLUS, 0, "+"};
            case '-': return {T_MINUS, 0, "-"};
            case '*': return {T_MUL, 0, "*"};
            case '/': return {T_DIV, 0, "/"};
            case '<': return {T_LESS, 0, "<"};
            case '=': 
                if (pos < source.length() && source[pos] == '=') {
                    pos++; return {T_EQEQ, 0, "=="};
                }
                return {T_ASSIGN, 0, "="};
            case '(': return {T_LPAREN, 0, "("};
            case ')': return {T_RPAREN, 0, ")"};
            case '{': return {T_LBRACE, 0, "{"};
            case '}': return {T_RBRACE, 0, "}"};
            case ';': return {T_SEMI, 0, ";"};
            default: return {T_ERR, 0, string(1, c)};
        }
    }
};

// ==========================================
// 3. ABSTRACT SYNTAX TREE (AST)
// ==========================================
struct ASTNode { 
    virtual void print(int indent) = 0;
    virtual ~ASTNode() = default; 
};

struct NumNode : public ASTNode { 
    int value; 
    NumNode(int v) : value(v) {} 
    void print(int indent) override { cout << string(indent, ' ') << "Num(" << value << ")\n"; }
};

struct VarNode : public ASTNode { 
    string name; 
    VarNode(string n) : name(n) {} 
    void print(int indent) override { cout << string(indent, ' ') << "VarLoad(" << name << ")\n"; }
};

struct InputNode : public ASTNode {
    void print(int indent) override { cout << string(indent, ' ') << "Input()\n"; }
};

struct BinOpNode : public ASTNode {
    string op; shared_ptr<ASTNode> left, right;
    BinOpNode(string o, shared_ptr<ASTNode> l, shared_ptr<ASTNode> r) : op(o), left(l), right(r) {}
    void print(int indent) override {
        cout << string(indent, ' ') << "BinOp(" << op << ")\n";
        left->print(indent + 2);
        right->print(indent + 2);
    }
};

struct AssignNode : public ASTNode {
    string name; shared_ptr<ASTNode> expr;
    AssignNode(string n, shared_ptr<ASTNode> e) : name(n), expr(e) {}
    void print(int indent) override {
        cout << string(indent, ' ') << "Assign(" << name << ")\n";
        expr->print(indent + 2);
    }
};

struct PrintNode : public ASTNode {
    shared_ptr<ASTNode> expr; 
    PrintNode(shared_ptr<ASTNode> e) : expr(e) {}
    void print(int indent) override {
        cout << string(indent, ' ') << "PrintStmt\n";
        expr->print(indent + 2);
    }
};

struct BlockNode : public ASTNode {
    vector<shared_ptr<ASTNode>> statements;
    void print(int indent) override {
        cout << string(indent, ' ') << "Block:\n";
        for (auto& stmt : statements) stmt->print(indent + 2);
    }
};

struct IfNode : public ASTNode {
    shared_ptr<ASTNode> condition, thenBranch, elseBranch;
    IfNode(shared_ptr<ASTNode> c, shared_ptr<ASTNode> t, shared_ptr<ASTNode> e) 
        : condition(c), thenBranch(t), elseBranch(e) {}
    
    void print(int indent) override {
        cout << string(indent, ' ') << "If:\n";
        cout << string(indent + 2, ' ') << "Condition:\n";
        condition->print(indent + 4);
        cout << string(indent + 2, ' ') << "Then:\n";
        thenBranch->print(indent + 4);
        if (elseBranch) {
            cout << string(indent + 2, ' ') << "Else:\n";
            elseBranch->print(indent + 4);
        }
    }
};

struct WhileNode : public ASTNode {
    shared_ptr<ASTNode> condition, body;
    WhileNode(shared_ptr<ASTNode> c, shared_ptr<ASTNode> b) : condition(c), body(b) {}
    
    void print(int indent) override {
        cout << string(indent, ' ') << "While:\n";
        cout << string(indent + 2, ' ') << "Condition:\n";
        condition->print(indent + 4);
        cout << string(indent + 2, ' ') << "Body:\n";
        body->print(indent + 4);
    }
};

struct ProgramNode : public ASTNode { 
    vector<shared_ptr<ASTNode>> statements; 
    void print(int indent) override {
        cout << "--- Abstract Syntax Tree ---\n";
        for (auto& stmt : statements) stmt->print(indent + 2);
        cout << "----------------------------\n";
    }
};

// ==========================================
// 4. PARSER
// ==========================================
class Parser {
    Lexer lexer; Token current;
    void advance() { current = lexer.nextToken(); }
    void consume(TokenType type, string errMsg) {
        if (current.type == type) advance();
        else { cerr << "Parse Error: " << errMsg << " (Found: " << current.text << ")\n"; exit(1); }
    }

public:
    Parser(string src) : lexer(src) { advance(); }

    shared_ptr<ASTNode> parseFactor() {
        if (current.type == T_NUM || current.type == T_TRUE || current.type == T_FALSE) {
            auto node = make_shared<NumNode>(current.value);
            advance(); return node;
        }
        if (current.type == T_INPUT) {
            advance(); return make_shared<InputNode>();
        }
        if (current.type == T_ID) {
            auto node = make_shared<VarNode>(current.text);
            advance(); return node;
        }
        if (current.type == T_LPAREN) {
            advance(); auto node = parseExpression();
            consume(T_RPAREN, "Expected ')'"); return node;
        }
        return nullptr;
    }

    shared_ptr<ASTNode> parseTerm() {
        auto left = parseFactor();
        while (current.type == T_MUL || current.type == T_DIV) {
            string op = (current.type == T_MUL) ? "*" : "/";
            advance(); left = make_shared<BinOpNode>(op, left, parseFactor());
        }
        return left;
    }

    shared_ptr<ASTNode> parseMath() {
        auto left = parseTerm();
        while (current.type == T_PLUS || current.type == T_MINUS) {
            string op = (current.type == T_PLUS) ? "+" : "-";
            advance(); left = make_shared<BinOpNode>(op, left, parseTerm());
        }
        return left;
    }

    shared_ptr<ASTNode> parseExpression() {
        auto left = parseMath();
        while (current.type == T_EQEQ || current.type == T_LESS) {
            string op = (current.type == T_EQEQ) ? "==" : "<";
            advance(); left = make_shared<BinOpNode>(op, left, parseMath());
        }
        return left;
    }

    shared_ptr<BlockNode> parseBlock() {
        consume(T_LBRACE, "Expected '{'");
        auto block = make_shared<BlockNode>();
        while (current.type != T_RBRACE && current.type != T_EOF) {
            block->statements.push_back(parseStatement());
        }
        consume(T_RBRACE, "Expected '}'");
        return block;
    }

    shared_ptr<ASTNode> parseStatement() {
        if (current.type == T_LET) {
            advance(); string varName = current.text; consume(T_ID, "Expected variable name");
            consume(T_ASSIGN, "Expected '='"); auto expr = parseExpression();
            consume(T_SEMI, "Expected ';'"); return make_shared<AssignNode>(varName, expr);
        }
        if (current.type == T_PRINT) {
            advance(); auto expr = parseExpression(); consume(T_SEMI, "Expected ';'");
            return make_shared<PrintNode>(expr);
        }
        if (current.type == T_IF) {
            advance();
            consume(T_LPAREN, "Expected '(' after 'if'"); auto cond = parseExpression(); consume(T_RPAREN, "Expected ')'");
            auto thenBranch = parseBlock();
            shared_ptr<ASTNode> elseBranch = nullptr;
            if (current.type == T_ELSE) { advance(); elseBranch = parseBlock(); }
            return make_shared<IfNode>(cond, thenBranch, elseBranch);
        }
        if (current.type == T_WHILE) {
            advance();
            consume(T_LPAREN, "Expected '(' after 'while'"); auto cond = parseExpression(); consume(T_RPAREN, "Expected ')'");
            return make_shared<WhileNode>(cond, parseBlock());
        }
        
        if (current.type == T_ID) {
             string varName = current.text; advance();
             consume(T_ASSIGN, "Expected '='"); auto expr = parseExpression();
             consume(T_SEMI, "Expected ';'"); return make_shared<AssignNode>(varName, expr);
        }
        return nullptr;
    }

    shared_ptr<ProgramNode> parseProgram() {
        auto prog = make_shared<ProgramNode>();
        while (current.type != T_EOF) prog->statements.push_back(parseStatement());
        return prog;
    }
};

// ==========================================
// 5. COMPILER 
// ==========================================
class Compiler {
    unordered_map<string, int> varMap;
    int varCount = 0;

public:
    vector<int> bytecode;

    void compile(shared_ptr<ASTNode> node) {
        if (auto n = dynamic_pointer_cast<NumNode>(node)) {
            bytecode.push_back(OP_PUSH); bytecode.push_back(n->value);
        } 
        else if (auto v = dynamic_pointer_cast<VarNode>(node)) {
            bytecode.push_back(OP_GET_VAR); bytecode.push_back(varMap[v->name]);
        }
        else if (dynamic_pointer_cast<InputNode>(node)) {
            bytecode.push_back(OP_INPUT);
        }
        else if (auto b = dynamic_pointer_cast<BinOpNode>(node)) {
            compile(b->left); compile(b->right);
            if (b->op == "+") bytecode.push_back(OP_ADD);
            if (b->op == "-") bytecode.push_back(OP_SUB);
            if (b->op == "*") bytecode.push_back(OP_MUL);
            if (b->op == "/") bytecode.push_back(OP_DIV);
            if (b->op == "==") bytecode.push_back(OP_EQUAL);
            if (b->op == "<") bytecode.push_back(OP_LESS);
        } 
        else if (auto a = dynamic_pointer_cast<AssignNode>(node)) {
            compile(a->expr);
            if (varMap.find(a->name) == varMap.end()) varMap[a->name] = varCount++;
            bytecode.push_back(OP_SET_VAR); bytecode.push_back(varMap[a->name]);
        }
        else if (auto p = dynamic_pointer_cast<PrintNode>(node)) {
            compile(p->expr); bytecode.push_back(OP_PRINT);
        }
        else if (auto blk = dynamic_pointer_cast<BlockNode>(node)) {
            for (auto& stmt : blk->statements) compile(stmt);
        }
        else if (auto ifN = dynamic_pointer_cast<IfNode>(node)) {
            compile(ifN->condition);
            bytecode.push_back(OP_JUMP_IF_FALSE);
            int jumpFalseIdx = bytecode.size(); bytecode.push_back(0); 
            
            compile(ifN->thenBranch);
            
            bytecode.push_back(OP_JUMP);
            int jumpEndIdx = bytecode.size(); bytecode.push_back(0); 
            
            bytecode[jumpFalseIdx] = bytecode.size(); 
            if (ifN->elseBranch) compile(ifN->elseBranch);
            bytecode[jumpEndIdx] = bytecode.size();   
        }
        else if (auto whl = dynamic_pointer_cast<WhileNode>(node)) {
            int loopStart = bytecode.size();
            compile(whl->condition);
            bytecode.push_back(OP_JUMP_IF_FALSE);
            int jumpEndIdx = bytecode.size(); bytecode.push_back(0); 
            
            compile(whl->body);
            bytecode.push_back(OP_JUMP); bytecode.push_back(loopStart);
            
            bytecode[jumpEndIdx] = bytecode.size(); 
        }
        else if (auto prog = dynamic_pointer_cast<ProgramNode>(node)) {
            for (auto& stmt : prog->statements) compile(stmt);
            bytecode.push_back(OP_HALT);
        }
    }
};

// ==========================================
// 6. VIRTUAL MACHINE
// ==========================================
class VM {
    vector<int> bytecode;
    int stack[256];
    int* stackTop;
    int globals[256];

    void push(int val) { *stackTop++ = val; }
    int pop() { return *(--stackTop); }

public:
    VM(vector<int> code) : bytecode(code) { stackTop = stack; }

    void run() {
        cout << "--- VM Execution Output ---\n";
        size_t ip = 0; 
        while (ip < bytecode.size()) {
            switch (bytecode[ip++]) {
                case OP_PUSH: push(bytecode[ip++]); break;
                case OP_ADD: { int b = pop(); int a = pop(); push(a + b); break; }
                case OP_SUB: { int b = pop(); int a = pop(); push(a - b); break; }
                case OP_MUL: { int b = pop(); int a = pop(); push(a * b); break; }
                case OP_DIV: { int b = pop(); int a = pop(); push(a / b); break; }
                case OP_EQUAL: { int b = pop(); int a = pop(); push(a == b ? 1 : 0); break; }
                case OP_LESS: { int b = pop(); int a = pop(); push(a < b ? 1 : 0); break; }
                case OP_SET_VAR: globals[bytecode[ip++]] = pop(); break;
                case OP_GET_VAR: push(globals[bytecode[ip++]]); break;
                case OP_JUMP_IF_FALSE: { int target = bytecode[ip++]; if (pop() == 0) ip = target; break; }
                case OP_JUMP: { ip = bytecode[ip++]; break; }
                case OP_INPUT: { 
                    int val; cout << "Input required: "; cin >> val; push(val); break; 
                }
                case OP_PRINT: cout << "> " << pop() << endl; break;
                case OP_HALT: return;
            }
        }
    }
};

// ==========================================
// 7. CLI RUNNER
// ==========================================
int main(int argc, char* argv[]) {
    bool showAst = false;
    bool showBytecode = false;
    string filename = "";

    // Parse command-line arguments safely
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--show-ast") showAst = true;
        else if (arg == "--show-bytecode") showBytecode = true;
        else filename = arg;
    }

    if (filename.empty()) {
        cout << "Usage: ./cvm [--show-ast] [--show-bytecode] <script.cvm>\n"; 
        return 1;
    }

    ifstream file(filename);
    if (!file.is_open()) { cerr << "Error opening file: " << filename << "\n"; return 1; }
    string sourceCode((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    // 1. Parsing Phase
    Parser parser(sourceCode);
    auto ast = parser.parseProgram();
    
    if (showAst) {
        ast->print(0);
    }

    // 2. Compilation Phase
    Compiler compiler;
    compiler.compile(ast);

    if (showBytecode) {
        cout << "--- Compiled Bytecode (Raw Array) ---\n";
        for (size_t i = 0; i < compiler.bytecode.size(); i++) {
            cout << compiler.bytecode[i] << " ";
        }
        cout << "\n-------------------------------------\n";
    }

    // 3. Execution Phase
    VM vm(compiler.bytecode);
    vm.run();

    return 0;
}