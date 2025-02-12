//===--- SILLocation.cpp - Location information for SIL nodes -------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/SIL/SILLocation.h"
#include "swift/SIL/SILModule.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Expr.h"
#include "swift/AST/Pattern.h"
#include "swift/AST/Stmt.h"
#include "swift/AST/Module.h"
#include "swift/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"

using namespace swift;

static_assert(sizeof(SILLocation) <= 2 * sizeof(void *),
              "SILLocation must stay small");

SILLocation::FilenameAndLocation *SILLocation::FilenameAndLocation::
alloc(unsigned line, unsigned column, StringRef filename, SILModule &module) {
  return new (module) FilenameAndLocation(line, column, filename);
}

void SILLocation::FilenameAndLocation::dump() const { print(llvm::dbgs()); }

void SILLocation::FilenameAndLocation::print(raw_ostream &OS) const {
  OS << filename << ':' << line << ':' << column;
}

SourceLoc SILLocation::getSourceLoc() const {
  if (isSILFile())
    return storage.sourceLoc;

  // Don't crash if the location is a FilenameAndLocation.
  // TODO: this is a workaround until rdar://problem/25225083 is implemented.
  if (getStorageKind() == FilenameAndLocationKind)
    return SourceLoc();

  return getSourceLoc(getPrimaryASTNode());
}

SourceLoc SILLocation::getSourceLoc(ASTNodeTy N) const {
  if (N.isNull())
    return SourceLoc();

  if (alwaysPointsToEnd() ||
      is<CleanupLocation>() ||
      is<ImplicitReturnLocation>())
    return getEndSourceLoc(N);

  // Use the start location for the ReturnKind.
  if (is<ReturnLocation>())
    return getStartSourceLoc(N);

  if (auto *decl = N.dyn_cast<Decl*>())
    return decl->getLoc();
  if (auto *expr = N.dyn_cast<Expr*>())
    return expr->getLoc();
  if (auto *stmt = N.dyn_cast<Stmt*>())
    return stmt->getStartLoc();
  if (auto *patt = N.dyn_cast<Pattern*>())
    return patt->getStartLoc();
  llvm_unreachable("impossible SILLocation");
}

SourceLoc SILLocation::getSourceLocForDebugging() const {
  if (hasASTNodeForDebugging())
    return getSourceLoc(storage.extendedASTNodeLoc->forDebugging);

  if (isNull())
    return SourceLoc();

  if (isSILFile())
    return storage.sourceLoc;

  return getSourceLoc(getPrimaryASTNode());
}

SourceLoc SILLocation::getStartSourceLoc() const {
  if (isAutoGenerated())
    return SourceLoc();
  if (isSILFile())
    return storage.sourceLoc;
  return getStartSourceLoc(getPrimaryASTNode());
}

SourceLoc SILLocation::getStartSourceLoc(ASTNodeTy N) {
  if (auto *decl = N.dyn_cast<Decl*>())
    return decl->getStartLoc();
  if (auto *expr = N.dyn_cast<Expr*>())
    return expr->getStartLoc();
  if (auto *stmt = N.dyn_cast<Stmt*>())
    return stmt->getStartLoc();
  if (auto *patt = N.dyn_cast<Pattern*>())
    return patt->getStartLoc();
  llvm_unreachable("impossible SILLocation");
}

SourceLoc SILLocation::getEndSourceLoc() const {
  if (isAutoGenerated())
    return SourceLoc();
  if (isSILFile())
    return storage.sourceLoc;
  return getEndSourceLoc(getPrimaryASTNode());
}

SourceLoc SILLocation::getEndSourceLoc(ASTNodeTy N) {
  if (auto decl = N.dyn_cast<Decl*>())
    return decl->getEndLoc();
  if (auto expr = N.dyn_cast<Expr*>())
    return expr->getEndLoc();
  if (auto stmt = N.dyn_cast<Stmt*>())
    return stmt->getEndLoc();
  if (auto patt = N.dyn_cast<Pattern*>())
    return patt->getEndLoc();
  llvm_unreachable("impossible SILLocation");
}

DeclContext *SILLocation::getAsDeclContext() const {
  if (!isASTNode())
    return nullptr;
  if (auto *D = getAsASTNode<Decl>())
    return D->getInnermostDeclContext();
  if (auto *E = getAsASTNode<Expr>())
    if (auto *DC = dyn_cast<AbstractClosureExpr>(E))
      return DC;
  return nullptr;
}

SILLocation::FilenameAndLocation SILLocation::decode(SourceLoc Loc,
                                              const SourceManager &SM,
                                              bool ForceGeneratedSourceToDisk) {
  FilenameAndLocation DL;
  if (Loc.isValid()) {
    DL.filename = SM.getDisplayNameForLoc(Loc, ForceGeneratedSourceToDisk);
    std::tie(DL.line, DL.column) = SM.getPresumedLineAndColumnForLoc(Loc);
  }
  return DL;
}

SILLocation::FilenameAndLocation *SILLocation::getCompilerGeneratedLoc() {
  static FilenameAndLocation compilerGenerated({0, 0, "<compiler-generated>"});
  return &compilerGenerated;
}

static void printSourceLoc(SourceLoc loc, raw_ostream &OS) {
  if (!loc.isValid()) {
    OS << "<invalid loc>";
    return;
  }
  const char *srcPtr = (const char *)loc.getOpaquePointerValue();
  unsigned len = strnlen(srcPtr, 20);
  if (len < 20) {
    OS << '"' << StringRef(srcPtr, len) << '"';
  } else {
    OS << '"' << StringRef(srcPtr, 20) << "[...]\"";
  }
}

void SILLocation::dump() const {
  if (isNull()) {
    llvm::dbgs() << "<no loc>";
    return;
  }
  if (auto D = getAsASTNode<Decl>())
    llvm::dbgs() << Decl::getKindName(D->getKind()) << "Decl @ ";
  if (auto E = getAsASTNode<Expr>())
    llvm::dbgs() << Expr::getKindName(E->getKind()) << "Expr @ ";
  if (auto S = getAsASTNode<Stmt>())
    llvm::dbgs() << Stmt::getKindName(S->getKind()) << "Stmt @ ";
  if (auto P = getAsASTNode<Pattern>())
    llvm::dbgs() << Pattern::getKindName(P->getKind()) << "Pattern @ ";

  if (isFilenameAndLocation()) {
    getFilenameAndLocation()->dump();
  } else {
    printSourceLoc(getSourceLoc(), llvm::dbgs());
  }

  if (isAutoGenerated())     llvm::dbgs() << ":auto";
  if (alwaysPointsToEnd())   llvm::dbgs() << ":end";
  if (isInPrologue())        llvm::dbgs() << ":prologue";
  if (isSILFile())           llvm::dbgs() << ":sil";
  if (hasASTNodeForDebugging()) {
    llvm::dbgs() << ":debug[";
    printSourceLoc(getSourceLocForDebugging(), llvm::dbgs());
    llvm::dbgs() << "]\n";
  }
}

void SILLocation::print(raw_ostream &OS, const SourceManager &SM) const {
  if (isNull()) {
    OS << "<no loc>";
  } else if (isFilenameAndLocation()) {
    getFilenameAndLocation()->print(OS);
  } else {
    getSourceLoc().print(OS, SM);
  }
}

void SILLocation::print(raw_ostream &OS) const {
  if (isNull()) {
    OS << "<no loc>";
  } else if (isFilenameAndLocation()) {
    getFilenameAndLocation()->print(OS);
  } else if (DeclContext *dc = getAsDeclContext()){
    getSourceLoc().print(OS, dc->getASTContext().SourceMgr);
  } else {
    printSourceLoc(getSourceLoc(), OS);
  }
}

RegularLocation::RegularLocation(Stmt *S, Pattern *P, SILModule &Module) :
  SILLocation(new (Module) ExtendedASTNodeLoc(S, P), RegularKind) {}

SILLocation::ExtendedASTNodeLoc *
RegularLocation::getDebugOnlyExtendedASTNodeLoc(SILLocation L,
                                                SILModule &Module) {
  if (auto D = L.getAsASTNode<Decl>())
    return new (Module) ExtendedASTNodeLoc((Decl *)nullptr, D);
  if (auto E = L.getAsASTNode<Expr>())
    return new (Module) ExtendedASTNodeLoc((Decl *)nullptr, E);
  if (auto S = L.getAsASTNode<Stmt>())
    return new (Module) ExtendedASTNodeLoc((Decl *)nullptr, S);
  auto P = L.getAsASTNode<Pattern>();
  return new (Module) ExtendedASTNodeLoc((Decl *)nullptr, P);
}

SILLocation::ExtendedASTNodeLoc *
RegularLocation::getDiagnosticOnlyExtendedASTNodeLoc(SILLocation L,
                                                     SILModule &Module) {
  if (auto D = L.getAsASTNode<Decl>())
    return new (Module) ExtendedASTNodeLoc(D, (Decl *)nullptr);
  if (auto E = L.getAsASTNode<Expr>())
    return new (Module) ExtendedASTNodeLoc(E, (Decl *)nullptr);
  if (auto S = L.getAsASTNode<Stmt>())
    return new (Module) ExtendedASTNodeLoc(S, (Decl *)nullptr);
  auto P = L.getAsASTNode<Pattern>();
  return new (Module) ExtendedASTNodeLoc(P, (Decl *)nullptr);
}

RegularLocation::RegularLocation(SILLocation ForDebuggingOrDiagnosticsOnly,
                                 SILModule &Module, bool isForDebugOnly)
    : SILLocation(isForDebugOnly ? getDebugOnlyExtendedASTNodeLoc(
                                       ForDebuggingOrDiagnosticsOnly, Module)
                                 : getDiagnosticOnlyExtendedASTNodeLoc(
                                       ForDebuggingOrDiagnosticsOnly, Module),
                  RegularKind) {
  markAutoGenerated();
}

ReturnLocation::ReturnLocation(ReturnStmt *RS) :
  SILLocation(ASTNodeTy(RS), ReturnKind) {}

ReturnLocation::ReturnLocation(BraceStmt *BS) :
  SILLocation(ASTNodeTy(BS), ReturnKind) {}

ImplicitReturnLocation::ImplicitReturnLocation(AbstractClosureExpr *E)
  : SILLocation(ASTNodeTy(E), ImplicitReturnKind) { }

ImplicitReturnLocation::ImplicitReturnLocation(ReturnStmt *S)
  : SILLocation(ASTNodeTy(S), ImplicitReturnKind) { }

ImplicitReturnLocation::ImplicitReturnLocation(AbstractFunctionDecl *AFD)
  : SILLocation(ASTNodeTy(AFD), ImplicitReturnKind) { }

ImplicitReturnLocation::ImplicitReturnLocation(SILLocation L)
  : SILLocation(L, ImplicitReturnKind) {
  assert(L.isASTNode<Expr>() ||
         L.isASTNode<ValueDecl>() ||
         L.isASTNode<PatternBindingDecl>() ||
         L.isNull());
}

std::string SILDebugLocation::getDebugDescription() const {
  std::string str;
  llvm::raw_string_ostream os(str);
  SILLocation loc = getLocation();
  loc.print(os);
#ifndef NDEBUG
  if (const SILDebugScope *scope = getScope()) {
    if (DeclContext *dc = loc.getAsDeclContext()) {
      os << ", scope=";
      scope->print(dc->getASTContext().SourceMgr, os, /*indent*/ 2);
    } else {
      os << ", scope=?";
    }
  }
#endif
  return str;
}
