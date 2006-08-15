#include "runtime.h"

ModuleSymbol* baseModule = NULL;
ModuleSymbol* fileModule = NULL;
ModuleSymbol* tupleModule = NULL;
ModuleSymbol* seqModule = NULL;
ModuleSymbol* standardModule = NULL;
ModuleSymbol* compilerModule = NULL;
ModuleSymbol* domainModule = NULL;

TypeSymbol* chpl_seq;
TypeSymbol* chpl_array;

VarSymbol* chpl_stdin = NULL;
VarSymbol* chpl_stdout = NULL;
VarSymbol* chpl_stderr = NULL;
