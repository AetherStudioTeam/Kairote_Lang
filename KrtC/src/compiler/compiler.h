#ifndef KRT_COMPILER_H
#define KRT_COMPILER_H

#include "Frontend/FrontendTemp/FrontendTemp/lexer/Tokenizer.h"
#include "Frontend/FrontendTemp/FrontendTemp/parser/Parser.h"
#include "Frontend/FrontendTemp/FrontendTemp/parser/Ast.h"
#include "Frontend/FrontendTemp/FrontendTemp/semantic/SemanticAnalyzer.h"
#include "Frontend/FrontendTemp/FrontendTemp/semantic/SymbolTable.h"
#include "Frontend/FrontendTemp/FrontendTemp/semantic/Generics.h"
#include "Frontend/FrontendTemp/FrontendTemp/semantic/TypeChecker.h"
#include "Frontend/FrontendTemp/FrontendTemp/semantic/NameMangling.h"
#include "Middle/Ir/Ir.h"
#include "Backend/X86/X86Codegen.h"
#include "Driver/Compiler.h"
#include "Driver/ParallelCompiler.h"
#include "Driver/Preprocessor.h"
#include "Driver/Project.h"

#endif 