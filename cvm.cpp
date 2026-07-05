#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <cctype>
#include <unordered_map>
#include <fstream>
#include <chrono>
#include <stdexcept>
#include <array>

// Assigning OPCode standard names to identify operations easily
enum OpCode {
    OP_PUSH,        
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_EQUAL, OP_LESS,    
    OP_PRINT, OP_INPUT,   
    OP_SET_VAR, OP_GET_VAR,
    OP_JUMP_IF_FALSE, OP_JUMP, 
    OP_NEG,
    OP_HALT
};

// Assigning Token Type standard names to identify tokens easily
enum TokenType { 
    T_NUM, T_ID, T_LET, T_PRINT, T_INPUT,
    T_TRUE, T_FALSE, T_IF, T_ELSE, T_WHILE,
    T_PLUS, T_MINUS, T_MUL, T_DIV, 
    T_ASSIGN, T_EQEQ, T_LESS, 
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_SEMI, T_EOF, T_ERR 
};

// Creating a struct to hold token information
struct Token {
    std::string_view text;
    TokenType type;
    int value; 
    int line;
};

// Lexer
class Lexer {
    std::string_view source;
    size_t pos = 0;
    int currentLine = 1;

public:
    Lexer(std::string_view src) : source(src) {}

    Token nextToken() {
        while (pos < source.length() && isspace(source[pos])) {
            if (source[pos] == '\n') [[unlikely]] currentLine++;
            pos++;
        }
        if (pos >= source.length()) [[unlikely]] return {T_EOF, 0, "", currentLine};

        char c = source[pos];

        if (isdigit(c)) {
            int val = 0;
            size_t start = pos;
            while (pos < source.length() && isdigit(source[pos])) {
                val = val * 10 + (source[pos] - '0');
                pos++;
            }
            return {T_NUM, val, source.substr(start, pos - start), currentLine};
        }

        if (isalpha(c) || c == '_') {
            size_t start = pos;
            while (pos < source.length() && (isalnum(source[pos]) || source[pos] == '_')) {
                pos++;
            }
            std::string_view word = source.substr(start, pos - start);
            if (word == "let") return {T_LET, 0, word, currentLine};
            if (word == "print") return {T_PRINT, 0, word, currentLine};
            if (word == "input") return {T_INPUT, 0, word, currentLine};
            if (word == "true") return {T_TRUE, 1, word, currentLine};
            if (word == "false") return {T_FALSE, 0, word, currentLine};
            if (word == "if") return {T_IF, 0, word, currentLine};
            if (word == "else") return {T_ELSE, 0, word, currentLine};
            if (word == "while") return {T_WHILE, 0, word, currentLine};
            return {T_ID, 0, word, currentLine};
        }

        pos++;
        switch (c) {
            case '+': return {T_PLUS, 0, source.substr(pos-1, 1), currentLine};
            case '-': return {T_MINUS, 0, source.substr(pos-1, 1), currentLine};
            case '*': return {T_MUL, 0, source.substr(pos-1, 1), currentLine};
            case '/': return {T_DIV, 0, source.substr(pos-1, 1), currentLine};
            case '<': return {T_LESS, 0, source.substr(pos-1, 1), currentLine};
            case '=': 
                if (pos < source.length() && source[pos] == '=') {
                    pos++; return {T_EQEQ, 0, source.substr(pos-2, 2), currentLine};
                }
                return {T_ASSIGN, 0, source.substr(pos-1, 1), currentLine};
            case '(': return {T_LPAREN, 0, source.substr(pos-1, 1), currentLine};
            case ')': return {T_RPAREN, 0, source.substr(pos-1, 1), currentLine};
            case '{': return {T_LBRACE, 0, source.substr(pos-1, 1), currentLine};
            case '}': return {T_RBRACE, 0, source.substr(pos-1, 1), currentLine};
            case ';': return {T_SEMI, 0, source.substr(pos-1, 1), currentLine};
            default: return {T_ERR, 0, source.substr(pos-1, 1), currentLine};
        }
    }
};

enum class ASTNodeType {
    Program, Block, Num, Var, Assign, Input, BinOp, UnaryOp, Print, If, While
};

struct ASTArena {
    static constexpr size_t SIZE = 64 * 1024 * 1024; // 64 MB buffer for AST allocations
    inline static char buffer[SIZE];
    inline static size_t offset = 0;

    static void* allocate(size_t size) {
        size = (size + 7) & ~7; // 8-byte alignment
        if (offset + size > SIZE) [[unlikely]] {
            throw std::bad_alloc();
        }
        void* ptr = &buffer[offset];
        offset += size;
        return ptr;
    }

    static void reset() {
        offset = 0;
    }
};

struct ASTNode {
    ASTNodeType type;
    ASTNode(ASTNodeType t) : type(t) {}
    virtual void print(int indent) = 0;
    virtual ~ASTNode() = default;

    static void* operator new(size_t size) {
        return ASTArena::allocate(size);
    }
    static void operator delete(void* ptr) noexcept {
        // Managed by ASTArena::reset()
    }
};

struct NumNode : public ASTNode { 
    int value; 
    NumNode(int v) : ASTNode(ASTNodeType::Num), value(v) {} 
    void print(int indent) override { std::cout << std::string(indent, ' ') << "Num(" << value << ")\n"; }
};

struct VarNode : public ASTNode { 
    std::string_view name; 
    VarNode(std::string_view n) : ASTNode(ASTNodeType::Var), name(n) {} 
    void print(int indent) override { std::cout << std::string(indent, ' ') << "VarLoad(" << std::string(name) << ")\n"; }
};

struct InputNode : public ASTNode {
    InputNode() : ASTNode(ASTNodeType::Input) {}
    void print(int indent) override { std::cout << std::string(indent, ' ') << "Input()\n"; }
};

struct UnaryOpNode : public ASTNode {
    char op; std::unique_ptr<ASTNode> expr;
    UnaryOpNode(char o, std::unique_ptr<ASTNode> e) : ASTNode(ASTNodeType::UnaryOp), op(o), expr(std::move(e)) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "UnaryOp(" << op << ")\n";
        expr->print(indent + 2);
    }
};

struct BinOpNode : public ASTNode {
    std::string_view op; std::unique_ptr<ASTNode> left, right;
    BinOpNode(std::string_view o, std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r) : ASTNode(ASTNodeType::BinOp), op(o), left(std::move(l)), right(std::move(r)) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "BinOp(" << std::string(op) << ")\n";
        left->print(indent + 2);
        right->print(indent + 2);
    }
};

struct AssignNode : public ASTNode {
    std::string_view name; std::unique_ptr<ASTNode> expr;
    AssignNode(std::string_view n, std::unique_ptr<ASTNode> e) : ASTNode(ASTNodeType::Assign), name(n), expr(std::move(e)) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "Assign(" << std::string(name) << ")\n";
        expr->print(indent + 2);
    }
};

struct PrintNode : public ASTNode {
    std::unique_ptr<ASTNode> expr; 
    PrintNode(std::unique_ptr<ASTNode> e) : ASTNode(ASTNodeType::Print), expr(std::move(e)) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "PrintStmt\n";
        expr->print(indent + 2);
    }
};

struct BlockNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
    BlockNode() : ASTNode(ASTNodeType::Block) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "Block:\n";
        for (auto& stmt : statements) stmt->print(indent + 2);
    }
};

struct IfNode : public ASTNode {
    std::unique_ptr<ASTNode> condition, thenBranch, elseBranch;
    IfNode(std::unique_ptr<ASTNode> c, std::unique_ptr<ASTNode> t, std::unique_ptr<ASTNode> e) 
        : ASTNode(ASTNodeType::If), condition(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "If:\n";
        condition->print(indent + 4);
        thenBranch->print(indent + 4);
        if (elseBranch) elseBranch->print(indent + 4);
    }
};

struct WhileNode : public ASTNode {
    std::unique_ptr<ASTNode> condition, body;
    WhileNode(std::unique_ptr<ASTNode> c, std::unique_ptr<ASTNode> b) : ASTNode(ASTNodeType::While), condition(std::move(c)), body(std::move(b)) {}
    void print(int indent) override {
        std::cout << std::string(indent, ' ') << "While:\n";
        condition->print(indent + 4);
        body->print(indent + 4);
    }
};

struct ProgramNode : public ASTNode { 
    std::vector<std::unique_ptr<ASTNode>> statements; 
    ProgramNode() : ASTNode(ASTNodeType::Program) {}
    void print(int indent) override {
        for (auto& stmt : statements) stmt->print(indent + 2);
    }
};

class Parser {
    Lexer lexer;
    Token current;

    // --- Category A: Utilities & Helpers (Private) ---
    void advance() { current = lexer.nextToken(); }
    
    void consume(TokenType type, std::string_view errMsg) {
        if (current.type == type) [[likely]] advance();
        else [[unlikely]] { 
            throw std::runtime_error("Parse Error: " + std::string(errMsg) + " at line " + std::to_string(current.line) + " (Found: '" + std::string(current.text) + "')");
        }
    }

public:
    // --- Constructor ---
    Parser(std::string_view src) : lexer(src) { advance(); }

    // --- Category B: Program & Statement Structure (Top-Down) ---
    std::unique_ptr<ProgramNode> parseProgram() {
        auto prog = std::make_unique<ProgramNode>();
        while (current.type != T_EOF) prog->statements.push_back(parseStatement());
        return prog;
    }

    std::unique_ptr<ASTNode> parseStatement() {
        if (current.type == T_LET) {
            advance(); std::string_view varName = current.text; consume(T_ID, "Expected variable name");
            consume(T_ASSIGN, "Expected '='"); auto expr = parseExpression();
            consume(T_SEMI, "Expected ';'"); return std::make_unique<AssignNode>(varName, std::move(expr));
        }
        if (current.type == T_PRINT) {
            advance(); auto expr = parseExpression(); consume(T_SEMI, "Expected ';'");
            return std::make_unique<PrintNode>(std::move(expr));
        }
        if (current.type == T_IF) {
            advance();
            consume(T_LPAREN, "Expected '(' after 'if'"); auto cond = parseExpression(); consume(T_RPAREN, "Expected ')'");
            auto thenBranch = parseBlock();
            std::unique_ptr<ASTNode> elseBranch = nullptr;
            if (current.type == T_ELSE) { advance(); elseBranch = parseBlock(); }
            return std::make_unique<IfNode>(std::move(cond), std::move(thenBranch), std::move(elseBranch));
        }
        if (current.type == T_WHILE) {
            advance();
            consume(T_LPAREN, "Expected '(' after 'while'"); auto cond = parseExpression(); consume(T_RPAREN, "Expected ')'");
            return std::make_unique<WhileNode>(std::move(cond), parseBlock());
        }
        if (current.type == T_ID) {
             std::string_view varName = current.text; advance();
             consume(T_ASSIGN, "Expected '='"); auto expr = parseExpression();
             consume(T_SEMI, "Expected ';'"); return std::make_unique<AssignNode>(varName, std::move(expr));
        }
        throw std::runtime_error("Parse Error: Invalid statement at line " + std::to_string(current.line));
    }

    std::unique_ptr<BlockNode> parseBlock() {
        consume(T_LBRACE, "Expected '{'");
        auto block = std::make_unique<BlockNode>();
        while (current.type != T_RBRACE && current.type != T_EOF) {
            block->statements.push_back(parseStatement());
        }
        consume(T_RBRACE, "Expected '}'");
        return block;
    }

    // --- Category C: Expression Precedence Chain (Top-Down) ---
    std::unique_ptr<ASTNode> parseExpression() {
        auto left = parseMath();
        while (current.type == T_EQEQ || current.type == T_LESS) {
            std::string_view op = (current.type == T_EQEQ) ? "==" : "<";
            advance(); left = std::make_unique<BinOpNode>(op, std::move(left), parseMath());
        }
        return left;
    }

    std::unique_ptr<ASTNode> parseMath() {
        auto left = parseTerm();
        while (current.type == T_PLUS || current.type == T_MINUS) {
            std::string_view op = (current.type == T_PLUS) ? "+" : "-";
            advance(); left = std::make_unique<BinOpNode>(op, std::move(left), parseTerm());
        }
        return left;
    }

    std::unique_ptr<ASTNode> parseTerm() {
        auto left = parseUnary();
        while (current.type == T_MUL || current.type == T_DIV) {
            std::string_view op = (current.type == T_MUL) ? "*" : "/";
            advance(); left = std::make_unique<BinOpNode>(op, std::move(left), parseUnary());
        }
        return left;
    }

    std::unique_ptr<ASTNode> parseUnary() {
        if (current.type == T_MINUS) {
            advance(); return std::make_unique<UnaryOpNode>('-', parseUnary());
        }
        return parseFactor();
    }

    std::unique_ptr<ASTNode> parseFactor() {
        if (current.type == T_NUM || current.type == T_TRUE || current.type == T_FALSE) {
            auto node = std::make_unique<NumNode>(current.value);
            advance(); return node;
        }
        if (current.type == T_INPUT) {
            advance(); return std::make_unique<InputNode>();
        }
        if (current.type == T_ID) {
            auto node = std::make_unique<VarNode>(current.text);
            advance(); return node;
        }
        if (current.type == T_LPAREN) {
            advance(); auto node = parseExpression();
            consume(T_RPAREN, "Expected ')'"); return node;
        }
        throw std::runtime_error("Parse Error: Unexpected token at line " + std::to_string(current.line));
    }
};

class Compiler {
    std::unordered_map<std::string_view, int> varMap;
    int varCount = 0;

public:
    std::vector<int> bytecode;

    void compile(const ASTNode* node) {
        if (!node) return;

        switch (node->type) {
            // --- Category A: Structure & Flow (Scope Wrappers) ---
            case ASTNodeType::Program: {
                auto prog = static_cast<const ProgramNode*>(node);
                for (auto& stmt : prog->statements) compile(stmt.get());
                bytecode.push_back(OP_HALT);
                break;
            }
            case ASTNodeType::Block: {
                auto blk = static_cast<const BlockNode*>(node);
                for (auto& stmt : blk->statements) compile(stmt.get());
                break;
            }

            // --- Category B: Values & Variables (Identifiers & Literals) ---
            case ASTNodeType::Num: {
                auto n = static_cast<const NumNode*>(node);
                bytecode.push_back(OP_PUSH); bytecode.push_back(n->value);
                break;
            }
            case ASTNodeType::Var: {
                auto v = static_cast<const VarNode*>(node);
                if (varMap.find(v->name) == varMap.end()) [[unlikely]] {
                    throw std::runtime_error("Compile Error: Usage of undefined variable");
                }
                bytecode.push_back(OP_GET_VAR); bytecode.push_back(varMap[v->name]);
                break;
            }
            case ASTNodeType::Assign: {
                auto a = static_cast<const AssignNode*>(node);
                compile(a->expr.get());
                if (varMap.find(a->name) == varMap.end()) [[unlikely]] {
                    if (varCount >= 256) throw std::runtime_error("Compile Error: Maximum variable limit exceeded");
                    varMap[a->name] = varCount++;
                }
                bytecode.push_back(OP_SET_VAR); bytecode.push_back(varMap[a->name]);
                break;
            }
            case ASTNodeType::Input: {
                bytecode.push_back(OP_INPUT);
                break;
            }

            // --- Category C: Expressions & Operations (Math & Logic) ---
            case ASTNodeType::BinOp: {
                auto b = static_cast<const BinOpNode*>(node);
                compile(b->left.get()); compile(b->right.get());
                if (b->op == "+") bytecode.push_back(OP_ADD);
                if (b->op == "-") bytecode.push_back(OP_SUB);
                if (b->op == "*") bytecode.push_back(OP_MUL);
                if (b->op == "/") bytecode.push_back(OP_DIV);
                if (b->op == "==") bytecode.push_back(OP_EQUAL);
                if (b->op == "<") bytecode.push_back(OP_LESS);
                break;
            }
            case ASTNodeType::UnaryOp: {
                auto u = static_cast<const UnaryOpNode*>(node);
                compile(u->expr.get());
                if (u->op == '-') bytecode.push_back(OP_NEG);
                break;
            }

            // --- Category D: Output Execution ---
            case ASTNodeType::Print: {
                auto p = static_cast<const PrintNode*>(node);
                compile(p->expr.get()); bytecode.push_back(OP_PRINT);
                break;
            }

            // --- Category E: Conditional Jump Controls (The Jump Engines) ---
            case ASTNodeType::If: {
                auto ifN = static_cast<const IfNode*>(node);
                compile(ifN->condition.get());
                bytecode.push_back(OP_JUMP_IF_FALSE);
                int jumpFalseIdx = bytecode.size(); bytecode.push_back(0); 
                compile(ifN->thenBranch.get());
                bytecode.push_back(OP_JUMP);
                int jumpEndIdx = bytecode.size(); bytecode.push_back(0); 
                bytecode[jumpFalseIdx] = bytecode.size(); 
                if (ifN->elseBranch) compile(ifN->elseBranch.get());
                bytecode[jumpEndIdx] = bytecode.size();   
                break;
            }
            case ASTNodeType::While: {
                auto whl = static_cast<const WhileNode*>(node);
                int loopStart = bytecode.size();
                compile(whl->condition.get());
                bytecode.push_back(OP_JUMP_IF_FALSE);
                int jumpEndIdx = bytecode.size(); bytecode.push_back(0); 
                compile(whl->body.get());
                bytecode.push_back(OP_JUMP); bytecode.push_back(loopStart);
                bytecode[jumpEndIdx] = bytecode.size(); 
                break;
            }
        }
    }
};

class VM {
    std::vector<int> bytecode;
    std::array<int, 256> stack;
    int stackTopIdx = 0;
    std::array<int, 256> globals;

public:
    VM(std::vector<int> code) : bytecode(code) {}

    void run() {
        std::cout << "--- VM Execution Output ---\n";
        if (bytecode.empty()) return;

        const int* raw_ip = bytecode.data();
        int* raw_stack = stack.data();
        int* raw_globals = globals.data();

        // Computed Gotos / Direct Threading
        static void* dispatch_table[] = {
            &&do_OP_PUSH, &&do_OP_ADD, &&do_OP_SUB, &&do_OP_MUL, &&do_OP_DIV,
            &&do_OP_EQUAL, &&do_OP_LESS, &&do_OP_PRINT, &&do_OP_INPUT,
            &&do_OP_SET_VAR, &&do_OP_GET_VAR, &&do_OP_JUMP_IF_FALSE, &&do_OP_JUMP,
            &&do_OP_NEG, &&do_OP_HALT
        };

        #define DISPATCH() goto *dispatch_table[*raw_ip++]

        DISPATCH();

    do_OP_PUSH:
        if (stackTopIdx >= 256) [[unlikely]] throw std::runtime_error("VM Error: Stack Overflow");
        raw_stack[stackTopIdx++] = *raw_ip++;
        DISPATCH();
    do_OP_ADD:
        if (stackTopIdx < 2) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        stackTopIdx--;
        raw_stack[stackTopIdx - 1] += raw_stack[stackTopIdx];
        DISPATCH();
    do_OP_SUB:
        if (stackTopIdx < 2) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        stackTopIdx--;
        raw_stack[stackTopIdx - 1] -= raw_stack[stackTopIdx];
        DISPATCH();
    do_OP_MUL:
        if (stackTopIdx < 2) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        stackTopIdx--;
        raw_stack[stackTopIdx - 1] *= raw_stack[stackTopIdx];
        DISPATCH();
    do_OP_DIV: {
        if (stackTopIdx < 2) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        stackTopIdx--;
        int divisor = raw_stack[stackTopIdx];
        if (divisor == 0) [[unlikely]] throw std::runtime_error("VM Error: Division by zero");
        raw_stack[stackTopIdx - 1] /= divisor;
        DISPATCH();
    }
    do_OP_EQUAL:
        if (stackTopIdx < 2) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        stackTopIdx--;
        raw_stack[stackTopIdx - 1] = (raw_stack[stackTopIdx - 1] == raw_stack[stackTopIdx]) ? 1 : 0;
        DISPATCH();
    do_OP_LESS:
        if (stackTopIdx < 2) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        stackTopIdx--;
        raw_stack[stackTopIdx - 1] = (raw_stack[stackTopIdx - 1] < raw_stack[stackTopIdx]) ? 1 : 0;
        DISPATCH();
    do_OP_SET_VAR:
        if (stackTopIdx < 1) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        raw_globals[*raw_ip++] = raw_stack[--stackTopIdx];
        DISPATCH();
    do_OP_GET_VAR:
        if (stackTopIdx >= 256) [[unlikely]] throw std::runtime_error("VM Error: Stack Overflow");
        raw_stack[stackTopIdx++] = raw_globals[*raw_ip++];
        DISPATCH();
    do_OP_JUMP_IF_FALSE: {
        if (stackTopIdx < 1) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        int target = *raw_ip++;
        if (raw_stack[--stackTopIdx] == 0) raw_ip = bytecode.data() + target;
        DISPATCH();
    }
    do_OP_JUMP:
        raw_ip = bytecode.data() + *raw_ip;
        DISPATCH();
    do_OP_INPUT: {
        if (stackTopIdx >= 256) [[unlikely]] throw std::runtime_error("VM Error: Stack Overflow");
        int val; std::cout << "Input required: "; std::cin >> val;
        raw_stack[stackTopIdx++] = val;
        DISPATCH();
    }
    do_OP_PRINT:
        if (stackTopIdx < 1) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        std::cout << "> " << raw_stack[--stackTopIdx] << std::endl;
        DISPATCH();
    do_OP_NEG:
        if (stackTopIdx < 1) [[unlikely]] throw std::runtime_error("VM Error: Stack Underflow");
        raw_stack[stackTopIdx - 1] = -raw_stack[stackTopIdx - 1];
        DISPATCH();
    do_OP_HALT:
        return;
    }
};

int main(int argc, char* argv[]) {
    bool showAst = false, showBytecode = false;
    std::string filename = "";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--show-ast") showAst = true;
        else if (arg == "--show-bytecode") showBytecode = true;
        else filename = arg;
    }

    if (filename.empty()) {
        std::cout << "Required file name ./cvm_modern <script.cvm>\n"; 
        return 1;
    }

    std::ifstream file(filename);
    if (!file.is_open()) return 1;
    std::string sourceCode((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    try {
        Parser parser(sourceCode);
        auto ast = parser.parseProgram();
        
        if (showAst) ast->print(0);

        auto startCompile = std::chrono::high_resolution_clock::now();
        Compiler compiler;
        compiler.compile(ast.get());
        auto endCompile = std::chrono::high_resolution_clock::now();

        // Release AST memory and reset the compiler allocation arena
        ast.reset();
        ASTArena::reset();

        if (showBytecode) {
            std::cout << "--- Printing Compiled Bytecode ---\n";
            for (size_t i = 0; i < compiler.bytecode.size(); i++) {
                std::cout << compiler.bytecode[i] << " ";
            }
            std::cout << "\n-------------------------------------\n";
        }

        std::cout << "[Metrics] Compilation Time: " << std::chrono::duration_cast<std::chrono::microseconds>(endCompile - startCompile).count() << " microseconds\n";

        VM vm(compiler.bytecode);
        auto startVM = std::chrono::high_resolution_clock::now();
        vm.run();
        auto endVM = std::chrono::high_resolution_clock::now();
        std::cout << "[Metrics] Execution Time: " << std::chrono::duration_cast<std::chrono::microseconds>(endVM - startVM).count() << " microseconds\n";

    } catch (const std::exception& e) {
        std::cerr << "\n[Execution Halted] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
