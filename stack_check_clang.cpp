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
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/OptimizationLevel.h"
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

static std::string LocToStr(const SourceLocation &loc, const SourceManager &sm) {
    std::string str = loc.printToString(sm);
    size_t pos = str.find(' ');
    if (pos == std::string::npos) {
        return str;
    }
    return str.substr(0, pos);
}

/**
 * @def TrustAttrInfo
 *
 * Class for applies a custom attribute to any declaration
 * or expression without examining the arguments
 * (only the number of arguments and their type are checked).
 */

struct TrustAttrInfo : public ParsedAttrInfo {

    #define ATTR_STACK_CHECK "stack_check"
    #define ATTR_STACK_CHECK_LIMIT "stack_check_limit"

    TrustAttrInfo() {

        OptArgs = 3;

        static constexpr Spelling S[] = {
            {ParsedAttr::AS_GNU, ATTR_STACK_CHECK},
            {ParsedAttr::AS_C23, ATTR_STACK_CHECK},
            {ParsedAttr::AS_CXX11, ATTR_STACK_CHECK},
            {ParsedAttr::AS_CXX11, "::" ATTR_STACK_CHECK},

            {ParsedAttr::AS_GNU, ATTR_STACK_CHECK_LIMIT},
            {ParsedAttr::AS_C23, ATTR_STACK_CHECK_LIMIT},
            {ParsedAttr::AS_CXX11, ATTR_STACK_CHECK_LIMIT},
            {ParsedAttr::AS_CXX11, "::" ATTR_STACK_CHECK_LIMIT},
        };
        Spellings = S;
    }

    AnnotateAttr *CreateAttr(Sema &S, const ParsedAttr &Attr) const {

        if (Attr.getNumArgs()) {
            S.Diag(Attr.getLoc(),
                   S.getDiagnostics().getCustomDiagID(
                       DiagnosticsEngine::Error, "The attribute '" TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE) "' does not support arguments."));

            return nullptr;
        }

        return AnnotateAttr::Create(S.Context, TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE), nullptr, 0, Attr.getRange());
    }

    AttrHandling handleDeclAttribute(Sema &S, Decl *D, const ParsedAttr &attr) const override {

        if (const CXXMethodDecl *method = dyn_cast<CXXMethodDecl>(D)) {
            Verbose(attr.getLoc(),
                    std::format("Apply attr '" TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE) "' to {}", method->getQualifiedNameAsString()));
        } else if (const FunctionDecl *func = dyn_cast<FunctionDecl>(D)) {
            Verbose(attr.getLoc(),
                    std::format("Apply attr '" TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE) "' to {}", func->getQualifiedNameAsString()));
        } else {

            auto DB = S.getDiagnostics().Report(
                attr.getLoc(),
                S.getDiagnostics().getCustomDiagID(
                    DiagnosticsEngine::Error, "The attribute '" TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE) "' for '%0' is not applicable."));
            DB.AddString(D->getDeclKindName());

            // S.Diag(attr.getLoc(), S.getDiagnostics().getCustomDiagID(
            //         DiagnosticsEngine::Error,
            //         "The attribute [[" TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE) "]] for '%0' is not applicable."));
            //         // .AddString(D->getDeclKindName();

            return AttributeNotApplied;
        }
        D->addAttr(CreateAttr(S, attr));
        return AttributeApplied;
    }

    AttrHandling handleStmtAttribute(Sema &S, Stmt *St, const ParsedAttr &attr, class Attr *&Result) const override {

        St->dump();

        S.Diag(attr.getLoc(),
               S.getDiagnostics().getCustomDiagID(DiagnosticsEngine::Error,
                                                  "The attribute '" TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE) "' is not applicable."));

        return AttributeNotApplied;
    }
};



// struct FunctionTracePass : public llvm::PassInfoMixin<FunctionTracePass> {
//     llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
//         bool modified = false;

//         // Объявляем отладочную функцию
//         llvm::LLVMContext &context = M.getContext();
//         llvm::Type *voidType = llvm::Type::getVoidTy(context);
//         llvm::Type *ptrType = llvm::PointerType::get(context, 0);
//         std::vector<llvm::Type *> paramTypes = {ptrType, ptrType};
//         llvm::FunctionType *debugFuncType = llvm::FunctionType::get(voidType, paramTypes, false);

//         llvm::FunctionCallee debugFunc = M.getOrInsertFunction("__cyg_profile_func_enter", debugFuncType);
//         llvm::Function *debugFunction = llvm::cast<llvm::Function>(debugFunc.getCallee());

//         // Делаем функцию external
//         debugFunction->setLinkage(llvm::GlobalValue::ExternalLinkage);

//         // Проходим по всем функциям в модуле
//         for (auto &function : M) {
//             // Пропускаем объявления и нашу отладочную функцию
//             if (function.isDeclaration() || &function == debugFunction)
//                 continue;

//             // Проходим по всем базовым блокам
//             for (auto &block : function) {
//                 // Создаем временный вектор инструкций для обработки
//                 std::vector<llvm::Instruction *> callInstructions;

//                 // Собираем все вызовы функций
//                 for (auto &inst : block) {
//                     if (auto *cb = llvm::dyn_cast<llvm::CallBase>(&inst)) {
//                         callInstructions.push_back(cb);
//                     }
//                 }

//                 // Вставляем отладочные вызовы перед каждым вызовом функции
//                 for (auto inst : callInstructions) {
//                     auto *cb = llvm::cast<llvm::CallBase>(inst);
//                     llvm::Value *calledOperand = cb->getCalledOperand();
//                     llvm::Value *calledStripped = calledOperand->stripPointerCasts();

//                     if (auto *calleeFunc = llvm::dyn_cast<llvm::Function>(calledStripped)) {
//                         // Пропускаем внутренние и отладочную
//                         if (calleeFunc->isIntrinsic() || calleeFunc == debugFunction)
//                             continue;
//                     }

//                     // Создаем параметры для отладочной функции
//                     std::vector<llvm::Value *> args;
//                     llvm::IRBuilder<> builder(inst);

//                     // Функция
//                     llvm::Value *funcPtr = builder.CreateBitCast(calledOperand, ptrType);
//                     // Вместо адреса инструкции (для void вызовов это не pointer type) используем адрес текущей функции
//                     llvm::Value *callSitePtr = builder.CreateBitCast(&function, ptrType);

//                     args.push_back(funcPtr);
//                     args.push_back(callSitePtr);

//                     builder.CreateCall(debugFunc, args);
//                     modified = true;
//                 }
//             }
//         }

//         return modified ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
//     }

//   static bool isRequired() { return true; }
// };



class DebugInjectorPass : public llvm::PassInfoMixin<DebugInjectorPass> {
public:
  llvm::PreservedAnalyses run(llvm::Module &Module,
                              llvm::ModuleAnalysisManager &);

static bool isRequired() { return true; }                              
private:
  llvm::Function *ensurePrintf(llvm::Module &Module);
  llvm::Function *ensureLogger(llvm::Module &Module);
  bool instrumentCall(llvm::CallBase &Call, llvm::Function *Logger);
};

llvm::Function *DebugInjectorPass::ensurePrintf(llvm::Module &Module) {
  llvm::Function *Printf = Module.getFunction("printf");
  if (Printf != nullptr) {
    return Printf;
  }

  llvm::LLVMContext &Context = Module.getContext();
  llvm::FunctionType *PrintfType =
      llvm::FunctionType::get(llvm::Type::getInt32Ty(Context),
                              llvm::PointerType::get(Context, 0),
                              true);
  llvm::FunctionCallee Callee =
      Module.getOrInsertFunction("printf", PrintfType);
  return llvm::cast<llvm::Function>(Callee.getCallee());
}

llvm::Function *DebugInjectorPass::ensureLogger(llvm::Module &Module) {
  llvm::Function *Logger = Module.getFunction("debug_log");
  if (Logger != nullptr && !Logger->empty()) {
    return Logger;
  }

  llvm::LLVMContext &Context = Module.getContext();
  llvm::Type *VoidType = llvm::Type::getVoidTy(Context);
  llvm::Type *CharPtrType = llvm::PointerType::get(Context, 0);
  llvm::FunctionType *LoggerType =
      llvm::FunctionType::get(VoidType, {CharPtrType}, false);

  if (Logger == nullptr) {
    Logger = llvm::Function::Create(LoggerType, llvm::GlobalValue::InternalLinkage,
                                    "debug_log", Module);
  }

  if (Logger->empty()) {
    llvm::BasicBlock *Entry = llvm::BasicBlock::Create(Context, "entry", Logger);
    llvm::IRBuilder<> Builder(Entry);
    llvm::Function *Printf = ensurePrintf(Module);
    llvm::Value *FormatValue = Builder.CreateGlobalString(
        "Debug call before: %s\n", "debug_log.format");
    Builder.CreateCall(Printf, {FormatValue, Logger->getArg(0)});
    Builder.CreateRetVoid();
  }

  return Logger;
}

bool DebugInjectorPass::instrumentCall(llvm::CallBase &Call,
                                       llvm::Function *Logger) {
  llvm::Function *Callee = Call.getCalledFunction();
  if (Callee != nullptr) {
    if (Callee == Logger) {
      return false;
    }
    if (Callee->getName().starts_with("llvm.")) {
      return false;
    }

    // llvm::outs() << Callee->getContext()->get;
    llvm::outs() << "\n";
    // for (const auto *Attr : Callee->getAttributes()) {
    //   if (const auto *Ann = dyn_cast<AnnotateAttr>(Attr)) {
        
    //     // if (Ann->getAnnotation() == kAnnotateName)
    //     //   return true;
    //   }
    // }
    
    
  }

  llvm::IRBuilder<> Builder(Call.getContext());
  Builder.SetInsertPoint(&Call);

  std::string CalleeName;
  if (Callee != nullptr) {
    CalleeName = Callee->getName().str();
  } else {
    CalleeName = "indirect-call";
  }
  if (CalleeName.empty()) {
    CalleeName = "unnamed-call";
  }

  llvm::Value *NamePointer =
      Builder.CreateGlobalString(CalleeName, "debug.call.name");
  Builder.CreateCall(Logger, {NamePointer});
  return true;
}

llvm::PreservedAnalyses
DebugInjectorPass::run(llvm::Module &Module,
                       llvm::ModuleAnalysisManager &) {
  llvm::Function *Logger = ensureLogger(Module);
  if (Logger == nullptr) {
    return llvm::PreservedAnalyses::all();
  }

  bool Changed = false;
  for (llvm::Function &Function : Module) {
    if (Function.isDeclaration()) {
      continue;
    }
    if (&Function == Logger) {
      continue;
    }
    for (llvm::BasicBlock &Block : Function) {
      for (llvm::Instruction &Instruction : Block) {
        auto *Call = llvm::dyn_cast<llvm::CallBase>(&Instruction);
        if (Call == nullptr) {
          continue;
        }
        llvm::Function *CurrentCallee = Call->getCalledFunction();
        if (CurrentCallee == Logger) {
          continue;
        }
        Changed |= instrumentCall(*Call, Logger);
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
//  *
//  */
// struct LifeTime {
//     typedef std::variant<std::monostate, const FunctionDecl *, const CXXRecordDecl *, const CXXTemporaryObjectExpr *, const CallExpr *,
//                          const CXXMemberCallExpr *, const CXXOperatorCallExpr *, const MemberExpr *>
//         ScopeType;
//     /**
//      * The scope and lifetime of variables (code block, function definition, function or method call, etc.)
//      */
//     const ScopeType scope;

//     // Start location for FunctionDecl or other ...Calls, or End locattion for Stmt
//     SourceLocation location;

//     // Locattion for UNSAFE block or Invalid
//     SourceLocation unsafeLoc;

//     LifeTime(SourceLocation loc = SourceLocation(), const ScopeType c = std::monostate(), SourceLocation unsafe = SourceLocation())
//         : location(loc), scope(c), unsafeLoc(unsafe) {}
// };

// class LifeTimeScope : SCOPE(protected) std::deque<LifeTime> { // use deque instead of vector as it preserves iterators when resizing

//   public:
//     const CompilerInstance &m_CI;

//     LifeTimeScope(const CompilerInstance &inst) : m_CI(inst) {}

//     SourceLocation testUnsafe() {
//         auto iter = rbegin();
//         while (iter != rend()) {
//             if (iter->unsafeLoc.isValid()) {
//                 return iter->unsafeLoc;
//             }
//             iter++;
//         }
//         return SourceLocation();
//     }

//     inline bool testInplaceCaller() {

//         static_assert(std::is_same_v<std::monostate, std::variant_alternative_t<0, LifeTime::ScopeType>>);
//         static_assert(std::is_same_v<const FunctionDecl *, std::variant_alternative_t<1, LifeTime::ScopeType>>);

//         return back().scope.index() >= 2;
//     }

//     // std::string Dump(const SourceLocation &loc, std::string_view filter) {

//     //     std::string result = STACK_CHECK_KEYWORD_START_DUMP;
//     //     if (loc.isValid()) {
//     //         if (loc.isMacroID()) {
//     //             result += m_CI.getSourceManager().getExpansionLoc(loc).printToString(m_CI.getSourceManager());
//     //         } else {
//     //             result += loc.printToString(m_CI.getSourceManager());
//     //         }
//     //         result += ": ";
//     //     }
//     //     if (!filter.empty()) {
//     //         //@todo Create Dump filter
//     //         result += std::format(" filter '{}' not implemented!", filter.begin());
//     //     }
//     //     result += "\n";

//     //     // auto iter = begin();
//     //     // while (iter != end()) {

//     //     //     if (iter->location.isValid()) {
//     //     //         result += iter->location.printToString(m_CI.getSourceManager());

//     //     //         std::string name = getName(iter->scope);
//     //     //         if (!name.empty()) {
//     //     //             result += " [";
//     //     //             result += name;
//     //     //             result += "]";
//     //     //         }

//     //     //     } else {
//     //     //         result += " #static ";
//     //     //     }

//     //     //     result += ": ";

//     //     //     std::string list;
//     //     //     auto iter_list = iter->vars.begin();
//     //     //     while (iter_list != iter->vars.end()) {

//     //     //         if (!list.empty()) {
//     //     //             list += ", ";
//     //     //         }

//     //     //         list += iter_list->first;
//     //     //         iter_list++;
//     //     //     }

//     //     //     result += list;

//     //     //     std::string dep_str;
//     //     //     auto dep_list = iter->dependent.begin();
//     //     //     while (dep_list != iter->dependent.end()) {

//     //     //         if (!dep_str.empty()) {
//     //     //             dep_str += ", ";
//     //     //         }

//     //     //         dep_str += "(";
//     //     //         dep_str += dep_list->first;
//     //     //         dep_str += "=>";
//     //     //         dep_str += dep_list->second;
//     //     //         dep_str += ")";

//     //     //         dep_list++;
//     //     //     }

//     //     //     if (!dep_str.empty()) {
//     //     //         result += " #dep ";
//     //     //         result += dep_str;
//     //     //     }

//     //     //     std::string other_str;
//     //     //     auto other_list = iter->other.begin();
//     //     //     while (other_list != iter->other.end()) {

//     //     //         if (!other_str.empty()) {
//     //     //             other_str += ", ";
//     //     //         }

//     //     //         other_str += *other_list;
//     //     //         other_list++;
//     //     //     }

//     //     //     if (!other_str.empty()) {
//     //     //         result += " #other ";
//     //     //         result += other_str;
//     //     //     }

//     //     //     result += "\n";
//     //     //     iter++;
//     //     // }
//     //     return result;
//     // }

//     static std::string getName(const LifeTime::ScopeType &scope) {

//         static_assert(std::is_same_v<const CallExpr *, std::variant_alternative_t<4, LifeTime::ScopeType>>);
//         static_assert(std::is_same_v<const MemberExpr *, std::variant_alternative_t<7, LifeTime::ScopeType>>);

//         const CallExpr *call = nullptr;
//         if (std::holds_alternative<const MemberExpr *>(scope)) {
//             //                llvm::outs() << "getMemberNameInfo(): " << std::get<const MemberExpr
//             //                *>(scope)->getMemberDecl()->getNameAsString() << "\n";
//             return std::get<const MemberExpr *>(scope)->getMemberNameInfo().getAsString();

//         } else if (std::holds_alternative<const FunctionDecl *>(scope)) {

//             return std::get<const FunctionDecl *>(scope)->getNameAsString();

//         } else if (std::holds_alternative<const CXXMemberCallExpr *>(scope)) {
//             call = std::get<const CXXMemberCallExpr *>(scope);
//         } else if (std::holds_alternative<const CXXOperatorCallExpr *>(scope)) {
//             call = std::get<const CXXOperatorCallExpr *>(scope);
//         } else if (std::holds_alternative<const CallExpr *>(scope)) {
//             call = std::get<const CallExpr *>(scope);
//         }

//         if (call) {
//             return call->getDirectCallee()->getQualifiedNameAsString();
//         }

//         return "";
//     }

//     std::string getCalleeName() {
//         auto iter = rbegin();
//         while (iter != rend()) {
//             std::string result = getName(iter->scope);
//             if (!result.empty()) {
//                 return result;
//             }
//             iter++;
//         }
//         return "";
//     }

//     void CheckRecursion(Stmt *st) {
//         auto iter = rbegin();
//         std::string name = getName(iter->scope);
//         if (!name.empty()) {
//             iter++;
//             while (iter != rend()) {
//                 if (name.compare(getName(iter->scope)) == 0) {

//             //         if(){

//             //         }
//             // auto DB = S.getDiagnostics().Report(
//             //     attr.getLoc(),
//             //     S.getDiagnostics().getCustomDiagID(DiagnosticsEngine::Error,
//             //                                        "The attribute '" TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE) "' for '%0' is not
//             applicable."));
//             // DB.AddString(D->getDeclKindName());

//                     Verbose(st->getBeginLoc(), std::format("Recursion call '{}'", name));
//                     return;
//                 }
//                 iter++;
//             }
//         }
//     }

//     std::string getClassName() {
//         auto iter = rbegin();
//         while (iter != rend()) {
//             if (std::holds_alternative<const CXXRecordDecl *>(iter->scope)) {
//                 return std::get<const CXXRecordDecl *>(iter->scope)->getQualifiedNameAsString();
//             }
//             iter++;
//         }
//         return "";
//     }

//     SourceLocation testArgument() {
//         auto iter = rbegin();
//         while (iter != rend()) {
//             if (iter->unsafeLoc.isValid()) {
//                 return iter->unsafeLoc;
//             }
//             iter++;
//         }
//         return SourceLocation();
//     }

//     void PushScope(SourceLocation loc, LifeTime::ScopeType call = std::monostate(), SourceLocation unsafe = SourceLocation()) {
//         push_back(LifeTime(loc, call, unsafe));
//     }

//     void PopScope() {
//         assert(size() > 1); // First level reserved for static objects
//         pop_back();
//     }

//     LifeTime &back() {
//         assert(size()); // First level reserved for static objects
//         return std::deque<LifeTime>::back();
//     }
// };

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
    // The first string arguments in the `trust` attribute for working and managing the plugin
    static inline const char *LEVEL = STACK_CHECK_KEYWORD_LEVEL;

    static inline const char *UNSAFE = STACK_CHECK_KEYWORD_UNSAFE;
    static inline const char *PRINT_AST = STACK_CHECK_KEYWORD_PRINT_AST;
    // static inline const char *PRINT_DUMP = STACK_CHECK_KEYWORD_PRINT_DUMP;

    static inline const char *STATUS_ENABLE = STACK_CHECK_KEYWORD_ENABLE;
    static inline const char *STATUS_DISABLE = STACK_CHECK_KEYWORD_DISABLE;
    static inline const char *STATUS_PUSH = STACK_CHECK_KEYWORD_PUSH;
    static inline const char *STATUS_POP = STACK_CHECK_KEYWORD_POP;

    std::set<std::string> m_listFirstArg{LEVEL, UNSAFE, PRINT_AST};
    std::set<std::string> m_listStatus{STATUS_ENABLE, STATUS_DISABLE, STATUS_PUSH, STATUS_POP};

    typedef std::map<std::string, std::pair<const CXXRecordDecl *, SourceLocation>> ClassListType;
    typedef std::variant<const CXXRecordDecl *, std::string> LocationType;

    std::vector<bool> m_status{true};

    clang::DiagnosticsEngine::Level m_level_non_const_arg;
    clang::DiagnosticsEngine::Level m_level_non_const_method;
    clang::DiagnosticsEngine::Level m_diagnostic_level;

    static const inline std::pair<std::string, std::string> pair_empty{std::make_pair<std::string, std::string>("", "")};

    int64_t line_base;
    int64_t line_number;

    // LifeTimeScope m_scopes;

    trust::StringMatcher m_dump_matcher;
    SourceLocation m_dump_location;
    SourceLocation m_trace_location;

    const CompilerInstance &m_CI;

    TrustPlugin(const CompilerInstance &instance) : m_CI(instance), line_base(0), line_number(0) {
        // Zero level for static variables
        // m_scopes.PushScope(SourceLocation(), std::monostate(), SourceLocation());

        m_diagnostic_level = clang::DiagnosticsEngine::Level::Error;
    }

    inline clang::DiagnosticsEngine &getDiag() { return m_CI.getDiagnostics(); }

    void clear() {
        //            TrustPlugin empty(m_CI);
        //            std::swap(*this, empty);
    }

    void dump(raw_ostream &out) {
        out << "\n#trust-config\n";
        // out << "error-type: " << makeHelperString(m_error_type) << "\n";
        // out << "warning-type: " << makeHelperString(m_warning_type) << "\n";
        // out << STACK_CHECK_KEYWORD_AUTO_TYPE ": " << makeHelperString(m_auto_type) << "\n";
        // out << STACK_CHECK_KEYWORD_SHARED_TYPE ": " << makeHelperString(m_shared_type) << "\n";
        // out << "not-shared-classes: " << makeHelperString(m_not_shared_class) << "\n";
        // out << STACK_CHECK_KEYWORD_INVALIDATE_FUNC ": " << makeHelperString(m_invalidate_func) << "\n";
        out << "\n";
    }

    clang::DiagnosticsEngine::Level getLevel(clang::DiagnosticsEngine::Level original) {
        // SourceLocation loc = m_scopes.testUnsafe();
        // if (loc.isValid()) {
        //     return clang::DiagnosticsEngine::Level::Ignored;
        // }
        if (!isEnabledStatus()) {
            return clang::DiagnosticsEngine::Level::Ignored;
        } else if (original > m_diagnostic_level) {
            return m_diagnostic_level;
        }
        return original;
    }

    std::string LogPos(const SourceLocation &loc) {

        size_t line_no = 0;
        if (loc.isMacroID()) {
            line_no = m_CI.getSourceManager().getSpellingLineNumber(m_CI.getSourceManager().getExpansionLoc(loc));
        } else {
            line_no = m_CI.getSourceManager().getSpellingLineNumber(loc);
        }

        if (loc.isValid()) {
            return std::format("{}", line_no - line_base + line_number);
        }
        return "0";
    }

    void LogOnly(SourceLocation loc, std::string str, SourceLocation hash = SourceLocation(), LogLevel level = LogLevel::INFO) {
        // if (logger) {
        //     const char * prefix = nullptr;
        //     switch (level) {
        //         case LogLevel::INFO:
        //             prefix = "log";
        //             break;
        //         case LogLevel::WARN:
        //             prefix = "warn";
        //             break;
        //         case LogLevel::ERR:
        //             prefix = "err";
        //             break;
        //     }
        //     logger->Log(loc, std::format("#{} #{} {}", prefix, hash.isValid() ? LogPos(hash) : LogPos(loc), str));
        // }
    }

    void LogWarning(SourceLocation loc, std::string str, SourceLocation hash = SourceLocation()) {
        getDiag().Report(loc, getDiag().getCustomDiagID(getLevel(clang::DiagnosticsEngine::Warning), "%0")).AddString(str);
        LogOnly(loc, str, hash, LogLevel::WARN);
    }

    void LogError(SourceLocation loc, std::string str, SourceLocation hash = SourceLocation()) {
        getDiag().Report(loc, getDiag().getCustomDiagID(getLevel(clang::DiagnosticsEngine::Error), "%0")).AddString(str);
        LogOnly(loc, str, hash, LogLevel::ERR);
    }

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

    // static bool checkBehavior(const std::string_view str, clang::DiagnosticsEngine::Level *level = nullptr) {
    //     if (str.compare(ERROR) == 0) {
    //         if (level) {
    //             *level = clang::DiagnosticsEngine::Level::Error;
    //         }
    //         return true;
    //     } else if (str.compare(WARNING) == 0) {
    //         if (level) {
    //             *level = clang::DiagnosticsEngine::Level::Warning;
    //         }
    //         return true;
    //     } else if (str.compare(IGNORED) == 0) {
    //         if (level) {
    //             *level = clang::DiagnosticsEngine::Level::Ignored;
    //         }
    //         return true;
    //     }
    //     return false;
    // }

    static std::string unknownArgumentHelper(const std::string_view arg, const std::set<std::string> &set) {
        std::string result = "Unknown argument '";
        result += arg.begin();
        result += "'. Expected string argument from the following list: ";
        result += makeHelperString(set);
        return result;
    }

    std::string processArgs(std::string_view first, std::string_view second, SourceLocation loc) {

        std::string result;
        if (first.empty() || second.empty()) {
            // if (first.compare(PROFILE) == 0) {
            //     clear();
            // } else
            if (first.compare(UNSAFE) == 0 || first.compare(PRINT_AST) == 0) {
                // The second argument may be empty
            } else {
                result = "Two string literal arguments expected!";
            }
        } else if (m_listFirstArg.find(first.begin()) == m_listFirstArg.end()) {
            result = unknownArgumentHelper(first.begin(), m_listFirstArg);
        }

        static const char *LEVEL_ERROR_MESSAGE = "Required behavior not recognized! Allowed values: '" STACK_CHECK_KEYWORD_ERROR "', '"
                                                 "', '" STACK_CHECK_KEYWORD_WARNING "' or '" STACK_CHECK_KEYWORD_IGNORED "'.";

        if (result.empty()) {
            // if (first.compare(STATUS) == 0) {
            //     if (m_listStatus.find(second.begin()) == m_listStatus.end()) {
            //         result = unknownArgumentHelper(second, m_listStatus);
            //     } else {
            //         if (m_status.empty()) {
            //             result = "Violation of the logic of saving and restoring the state of the plugin";
            //         } else {
            //             if (second.compare(STATUS_ENABLE) == 0) {
            //                 *m_status.rbegin() = true;
            //             } else if (second.compare(STATUS_DISABLE) == 0) {
            //                 *m_status.rbegin() = false;
            //             } else if (second.compare(STATUS_PUSH) == 0) {
            //                 m_status.push_back(true);
            //             } else if (second.compare(STATUS_POP) == 0) {
            //                 if (m_status.size() == 1) {
            //                     result = "Violation of the logic of saving and restoring the state of the plugin";
            //                 } else {
            //                     m_status.pop_back();
            //                 }
            //             }
            //         }
            //     }
            // } else if (first.compare(LEVEL) == 0) {

            //     if (m_listLevel.find(second.begin()) == m_listLevel.end()) {
            //         result = unknownArgumentHelper(second, m_listLevel);
            //     } else if (!checkBehavior(second, &m_diagnostic_level)) {
            //         result = LEVEL_ERROR_MESSAGE;
            //     }
            //
            // } else if (first.compare(PROFILE) == 0) {
            //     if (!second.empty()) {
            //         result = "Loading profile from file is not implemented!";
            //     }
            //     // } else if (first.compare(ERROR_TYPE) == 0) {
            //     //     m_error_type.emplace(second.begin());
            //     // } else if (first.compare(WARNING_TYPE) == 0) {
            //     //     m_warning_type.emplace(second.begin());
            //     // } else if (first.compare(SHARED_TYPE) == 0) {
            //     //     m_shared_type.emplace(second.begin(), LocToStr(loc, m_CI.getSourceManager()));
            //     // } else if (first.compare(AUTO_TYPE) == 0) {
            //     //     m_auto_type.emplace(second.begin());
            //     // } else if (first.compare(INVALIDATE_FUNC) == 0) {
            //     //     m_invalidate_func.emplace(second.begin());
            // }
        }
        return result;
    }

    std::string LocationTypeToString(LocationType &loc) {
        if (std::holds_alternative<std::string>(loc)) {
            return std::get<std::string>(loc);
        }
        assert(std::holds_alternative<const CXXRecordDecl *>(loc));
        return LocToStr(std::get<const CXXRecordDecl *>(loc)->getLocation(), m_CI.getSourceManager());
    }

    /**
     * Make the list of used classes by recursively traversing parent classes and all their field types.
     */
    void MakeUsedClasses(const CXXRecordDecl &decl, ClassListType &used, ClassListType &fields) {
        // if (used.find(decl.getQualifiedNameAsString()) != used.end()) {
        //     return;
        // }

        // if (const ClassTemplateSpecializationDecl * Special = dyn_cast<const ClassTemplateSpecializationDecl> (&decl)) {

        //     if (m_shared_type.find(Special->getQualifiedNameAsString()) != m_shared_type.end()) {
        //         m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //     } else {
        //         MakeUsedClasses(*Special->getSpecializedTemplate()->getTemplatedDecl(), used, fields);
        //     }

        //     const TemplateArgumentList& ArgsList = Special->getTemplateArgs();
        //     const TemplateParameterList* TemplateList = Special->getSpecializedTemplate()->getTemplateParameters();

        //     int Index = 0;
        //     for (auto TemplateToken = TemplateList->begin(); TemplateToken != TemplateList->end(); TemplateToken++) {
        //         if ((*TemplateToken)->getKind() == Decl::Kind::TemplateTypeParm) {
        //             if (const CXXRecordDecl * type = ArgsList[Index].getAsType()->getAsCXXRecordDecl()) {
        //                 if (m_shared_type.find(type->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                     m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //                 }
        //                 used[type->getQualifiedNameAsString()] = {type, Special->getLocation()};
        //                 MakeUsedClasses(*type, used, fields);
        //             }
        //         }
        //         Index++;
        //     }
        // }

        // if (decl.hasDefinition()) {

        //     for (auto iter = decl.bases_begin(); iter != decl.bases_end(); iter++) {

        //         // iter->getType()->dump();

        //         if (const ElaboratedType * templ = dyn_cast<const ElaboratedType> (iter->getType())) {

        //             if (const TemplateSpecializationType * Special = dyn_cast<const TemplateSpecializationType> (templ->desugar())) {
        //                 if (const ClassTemplateDecl * templ_class = dyn_cast<const ClassTemplateDecl>
        //                 (Special->getTemplateName().getAsTemplateDecl())) {
        //                     assert(templ_class->getTemplatedDecl());

        //                     if (m_shared_type.find(templ_class->getTemplatedDecl()->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                         m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //                     } else {
        //                         //@todo Do you need all the descendants or can you limit the list to the first template class?
        //                         used[templ_class->getTemplatedDecl()->getQualifiedNameAsString()] = {templ_class->getTemplatedDecl(),
        //                         iter->getBeginLoc()}; MakeUsedClasses(*templ_class->getTemplatedDecl(), used, fields);
        //                     }

        //                     for (auto elem : Special->template_arguments()) {
        //                         //                                    llvm::outs() << elem.getKind() << "\n";
        //                         if (elem.getKind() == TemplateArgument::ArgKind::Type) {
        //                             if (const CXXRecordDecl * type = elem.getAsType()->getAsCXXRecordDecl()) {
        //                                 if (m_shared_type.find(type->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                                     m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //                                 } else {
        //                                     //@todo Do you need all the descendants or can you limit the list to the first template
        //                                     class? used[type->getQualifiedNameAsString()] = {type, iter->getBeginLoc()};
        //                                     MakeUsedClasses(*elem.getAsType()->getAsCXXRecordDecl(), used, fields);
        //                                 }
        //                             }
        //                         }
        //                     }
        //                 }
        //             } else if (const RecordType * rec = dyn_cast<const RecordType> (templ->desugar())) {
        //                 if (const CXXRecordDecl * base = rec->desugar()->getAsCXXRecordDecl()) {
        //                     if (m_shared_type.find(base->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                         m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //                     }
        //                     used[base->getQualifiedNameAsString()] = {base, iter->getBeginLoc()};
        //                     MakeUsedClasses(*base, used, fields);
        //                 }
        //             }

        //         } else if (const ClassTemplateSpecializationDecl * Special = dyn_cast<const ClassTemplateSpecializationDecl>
        //         (iter->getType()->getAsCXXRecordDecl())) {

        //             if (m_shared_type.find(Special->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                 m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //             } else {
        //                 MakeUsedClasses(*Special->getSpecializedTemplate()->getTemplatedDecl(), used, fields);
        //             }

        //             const TemplateArgumentList& ArgsList = Special->getTemplateArgs();
        //             const TemplateParameterList* TemplateList = Special->getSpecializedTemplate()->getTemplateParameters();

        //             int Index = 0;
        //             for (auto TemplateToken = TemplateList->begin(); TemplateToken != TemplateList->end(); TemplateToken++) {
        //                 if ((*TemplateToken)->getKind() == Decl::Kind::TemplateTypeParm) {
        //                     if (const CXXRecordDecl * type = ArgsList[Index].getAsType()->getAsCXXRecordDecl()) {
        //                         if (m_shared_type.find(type->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                             m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //                         }
        //                         used[type->getQualifiedNameAsString()] = {type, iter->getBeginLoc()};
        //                         MakeUsedClasses(*type, used, fields);
        //                     }
        //                 }
        //                 Index++;
        //             }

        //         } else if (const CXXRecordDecl * base = iter->getType()->getAsCXXRecordDecl()) {

        //             if (m_shared_type.find(base->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                 m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //             }
        //             used[base->getQualifiedNameAsString()] = {base, iter->getBeginLoc()};
        //             MakeUsedClasses(*base, used, fields);
        //         }
        //     }

        //     for (auto field = decl.field_begin(); field != decl.field_end(); field++) {

        //         if (field->getCanonicalDecl()->getType()->isPointerOrReferenceType()) {

        //             QualType CanonicalType = field->getCanonicalDecl()->getType()->getPointeeType();
        //             //  llvm::outs() << "isPointerOrReferenceType " << CanonicalType.getAsString() << "\n";

        //             if (const auto *RT = CanonicalType->getAs<RecordType>()) {
        //                 if (const auto *ClassDecl = dyn_cast<CXXRecordDecl>(RT->getDecl())) {

        //                     // the presence of a field with a reference to a structured type
        //                     // means that the current type is a reference data type
        //                     m_shared_type[decl.getQualifiedNameAsString()] = &decl;

        //                     if (fields.find(ClassDecl->getQualifiedNameAsString()) == fields.end() &&
        //                     fields.find(decl.getQualifiedNameAsString()) == fields.end()) {

        //                         fields[ClassDecl->getQualifiedNameAsString()] = {ClassDecl, field->getLocation()};

        //                         LogOnly(field->getLocation(), std::format("Field with reference to structured data type '{}'",
        //                         ClassDecl->getQualifiedNameAsString()));

        //                         // Save the type name in the list of used data types
        //                         // for future analysis of circular references.
        //                         MakeUsedClasses(*ClassDecl, used, fields);
        //                     }

        //                 }
        //             }
        //         }

        //         const CXXRecordDecl * field_type = field->getType()->getAsCXXRecordDecl();
        //         if (!field_type) {
        //             continue;
        //         }

        //         if (const ClassTemplateSpecializationDecl * Special = dyn_cast<const ClassTemplateSpecializationDecl> (field_type)) {

        //             if (m_shared_type.find(Special->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                 m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //             } else {
        //                 if (fields.find(Special->getQualifiedNameAsString()) == fields.end() &&
        //                 fields.find(decl.getQualifiedNameAsString()) == fields.end()) {
        //                     MakeUsedClasses(*Special, used, fields);
        //                 }
        //             }

        //             // std::string diag_name;
        //             // llvm::raw_string_ostream str_out(diag_name);
        //             // Special->getNameForDiagnostic(str_out,m_CI.getASTContext().getPrintingPolicy(), true);
        //             // llvm::outs() << diag_name << "\n";

        //             const TemplateArgumentList& ArgsList = Special->getTemplateArgs();
        //             const TemplateParameterList* TemplateList = Special->getSpecializedTemplate()->getTemplateParameters();

        //             int Index = 0;
        //             for (auto TemplateToken = TemplateList->begin(); TemplateToken != TemplateList->end(); TemplateToken++) {
        //                 if ((*TemplateToken)->getKind() == Decl::Kind::TemplateTypeParm) {
        //                     if (const CXXRecordDecl * type = ArgsList[Index].getAsType()->getAsCXXRecordDecl()) {
        //                         if (fields.find(type->getQualifiedNameAsString()) == fields.end() &&
        //                         fields.find(decl.getQualifiedNameAsString()) == fields.end()) {
        //                             if (m_shared_type.find(type->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                                 m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //                             }
        //                             fields[type->getQualifiedNameAsString()] = {type, field->getLocation()};
        //                             MakeUsedClasses(*type, used, fields);
        //                         }
        //                     }
        //                 }
        //                 Index++;
        //             }

        //         } else {
        //             if (fields.find(field_type->getQualifiedNameAsString()) == fields.end() &&
        //             fields.find(decl.getQualifiedNameAsString()) == fields.end()) {
        //                 if (m_shared_type.find(field_type->getQualifiedNameAsString()) != m_shared_type.end()) {
        //                     m_shared_type[decl.getQualifiedNameAsString()] = &decl;
        //                 }
        //                 fields[field_type->getQualifiedNameAsString()] = {field_type, field->getLocation()};
        //                 MakeUsedClasses(*field_type, used, fields);
        //             }
        //         }
        //     }
        // }
    }

    void reduceSharedList(ClassListType &list, bool test_external = true) {
        auto iter = list.begin();
        // while (iter != list.end()) {
        // if (iter->second.first->hasDefinition()) {

        //     auto found_shared = m_shared_type.find(iter->first);
        //     if (found_shared == m_shared_type.end()) {
        //         iter = list.erase(iter);
        //     } else {
        //         iter++;
        //     }

        // } else if (test_external) {

        //     auto found = m_classes.find(iter->first);
        //     if (found == m_classes.end()) {

        //         std::string message = std::format("Class definition '{}' not found in current translation unit.", iter->first);

        //         LogError(iter->second.second, message);

        //         getDiag().Report(iter->second.second,
        //                 getDiag().getCustomDiagID(clang::DiagnosticsEngine::Error, ""
        //                 "%0\n"
        //                 "The circular reference analyzer requires two passes.\n"
        //                 "First run the plugin with key '--circleref-write -fsyntax-only' to generate the class list,\n"
        //                 "then run a second time with the '--circleref-read' key to re-analyze,\n"
        //                 "or disable the circular reference analyzer with the 'circleref-disable' option.\n"))
        //                 .AddString(message);

        //         throw std::runtime_error(message);

        //     } else {
        //         if (found->second.parents.empty() && found->second.fields.empty()) {
        //             LogOnly(iter->second.second, std::format("Non shared class definition '{}' used from another translation unit.",
        //             iter->first)); iter = list.erase(iter);
        //         } else {
        //             LogOnly(iter->second.second, std::format("Shared class definition '{}' used from another translation unit.",
        //             iter->first)); iter++;
        //         }
        //     }
        // } else {
        //     iter++;
        // }
        // }
    }

    /**
     * A class with cyclic relationships that has a field of reference type to its own type,
     * or its field has a reference type to another class that has a field of reference type
     * to the class being checked, either directly or through reference fields of other classes.
     */
    bool checkCycles(const CXXRecordDecl &decl, ClassListType &used, ClassListType &fields) {

        // Remove all not shared classes
        reduceSharedList(fields);

        // used[decl.getQualifiedNameAsString()] = {&decl, decl.getLocation()};

        // for (auto parent : used) {
        //     auto field_found = fields.find(parent.first);
        //     if (field_found != fields.end()) {
        //         LogError(field_found->second.second, std::format("Class {} has a reference to itself through the field type {}",
        //                 decl.getQualifiedNameAsString(), field_found->first));
        //         return false;
        //     }
        // }

        // ClassListType other;
        // ClassListType other_fields;
        // for (auto elem : fields) {

        //     assert(elem.second.first);

        //     other.clear();
        //     other_fields.clear();

        //     MakeUsedClasses(*elem.second.first, other, other_fields);

        //     reduceSharedList(other_fields);

        //     auto other_found = other_fields.find(elem.first);
        //     if (other_found != other_fields.end()) {
        //         bool is_unsafe = m_scopes.testUnsafe().isValid() || checkDeclUnsafe(decl) || checkDeclUnsafe(*elem.second.first) ||
        //         checkDeclUnsafe(*other_found->second.first);

        //         if (is_unsafe) {
        //             LogWarning(other_found->second.second, std::format("UNSAFE The class '{}' has a circular reference through class
        //             '{}'",
        //                     decl.getQualifiedNameAsString(), other_found->first));
        //         } else {
        //             // Class {} has a cross reference through a field with type вввввв
        //             LogError(other_found->second.second, std::format("The class '{}' has a circular reference through class '{}'",
        //                     decl.getQualifiedNameAsString(), other_found->first));
        //         }
        //         return false;
        //     }

        //     for (auto elem_other : other_fields) {
        //         if (used.count(elem_other.first)) {

        //             bool is_unsafe = m_scopes.testUnsafe().isValid() || checkDeclUnsafe(decl) || checkDeclUnsafe(*elem.second.first) ||
        //             checkDeclUnsafe(*elem_other.second.first);

        //             if (is_unsafe) {
        //                 LogWarning(elem_other.second.second, std::format("UNSAFE The class '{}' has a circular reference through class
        //                 '{}'",
        //                         elem.first, decl.getQualifiedNameAsString()));
        //             } else {
        //                 LogError(elem_other.second.second, std::format("The class '{}' has a circular reference through class '{}'",
        //                         elem.first, decl.getQualifiedNameAsString()));
        //             }
        //             return false;
        //         }
        //     }
        // }

        return true;
    }

    inline bool isEnabledStatus() {
        assert(!m_status.empty());
        return *m_status.rbegin();
    }

    /*
     * plugin helper methods
     *
     */
    inline bool isEnabled() { return isEnabledStatus(); }

    SourceLocation checkUnsafeBlock(const AttributedStmt *attrStmt) {

        if (attrStmt) {

            auto attrs = attrStmt->getAttrs();

            for (auto &elem : attrs) {

                std::pair<std::string, std::string> pair = parseAttr(dyn_cast_or_null<AnnotateAttr>(elem));
                if (pair != pair_empty) {

                    if (pair.first.compare(UNSAFE) == 0) {

                        // if (logger) {
                        //     logger->AttrComplete(elem->getLocation());
                        // }

                        LogOnly(attrStmt->getBeginLoc(), "Unsafe statement", attrStmt->getBeginLoc());

                        return elem->getLocation();
                    }
                }
            }
        }
        return SourceLocation();
    }

    std::pair<std::string, std::string> parseAttr(const AnnotateAttr *const attr) {
        if (!attr || attr->getAnnotation() != TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE) || attr->args_size() != 2) {
            return pair_empty;
        }

        clang::AnnotateAttr::args_iterator result = attr->args_begin();
        clang::StringLiteral *first = dyn_cast_or_null<clang::StringLiteral>(*result);
        result++;
        clang::StringLiteral *second = dyn_cast_or_null<clang::StringLiteral>(*result);

        if (!first || !second) {
            getDiag().Report(attr->getLocation(),
                             getDiag().getCustomDiagID(DiagnosticsEngine::Error, "Two string literal arguments expected!"));
            return pair_empty;
        }
        return std::make_pair<std::string, std::string>(first->getString().str(), second->getString().str());
    }

    /*
     *
     *
     *
     */

    bool checkDeclUnsafe(const Decl &decl) {
        auto attr_args = parseAttr(decl.getAttr<AnnotateAttr>());
        if (attr_args != pair_empty) {
            return attr_args.first.compare(UNSAFE) == 0;
        }
        return false;
    }

    void checkDeclAttributes(const Decl *decl) {
        // Check namespace annotation attribute
        // This check should be done first as it is used to enable and disable the plugin.

        if (!decl) {
            return;
        }

        // AnnotateAttr *attr = decl->getAttr<AnnotateAttr>();
        // auto attr_args = parseAttr(attr);

        // if (attr_args != pair_empty) {

        //     std::string error_str = processArgs(attr_args.first, attr_args.second, decl->getLocation());

        //     if (!error_str.empty()) {

        //         clang::DiagnosticBuilder DB =
        //             getDiag().Report(attr->getLocation(), getDiag().getCustomDiagID(DiagnosticsEngine::Error, "Error detected: %0"));
        //         DB.AddString(error_str);

        //         LogError(attr->getLocation(), error_str);

        //     } else if (attr_args.first.compare(BASELINE) == 0) {

        //         SourceLocation loc = decl->getLocation();
        //         if (loc.isMacroID()) {
        //             loc = m_CI.getSourceManager().getExpansionLoc(loc);
        //         }

        //         try {
        //             int old_line_number = line_number;
        //             line_number = std::stoi(SeparatorRemove(attr_args.second));
        //             line_base = getDiag().getSourceManager().getSpellingLineNumber(loc);

        //             if (old_line_number >= line_number) {
        //                 LogOnly(loc, "Error in base sequential numbering");
        //                 getDiag().Report(loc, getDiag().getCustomDiagID(DiagnosticsEngine::Error, "Error in base sequential numbering"));
        //             }

        //         } catch (...) {
        //             LogOnly(loc, "The second argument is expected to be a line number as a literal string!");
        //             getDiag().Report(loc,
        //                              getDiag().getCustomDiagID(DiagnosticsEngine::Error,
        //                                                        "The second argument is expected to be a line number as a literal
        //                                                        string!"));
        //         }

        //     } else if (attr_args.first.compare(STATUS) == 0) {

        //         clang::DiagnosticBuilder DB = getDiag().Report(
        //             decl->getLocation(), getDiag().getCustomDiagID(DiagnosticsEngine::Note, "Status memory safety plugin is %0!"));
        //         DB.AddString(attr_args.second);
        //     }

        //     // if (logger) {
        //     //     logger->AttrComplete(attr->getLocation());
        //     // }
        // }
    }

    /*
     * @ref TRUSTED_DUMP
     */
    void checkDumpFilter(const Decl *decl) {
        if (decl) {
            AnnotateAttr *attr = decl->getAttr<AnnotateAttr>();
            auto attr_args = parseAttr(attr);
            if (attr_args != pair_empty) {
                if (attr_args.first.compare(PRINT_AST) == 0) {
                    if (attr_args.second.empty()) {
                        m_dump_matcher.Clear();
                    } else {
                        //@todo Create Dump filter
                        llvm::outs() << "Dump filter '" << attr_args.second << "' not implemented!\n";
                        m_dump_matcher.Create(attr_args.second, ';');
                    }
                    if (decl->getLocation().isMacroID()) {
                        m_dump_location = m_CI.getSourceManager().getExpansionLoc(decl->getLocation());
                    } else {
                        m_dump_location = decl->getLocation();
                    }

                    // if (logger) {
                    //     logger->AttrComplete(attr->getLocation());
                    // }

                    // } else if (attr_args.first.compare(PRINT_DUMP) == 0) {

                    //     if (skipLocation(m_trace_location, decl->getLocation())) {
                    //         return;
                    //     }

                    //     // if (logger) {
                    //     //     logger->AttrComplete(attr->getLocation());
                    //     // }

                    //     llvm::outs() << m_scopes.Dump(attr->getLocation(), attr_args.second);
                }
            }
        }
    }

    bool skipLocation(SourceLocation &last, const SourceLocation &loc) {
        int64_t line_no = 0;
        SourceLocation test_loc = loc;

        if (loc.isMacroID()) {
            test_loc = m_CI.getSourceManager().getExpansionLoc(loc);
        }
        if (last.isValid() &&
            m_CI.getSourceManager().getSpellingLineNumber(test_loc) == m_CI.getSourceManager().getSpellingLineNumber(last)) {
            return true;
        }
        last = test_loc;
        return false;
    }

    void printDumpIfEnabled(const Decl *decl) {
        if (!decl || !isEnabledStatus() || m_dump_matcher.isEmpty() || skipLocation(m_dump_location, decl->getLocation())) {
            return;
        }

        // Source location for IDE
        llvm::outs() << decl->getLocation().printToString(m_CI.getSourceManager());
        // Color highlighting
        llvm::outs() << "  \033[1;46;34m";
        // The string at the current position to expand the AST

        PrintingPolicy Policy(m_CI.getASTContext().getPrintingPolicy());
        Policy.SuppressScope = false;
        Policy.AnonymousTagLocations = true;

        //@todo Create Dump filter
        std::string output;
        llvm::raw_string_ostream str(output);
        decl->print(str, Policy);

        size_t pos = output.find("\n");
        if (pos == std::string::npos) {
            llvm::outs() << output;
        } else {
            llvm::outs() << output.substr(0, pos - 1);
        }

        // Close color highlighting
        llvm::outs() << "\033[0m ";
        llvm::outs() << " dump:\n";

        // Ast tree for current line
        const ASTContext &Ctx = m_CI.getASTContext();
        ASTDumper P(llvm::outs(), Ctx, /*ShowColors=*/true);
        P.Visit(decl); // dyn_cast<Decl>(decl)
    }

    void printDumpIfEnabled(const Stmt *stmt) {
        if (!stmt || !isEnabledStatus() || m_dump_matcher.isEmpty() || skipLocation(m_dump_location, stmt->getBeginLoc())) {
            return;
        }

        // Source location for IDE
        llvm::outs() << stmt->getBeginLoc().printToString(m_CI.getSourceManager());
        // Color highlighting
        llvm::outs() << "  \033[1;46;34m";
        // The string at the current position to expand the AST

        PrintingPolicy Policy(m_CI.getASTContext().getPrintingPolicy());
        Policy.SuppressScope = false;
        Policy.AnonymousTagLocations = true;

        //@todo Create Dump filter
        std::string output;
        llvm::raw_string_ostream str(output);
        stmt->printPretty(str, nullptr, Policy);

        size_t pos = output.find("\n");
        if (pos == std::string::npos) {
            llvm::outs() << output;
        } else {
            llvm::outs() << output.substr(0, pos - 1);
        }

        // Close color highlighting
        llvm::outs() << "\033[0m ";
        llvm::outs() << " dump:\n";

        // Ast tree for current line
        const ASTContext &Ctx = m_CI.getASTContext();
        ASTDumper P(llvm::outs(), Ctx, /*ShowColors=*/true);
        P.Visit(stmt); // dyn_cast<Decl>(decl)
    }

    /*
     *
     *
     */

    // const char *checkClassNameTracking(const CXXRecordDecl *type) {
    //     const char *result = type ? findClassType(type->getQualifiedNameAsString()) : nullptr;
    //     const CXXRecordDecl *cxx = dyn_cast_or_null<CXXRecordDecl>(type);
    //     if (cxx) {
    //         for (auto iter = cxx->bases_begin(); !result && iter != cxx->bases_end(); iter++) {
    //             if (iter->isBaseOfClass()) {
    //                 if (const CXXRecordDecl *base = iter->getType()->getAsCXXRecordDecl()) {
    //                     result = checkClassNameTracking(base);
    //                 }
    //             }
    //         }
    //     }
    //     return result;
    // }

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
        if (isEnabledStatus()) {                                                                                                           \
            RecursiveASTVisitor<TrustPlugin>::Traverse##name(arg);                                                                         \
        }                                                                                                                                  \
        return true;                                                                                                                       \
    }

    TRAVERSE_CONTEXT(MemberExpr);

    TRAVERSE_CONTEXT(CallExpr);
    TRAVERSE_CONTEXT(CXXMemberCallExpr);

    /*
     * Creating a plugin context for classes
     */

    bool TraverseCXXRecordDecl(CXXRecordDecl *decl) {

        if (isEnabledStatus() && decl->hasDefinition()) {
            RecursiveASTVisitor<TrustPlugin>::TraverseCXXRecordDecl(decl);
            return true;
        }

        return RecursiveASTVisitor<TrustPlugin>::TraverseCXXRecordDecl(decl);
    }

    /*
     * Traversing statements in AST
     */
    bool TraverseStmt(Stmt *stmt) {

        // Enabling and disabling planig is implemented via declarations,
        // so statements are processed only after the plugin is activated.
        if (isEnabledStatus()) {

            // // Check for dump AST tree
            // if (const DeclStmt *decl = dyn_cast_or_null<DeclStmt>(stmt)) {
            //     checkDumpFilter(decl->getSingleDecl());
            // }
            // printDumpIfEnabled(stmt);

            const AttributedStmt *attrStmt = dyn_cast_or_null<AttributedStmt>(stmt);
            const CompoundStmt *block = dyn_cast_or_null<CompoundStmt>(stmt);

            if (isEnabled() && (attrStmt || block)) {

                RecursiveASTVisitor<TrustPlugin>::TraverseStmt(stmt);

                return true;
            }

            // Recursive traversal of statements only when the plugin is enabled
            RecursiveASTVisitor<TrustPlugin>::TraverseStmt(stmt);
        }

        return true;
    }

    /*
     * All AST analysis starts with TraverseDecl
     *
     */
    bool TraverseDecl(Decl *D) {

        // checkDumpFilter(D);
        checkDeclAttributes(D);
        // printDumpIfEnabled(D);

        if (const FunctionDecl *func = dyn_cast_or_null<FunctionDecl>(D)) {

            if (isEnabled() && func->isDefined()) {

                // m_scopes.PushScope(D->getLocation(), func, m_scopes.testUnsafe());

                RecursiveASTVisitor<TrustPlugin>::TraverseDecl(D);

                // m_scopes.PopScope();
                return true;
            }
        }

        // Enabling and disabling the plugin is implemented using declarations,
        // so declarations are always processed.
        RecursiveASTVisitor<TrustPlugin>::TraverseDecl(D);

        return true;
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

        // if (logger) {

        //     llvm::outs().flush();
        //     llvm::errs().flush();

        //     logger->Dump(llvm::outs());
        //     llvm::outs() << "\n";
        //     plugin->dump(llvm::outs());
        // }
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
    /* Three types of plugin execution:
     *
     * - In one pass for one translation unit (default).
     * When all type definitions are in one file or strong references are not used
     * for forward declarations of types whose definitions are in other translation units.
     *
     * - The first pass when processing several translation units (when the circleref-write argument is specified).
     * Create a file of the list of shared data types without performing the actual AST analysis.
     *
     * - The second pass when processing several translation units (when the circleref-read argument is specified).
     * Perform direct analysis of the program source code analysis with control of recursive references,
     * the list of which must be in the file created during the first processing of all translation units.
     *
     */
    std::string m_shared_write;
    std::string m_shared_read;

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile) override {

        llvm::outs() <<"CreateASTConsumer\n";
        std::unique_ptr<TrustPluginASTConsumer> obj = std::unique_ptr<TrustPluginASTConsumer>(new TrustPluginASTConsumer());

        // Compiler.getCodeGenOpts().PassPlugins.push_back("stack_check_clang.so");

        return obj;
    }

    bool BeginInvocation(CompilerInstance &CI) override {
        // // 1) Добавляем pass-plugin (то же, что даёт -fpass-plugin=...)
        // CI.getCodeGenOpts().PassPlugins.push_back("/abs/path/libMyPassPlugin.so");

        llvm::outs() <<"BeginInvocation\n";
        // llvm::outs() <<CI <<"\n"; /// .getPassPlugins().size()
        // CI.LoadRequestedPlugins ();
        //CI.getFrontendOpts().LLVMArgs.push_back("-passes=stack_check");
        // 2) Ничего больше не нужно, если pass-plugin сам добавляется в пайплайн
        // через registerPipelineStartEPCallback / registerOptimizerLastEPCallback и т.п.
        return true;
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

            std::string message = plugin->processArgs(first, second, SourceLocation());

            if (!message.empty()) {
                if (first.compare("verbose") == 0 || first.compare("v") == 0) {
                    is_verbose = true;
                    PrintColor(llvm::outs(), "Enable verbose mode");
                } else {
                    llvm::errs() << "Unknown plugin argument: '" << elem << "'!\n";
                    return false;
                }
            } else {
                if (first.compare("level") == 0) {
                    // ok
                } else {
                    llvm::errs() << "The argument '" << elem << "' is not supported via command line!\n";
                    return false;
                }
            }
        }
        return true;
    }
};

void Verbose(SourceLocation loc, std::string_view str) {
    if (is_verbose && plugin) {
        llvm::outs() << LocToStr(loc, plugin->m_CI.getSourceManager()) << ": verbose: " << str.begin() << "\n";
    }
}

} // namespace

static ParsedAttrInfoRegistry::Add<TrustAttrInfo> A(TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE), "Memory safety plugin control attribute");
static FrontendPluginRegistry::Add<TrustPluginASTAction> S(TO_STR(STACK_CHECK_KEYWORD_ATTRIBUTE), "Memory safety plugin");




extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    llvm::errs() <<"registerPipelineStartEPCallback\n";
  static ::llvm::PassPluginLibraryInfo PluginInfo = {
      LLVM_PLUGIN_API_VERSION,
      "stack_check",
      "0.1",
      [](::llvm::PassBuilder &PB) {
        llvm::errs() <<"PassBuilder\n";
        PB.registerPipelineStartEPCallback(
            [](::llvm::ModulePassManager &MPM, ::llvm::OptimizationLevel) {
                llvm::errs() <<"ModulePassManager\n";
              MPM.addPass(DebugInjectorPass());
            });
      }};
  return PluginInfo;
}
