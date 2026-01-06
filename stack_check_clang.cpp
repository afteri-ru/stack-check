#include "stack_check_clang.h"

#include "clang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Attr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"

#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "llvm/IR/Attributes.h"

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

#include "clang/Lex/PreprocessorOptions.h"
#include <charconv>
#include <string_view>

#pragma clang attribute push
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-this-capture"

#include "clang/AST/ASTDumper.h"

#pragma clang attribute pop

#include <string>

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace trust;

namespace {

/**
 * @def Stack check plugin
 *
 * The TrustPlugin class is made as a RecursiveASTVisitor template for the following reasons:
 * - AST traversal occurs from top to bottom through all leaves, which allows to dynamically create and clear context information.
 * Whereas when searching for matches using AST Matcher, MatchCallback only called for the found nodes,
 * but clearing the necessary context information for each call is very expensive.
 * - RecursiveASTVisitor allows interrupting (or repeating) traversal of individual AST node depending on dynamic context information.
 * - Matcher processes AST only for specific specified matchers and analysis of missed (not processed) attributes is difficult for it,
 * whereas RecursiveASTVisitor sequentially traverses all AST nodes regardless of the configured templates.
 *
 * The plugin is used as a static signleton to store context and simplify access from any classes,
 * but is created dynamically to use the context CompilerInstance
 *
 * Traverse ... - can form the current context
 * Visit ... - only analyze data without changing the context
 *
 */

class TrustPlugin;
static std::unique_ptr<TrustPlugin> plugin;
static bool is_verbose = false;

static void Verbose(SourceLocation loc, std::string_view str);

/**
 * @def TrustAttrInfo
 *
 * Class for applies a custom attribute to any declaration
 * or expression without examining the arguments
 * (only the number of arguments and their type are checked).
 */

#define ATTR_STACK_CHECK "stack_check_size"
#define ATTR_STACK_LIMIT "stack_check_limit"

static const std::string stack_check_size(ATTR_STACK_CHECK);
static const std::string stack_check_limit(ATTR_STACK_LIMIT);

struct TrustAttrInfo : public ParsedAttrInfo {

    TrustAttrInfo() {

        OptArgs = 0;
        NumArgs = 1;
        NumArgMembers = 1;

        IsType = false;
        IsStmt = false;
        IsKnownToGCC = false;
        IsSupportedByPragmaAttribute = false;

        static constexpr Spelling S[] = {
            {ParsedAttr::AS_GNU, ATTR_STACK_CHECK},
            {ParsedAttr::AS_C23, ATTR_STACK_CHECK},
            {ParsedAttr::AS_CXX11, ATTR_STACK_CHECK},
            {ParsedAttr::AS_CXX11, "::" ATTR_STACK_CHECK},
            {ParsedAttr::AS_CXX11, "::trust::" ATTR_STACK_CHECK},

            {ParsedAttr::AS_GNU, ATTR_STACK_LIMIT},
            {ParsedAttr::AS_C23, ATTR_STACK_LIMIT},
            {ParsedAttr::AS_CXX11, ATTR_STACK_LIMIT},
            {ParsedAttr::AS_CXX11, "::" ATTR_STACK_LIMIT},
            {ParsedAttr::AS_CXX11, "::trust::" ATTR_STACK_LIMIT},
        };
        Spellings = S;
    }

    std::string AttrStr(Sema &S, const ParsedAttr &attr) const {
        std::string result;
        SourceLocation loc = attr.getLoc();
        if (loc.isMacroID()) {
            result += attr.getNormalizedFullName();
        } else {
            result += Lexer::getSourceText(CharSourceRange::getTokenRange(attr.getRange()), S.getSourceManager(), S.getLangOpts());
        }
        return result;
    }

    AnnotateAttr *CreateAttr(Sema &S, const ParsedAttr &Attr) const {

        IntegerLiteral *literal = dyn_cast<IntegerLiteral>(Attr.getArgAsExpr(0)->IgnoreParenCasts());

        if (Attr.getNumArgs() != 1 || !literal) {
            S.Diag(Attr.getLoc(),
                   S.getDiagnostics().getCustomDiagID(DiagnosticsEngine::Error, "The attribute argument must be a single integer."));
            return nullptr;
        }

        std::string annotate(Attr.getAttrName()->getName());
        size_t stack_size = literal->getValue().getZExtValue();
        if (stack_size == 0 && annotate.compare(stack_check_size) == 0) {
            S.Diag(Attr.getLoc(),
                   S.getDiagnostics().getCustomDiagID(
                       DiagnosticsEngine::Error, "I couldn't find a way to call MachineFunctionPass from an AST plugin or as a regular IR "
                                                 "processing plugin, so automatic function stack size calculation is not supported yet."));
        }

        annotate += "=";
        annotate += std::to_string(stack_size);
        annotate += ";";
        return AnnotateAttr::Create(S.Context, annotate, Attr); // ArgsBuf.data(), ArgsBuf.size(), Attr.getRange());
    }

    AttrHandling handleDeclAttribute(Sema &S, Decl *D, const ParsedAttr &Attr) const override {

        if (auto attr = CreateAttr(S, Attr)) {
            D->addAttr(attr);
            if (isa<CXXMethodDecl>(D) || isa<FunctionDecl>(D)) {
                Verbose(Attr.getLoc(),
                        std::format("Apply attr '{}' to {}", AttrStr(S, Attr), dyn_cast<FunctionDecl>(D)->getQualifiedNameAsString()));
                return AttributeApplied;
            } else {
                auto DB = S.getDiagnostics().Report(
                    Attr.getLoc(),
                    S.getDiagnostics().getCustomDiagID(DiagnosticsEngine::Error, "The attribute '%0' for '%1' is not applicable."));
                DB.AddString(AttrStr(S, Attr));
                DB.AddString(D->getDeclKindName());
            }
        }
        return AttributeNotApplied;
    }
};

/*


*/

static std::string getCStringFromGlobal(const llvm::Value *V) {
    // ожидаем что это i8* на глобальную константу c"....\00"
    V = V->stripPointerCasts();

    auto *GV = dyn_cast<llvm::GlobalVariable>(V);
    if (!GV || !GV->hasInitializer())
        return {};

    auto *CA = dyn_cast<llvm::ConstantDataArray>(GV->getInitializer());
    if (!CA || !CA->isCString())
        return {};

    return CA->getAsCString().str();
}

static std::vector<std::string> getAnnotationsForValue(const llvm::Module &M, const llvm::Value *Target) {
    std::vector<std::string> Out;

    const llvm::GlobalVariable *GA = M.getNamedGlobal("llvm.global.annotations");
    if (!GA || !GA->hasInitializer())
        return Out;

    const auto *CA = dyn_cast<llvm::ConstantArray>(GA->getInitializer());
    if (!CA)
        return Out;

    for (const llvm::Use &Op : CA->operands()) {
        const auto *CS = dyn_cast<llvm::ConstantStruct>(Op.get());
        if (!CS || CS->getNumOperands() < 2)
            continue;

        // struct обычно вида: { i8* (ptr to annotated), i8* (ptr to annotation string), i8* file, i32 line, ... }
        const llvm::Value *Annotated = CS->getOperand(0)->stripPointerCasts();
        const llvm::Value *AnnStrPtr = CS->getOperand(1);

        if (Annotated == Target) {
            std::string S = getCStringFromGlobal(AnnStrPtr);
            if (!S.empty())
                Out.push_back(S);
        }
    }

    return Out;
}

class DebugInjectorPass : public llvm::PassInfoMixin<DebugInjectorPass> {
  public:
    llvm::PreservedAnalyses run(llvm::Module &Module, llvm::ModuleAnalysisManager &);

    static bool isRequired() { return true; }

  private:
    llvm::Function *FuncCheckSize(llvm::Module &Module);
    llvm::Function *FuncCheckLimit(llvm::Module &Module);
};

llvm::Function *DebugInjectorPass::FuncCheckSize(llvm::Module &Module) {
    llvm::Function *check_overflow = Module.getFunction("_ZN5trust11stack_check14check_overflowEm");
    if (check_overflow != nullptr) {
        return check_overflow;
    }

    llvm::LLVMContext &Context = Module.getContext();
    llvm::FunctionType *check_overflow_type =
        llvm::FunctionType::get(llvm::Type::getVoidTy(Context), llvm::Type::getInt64Ty(Context), false);
    llvm::FunctionCallee Callee = Module.getOrInsertFunction("_ZN5trust11stack_check14check_overflowEm", check_overflow_type);
    return llvm::cast<llvm::Function>(Callee.getCallee());
}

llvm::Function *DebugInjectorPass::FuncCheckLimit(llvm::Module &Module) {
    llvm::Function *check_limit = Module.getFunction("_ZN5trust11stack_check11check_limitEv");
    if (check_limit != nullptr) {
        return check_limit;
    }

    llvm::LLVMContext &Context = Module.getContext();
    llvm::FunctionType *check_limit_type = llvm::FunctionType::get(llvm::Type::getVoidTy(Context), false);
    llvm::FunctionCallee Callee = Module.getOrInsertFunction("_ZN5trust11stack_check11check_limitEv", check_limit_type);
    return llvm::cast<llvm::Function>(Callee.getCallee());
}

llvm::PreservedAnalyses DebugInjectorPass::run(llvm::Module &Module, llvm::ModuleAnalysisManager &) {

    llvm::Function *check_limit = FuncCheckLimit(Module);
    llvm::Function *check_size = FuncCheckSize(Module);
    size_t skip_injection = 0;

    bool Changed = false;
    for (llvm::Function &Function : Module) {
        if (Function.isDeclaration()) {
            continue;
        }
        for (llvm::BasicBlock &Block : Function) {
            for (llvm::BasicBlock::iterator DI = Block.begin(); DI != Block.end();) {
                // for (llvm::Instruction &Instruction : Block) {
                llvm::Instruction &Inst = *DI++;

                if (auto *Call = llvm::dyn_cast<llvm::CallBase>(&Inst)) {
                    if (llvm::Function *CurrentCallee = Call->getCalledFunction()) {

                        if (CurrentCallee->getName().compare("_ZN5trust11stack_check17ignore_next_checkEm") == 0) {

                            const llvm::Value *val = Call->getArgOperand(0);
                            if (auto *ci = dyn_cast<llvm::ConstantInt>(val)) {
                                skip_injection = ci->getZExtValue();
                                Verbose(SourceLocation(), std::format("Set skip injectioon to {}", skip_injection));
                            }

                            Inst.eraseFromParent();
                            Changed = true;
                            continue;
                        }

                        auto Ann = getAnnotationsForValue(Module, CurrentCallee);
                        if (!Ann.empty()) {
                            for (auto &S : Ann) {
                                if (S.find(stack_check_size) != std::string::npos) {
                                    size_t stack_size = atoi(&S[stack_check_size.size() + 1]);
                                    if (skip_injection) {
                                        Verbose(SourceLocation(), std::format("Code injection skipped {} for {}", skip_injection, S));
                                        skip_injection--;
                                    } else {
                                        llvm::IRBuilder<> Builder(Call->getContext());
                                        Builder.SetInsertPoint(Call);
                                        Builder.CreateCall(check_size, {Builder.getInt64(stack_size)});
                                        Changed = true;
                                    }
                                } else if (S.find(stack_check_limit) != std::string::npos) {
                                    if (skip_injection) {
                                        Verbose(SourceLocation(), std::format("Code injection skipped {} for {}", skip_injection, S));
                                        skip_injection--;
                                    } else {
                                        llvm::IRBuilder<> Builder(Call->getContext());
                                        Builder.SetInsertPoint(Call);
                                        Builder.CreateCall(check_limit);
                                        Changed = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (Changed) {
        return llvm::PreservedAnalyses::none();
    }
    return llvm::PreservedAnalyses::all();
}

// /*
//  *
//  *
//  */

enum LogLevel : uint8_t {
    INFO = 0,
    WARN = 1,
    ERR = 3,
};

class TrustPlugin : public RecursiveASTVisitor<TrustPlugin> {
  public:
    trust::StringMatcher m_dump_matcher;

    const CompilerInstance &m_CI;

    TrustPlugin(const CompilerInstance &instance) : m_CI(instance) {}

    inline clang::DiagnosticsEngine &getDiag() { return m_CI.getDiagnostics(); }

    template <typename T> static std::string makeHelperString(const T &map) {
        std::string result;
        for (auto elem : map) {
            if (!result.empty()) {
                result += "', '";
            }
            result += elem.first;
        }
        result.insert(0, "'");
        result += "'";
        return result;
    }

    static std::string makeHelperString(const std::set<std::string> &set) {
        std::string result;
        for (auto &elem : set) {

            if (!result.empty()) {
                result += "', '";
            }
            result += elem;
        }
        result.insert(0, "'");
        result += "'";
        return result;
    }

    static std::string unknownArgumentHelper(const std::string_view arg, const std::set<std::string> &set) {
        std::string result = "Unknown argument '";
        result += arg.begin();
        result += "'. Expected string argument from the following list: ";
        result += makeHelperString(set);
        return result;
    }

    /*
     *
     * RecursiveASTVisitor Traverse... template methods for the created plugin context
     *
     *
     *
     *
     *
     */

#define TRAVERSE_CONTEXT(name)                                                                                                             \
    bool Traverse##name(name *arg) {                                                                                                       \
        RecursiveASTVisitor<TrustPlugin>::Traverse##name(arg);                                                                             \
        return true;                                                                                                                       \
    }

    TRAVERSE_CONTEXT(MemberExpr);
    TRAVERSE_CONTEXT(CallExpr);
    TRAVERSE_CONTEXT(CXXMemberCallExpr);

    /*
     * Creating a plugin context for classes
     */

    bool TraverseCXXRecordDecl(CXXRecordDecl *decl) {

        if (decl->hasDefinition()) {
        }

        return RecursiveASTVisitor<TrustPlugin>::TraverseCXXRecordDecl(decl);
    }

    /*
     * All AST analysis starts with TraverseDecl
     *
     */
    bool TraverseDecl(Decl *D) {

        if (const FunctionDecl *func = dyn_cast_or_null<FunctionDecl>(D)) {
            if (func->isDefined()) {
            }
        }

        return RecursiveASTVisitor<TrustPlugin>::TraverseDecl(D);
    }
}; // namespace

/*
 *
 *
 *
 *
 */

class TrustPluginASTConsumer : public ASTConsumer {
  public:
    void HandleTranslationUnit(ASTContext &context) override {
        context.getParentMapContext().setTraversalKind(clang::TraversalKind::TK_IgnoreUnlessSpelledInSource);
        plugin->TraverseDecl(context.getTranslationUnitDecl());
    }
};

/*
 *
 *
 *
 *
 */

class TrustPluginASTAction : public PluginASTAction {
  public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) override {

        std::unique_ptr<TrustPluginASTConsumer> obj = std::unique_ptr<TrustPluginASTConsumer>(new TrustPluginASTConsumer());

        Compiler.getCodeGenOpts().PassPlugins.push_back("stack_check_clang.so");

        return obj;
    }

    template <class... Args> void PrintColor(raw_ostream &out, std::format_string<Args...> fmt, Args &&...args) {

        out << "\033[1;46;34m";
        out << std::format(fmt, args...);
        out << "\033[0m\n";
    }

    bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {

        plugin = std::unique_ptr<TrustPlugin>(new TrustPlugin(CI));

        llvm::outs().SetUnbuffered();
        llvm::errs().SetUnbuffered();

        std::string first;
        std::string second;
        size_t pos;
        for (auto &elem : args) {
            pos = elem.find("=");
            if (pos != std::string::npos) {
                first = elem.substr(0, pos);
                second = elem.substr(pos + 1);
            } else {
                first = elem;
                second = "";
            }

            // std::string message = plugin->processArgs(first, second, SourceLocation());

            // if (!message.empty()) {
            if (first.compare("verbose") == 0 || first.compare("v") == 0) {
                is_verbose = true;
                PrintColor(llvm::outs(), "Enable verbose mode");
            } else {
                llvm::errs() << "Unknown plugin argument: '" << elem << "'!\n";
                return false;
            }
            // } else {
            //     if (first.compare("level") == 0) {
            //         // ok
            //     } else {
            //         llvm::errs() << "The argument '" << elem << "' is not supported via command line!\n";
            //         return false;
            //     }
            // }
        }
        return true;
    }
};

void Verbose(SourceLocation loc, std::string_view msg) {
    if (is_verbose && plugin) {

        std::string str = loc.printToString(plugin->m_CI.getSourceManager());
        size_t pos = str.find(' ');
        if (pos == std::string::npos) {
            llvm::outs() << str;
        }
        llvm::outs() << str.substr(0, pos);

        llvm::outs() << ": verbose: " << msg.begin() << "\n";
    }
}

} // namespace

static ParsedAttrInfoRegistry::Add<TrustAttrInfo> A("stack_check", "Checking stack overflow attribute");
static FrontendPluginRegistry::Add<TrustPluginASTAction> S("stack_check", "Checking stack overflow plugin");

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    static ::llvm::PassPluginLibraryInfo PluginInfo = {
        LLVM_PLUGIN_API_VERSION, "stack_check", "0.1", [](::llvm::PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](::llvm::ModulePassManager &MPM, ::llvm::OptimizationLevel) { MPM.addPass(DebugInjectorPass()); });
        }};
    return PluginInfo;
}
