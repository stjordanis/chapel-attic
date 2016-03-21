/*
 * Copyright 2004-2016 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "addAutoDestroyCalls.h"

#include "astutil.h"
#include "expr.h"
#include "resolution.h"
#include "stlutil.h"
#include "stmt.h"
#include "symbol.h"

#include <vector>

/************************************* | **************************************
*                                                                             *
* Track the state of lexical scopes during the execution of a function.       *
*                                                                             *
*   1) We only track variables that have an autoDestroy flag                  *
*                                                                             *
*   2) The compiler introduces "formal temps" to manage formals with out and  *
*      in-out concrete intents.  If a function has multiple returns then any  *
*      formal temps must be handled differently from other locals.            *
*                                                                             *
************************************** | *************************************/

class Scope
{
public:
                           Scope(const Scope*     parent,
                                 const BlockStmt* block);

  void                     variableAdd(VarSymbol* var);

  void                     insertAutoDestroys(FnSymbol* fn,
                                              Expr*     refStmt);

private:
  void                     variablesDestroy(Expr*      refStmt,
                                            VarSymbol* excludeVar)       const;

  const Scope*             mParent;
  const BlockStmt*         mBlock;

  std::vector<VarSymbol*>  mFormalTemps;         // Temps for out/inout formals
  std::vector<VarSymbol*>  mLocals;
};

static void walkBlock(FnSymbol* fn, Scope* parent, BlockStmt* block);
static bool isAutoDestroyedVariable(Symbol* sym);

/************************************* | **************************************
*                                                                             *
* Entry point                                                                 *
*                                                                             *
************************************** | *************************************/

static void cullForDefaultConstructor(FnSymbol* fn);

void addAutoDestroyCalls() {
  forv_Vec(FnSymbol, fn, gFnSymbols) {
    if (fn->hasFlag(FLAG_DEFAULT_CONSTRUCTOR) == true) {
      cullForDefaultConstructor(fn);
    }

    walkBlock(fn, NULL, fn->body);
  }
}

//
// Retain current approach for pruning auto-destroy flags in constructors.
// We do not destroy variables that are written in to the fields of the
// object being initialized.
//

static void cullForDefaultConstructor(FnSymbol* fn) {
  if (isVarSymbol(fn->getReturnSymbol()) == true) {
    Map<Symbol*, Vec<SymExpr*>*> defMap;
    Map<Symbol*, Vec<SymExpr*>*> useMap;
    std::vector<DefExpr*>        defs;

    buildDefUseMaps(fn, defMap, useMap);

    collectDefExprs(fn, defs);

    for_vector(DefExpr, def, defs) {
      if (VarSymbol* var = toVarSymbol(def->sym)) {
        if (var->hasFlag(FLAG_INSERT_AUTO_DESTROY) == true) {
          // Look for a use in a PRIM_SET_MEMBER where the field is a record
          // type, and remove the flag. (We don't actually check that var is
          // of record type, because chpl__autoDestroy() is a NO-OP when
          // applied to all other types.
          for_uses(se, useMap, var) {
            CallExpr* call = toCallExpr(se->parentExpr);

            if (call->isPrimitive(PRIM_SET_MEMBER) == true &&
                toSymExpr(call->get(3))->var       == var) {
              var->removeFlag(FLAG_INSERT_AUTO_DESTROY);
            }
          }
        }
      }
    }

    freeDefUseMaps(defMap, useMap);
  }
}

/************************************* | **************************************
*                                                                             *
* Walk the statements for a function's top-level BlockStmt                    *
*                                                                             *
*   1) Collect variables that are marked to be auto destroyed                 *
*                                                                             *
*   2) Handle recursion in to nested sub blocks                               *
*                                                                             *
*   3) Insert auto destroy calls at the end of the block                      *
*                                                                             *
************************************** | *************************************/

static VarSymbol* definesAnAutoDestroyedVariable(const Expr* stmt);

static void walkBlock(FnSymbol*  fn,
                      Scope*     parent,
                      BlockStmt* block) {
  Scope scope(parent, block);

  for_alist(stmt, block->body) {
    //
    // Handle the current statement
    //

    // Collect variables that should be autoDestroyed
    if (VarSymbol* var = definesAnAutoDestroyedVariable(stmt)) {
      scope.variableAdd(var);

    // Recurse in to a BlockStmt (or sub-classes of BlockStmt e.g. a loop)
    } else if (BlockStmt* subBlock = toBlockStmt(stmt)) {
      walkBlock(fn, &scope, subBlock);

    // Recurse in to the BlockStmt(s) of a CondStmt
    } else if (CondStmt*  cond     = toCondStmt(stmt))  {
      walkBlock(fn, &scope, cond->thenStmt);

      if (cond->elseStmt != NULL)
        walkBlock(fn, &scope, cond->elseStmt);
    }

    //
    // Handle the end of a block
    //
    if (stmt->next == NULL) {
      GotoStmt* gotoStmt = toGotoStmt(stmt);

      // The main block for a function or a simple sub-block
      if (parent == NULL || gotoStmt == NULL) {
        scope.insertAutoDestroys(fn, stmt);

      // Currently unprepared for a nested scope that ends in a goto
      } else {
        switch (gotoStmt->gotoTag) {
          case GOTO_RETURN:
          case GOTO_CONTINUE:
          case GOTO_BREAK:
            scope.insertAutoDestroys(fn, stmt);
            break;

          case GOTO_NORMAL:
          case GOTO_GETITER_END:
          case GOTO_ITER_RESUME:
          case GOTO_ITER_END:
           // MDN 2016/03/18 Need to revisit these cases
           break;
        }
      }
    }
  }
}

// Is this a DefExpr that defines a variable that might be autoDestroyed?
static VarSymbol* definesAnAutoDestroyedVariable(const Expr* stmt) {
  VarSymbol* retval = NULL;

  if (const DefExpr* expr = toConstDefExpr(stmt)) {
    if (VarSymbol* var = toVarSymbol(expr->sym))
      retval = (isAutoDestroyedVariable(var) == true) ? var : NULL;
  }

  return retval;
}

/************************************* | **************************************
*                                                                             *
*                                                                             *
*                                                                             *
************************************** | *************************************/

static VarSymbol* variableToExclude(FnSymbol*  fn, Expr* refStmt);
static bool       isReturnStmt(const Expr* stmt);
static BlockStmt* findBlockForTarget(GotoStmt* stmt);

Scope::Scope(const Scope* parent, const BlockStmt* block) {
  mParent = parent;
  mBlock  = block;
}

void Scope::variableAdd(VarSymbol* var) {
  if (var->hasFlag(FLAG_FORMAL_TEMP) == false)
    mLocals.push_back(var);
  else
    mFormalTemps.push_back(var);
}

// If the refStmt is a goto then we need to recurse
// to the block that contains the target of the goto
void Scope::insertAutoDestroys(FnSymbol* fn, Expr* refStmt) {
  GotoStmt*    gotoStmt   = toGotoStmt(refStmt);
  bool         recurse    = (gotoStmt != NULL) ? true : false;
  BlockStmt*   forTarget  = findBlockForTarget(gotoStmt);
  VarSymbol*   excludeVar = variableToExclude(fn, refStmt);
  const Scope* scope      = this;

  while (scope != NULL && scope->mBlock != forTarget) {
    scope->variablesDestroy(refStmt, excludeVar);

    scope = (recurse == true) ? scope->mParent : NULL;
  }
}

void Scope::variablesDestroy(Expr* refStmt, VarSymbol* excludeVar) const {
  // Handle the primary locals
  if (true) {
    bool   insertAfter = false;
    size_t count       = mLocals.size();

    // If this is a simple nested block, insert after the final stmt
    if (mParent != NULL && isGotoStmt(refStmt) == false) {
      insertAfter = true;
    }

    for (size_t i = 1; i <= count; i++) {
      VarSymbol* var = mLocals[count - i];

      if (var != excludeVar) {
        if (FnSymbol* autoDestroyFn = autoDestroyMap.get(var->type)) {
          SET_LINENO(var);

          CallExpr* autoDestroy = new CallExpr(autoDestroyFn, var);

          if (insertAfter == true)
            refStmt->insertAfter (autoDestroy);
          else
            refStmt->insertBefore(autoDestroy);
        }
      }
    }
  }

  // Handle the formal temps
  if (isReturnStmt(refStmt) == true) {
    size_t count = mFormalTemps.size();

    for (size_t i = 1; i <= count; i++) {
      VarSymbol* var = mFormalTemps[count - i];

      if (FnSymbol* autoDestroyFn = autoDestroyMap.get(var->type)) {
        SET_LINENO(var);

        refStmt->insertBefore(new CallExpr(autoDestroyFn, var));
      }
    }
  }
}

// Walk backwards from the current statement to determine if a sequence of
// moves have copied a variable that is marked for auto destruction in to
// the dedicated return-temp within the current scope.
//
// Note that the value we are concerned about may be copied in to one or
// more temporary variables between being copied to the return temp.
static VarSymbol* variableToExclude(FnSymbol*  fn, Expr* refStmt) {
  VarSymbol* retVar = toVarSymbol(fn->getReturnSymbol());
  VarSymbol* retval = NULL;

  if (retVar != NULL) {
    if (isUserDefinedRecord(retVar)    == true ||
        fn->hasFlag(FLAG_INIT_COPY_FN) == true) {
      VarSymbol* needle = retVar;
      Expr*      expr   = refStmt;

      // Walk backwards looking for the variable that is being returned
      while (retval == NULL && expr != NULL && needle != NULL) {
        if (CallExpr* move = toCallExpr(expr)) {
          if (move->isPrimitive(PRIM_MOVE) == true) {
            SymExpr*   lhs    = toSymExpr(move->get(1));
            VarSymbol* lhsVar = toVarSymbol(lhs->var);

            if (needle == lhsVar) {
              SymExpr*   rhs    = toSymExpr(move->get(2));
              VarSymbol* rhsVar = (rhs != NULL) ? toVarSymbol(rhs->var) : NULL;

              if (isAutoDestroyedVariable(rhsVar) == true)
                retval = rhsVar;
              else
                needle = rhsVar;
            }
          }
        }

        expr = expr->prev;
      }
    }
  }

  return retval;
}

static bool isReturnStmt(const Expr* stmt) {
  bool retval = false;

  if (const CallExpr* expr = toConstCallExpr(stmt))
    retval = expr->isPrimitive(PRIM_RETURN);

  return retval;
}

// Find the block stmt that encloses the target of this gotoStmt
static BlockStmt* findBlockForTarget(GotoStmt* stmt) {
  BlockStmt* retval = NULL;

  if (stmt != NULL && stmt->isGotoReturn() == false) {
    SymExpr*   labelSymExpr = toSymExpr(stmt->label);
    Expr*      ptr          = labelSymExpr->var->defPoint;

    while (ptr != NULL && isBlockStmt(ptr) == false)
      ptr = ptr->parentExpr;

    retval = toBlockStmt(ptr);

    INT_ASSERT(retval);
  }

  return retval;
}

/************************************* | **************************************
*                                                                             *
* Common utilities                                                            *
*                                                                             *
************************************** | *************************************/

static bool isAutoDestroyedVariable(Symbol* sym) {
  bool retval = false;

  if (VarSymbol* var = toVarSymbol(sym)) {
    if (var->hasFlag(FLAG_INSERT_AUTO_DESTROY) == true ||

        (var->hasFlag(FLAG_INSERT_AUTO_DESTROY_FOR_EXPLICIT_NEW) == true  &&
         var->type->symbol->hasFlag(FLAG_ITERATOR_RECORD)        == false &&
         isRefCountedType(var->type)                             == false)) {

      retval = (var->isType() == false && autoDestroyMap.get(var->type) != 0);
    }
  }

  return retval;
}

