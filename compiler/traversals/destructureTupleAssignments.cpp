#include "destructureTupleAssignments.h"
#include "stmt.h"
#include "expr.h"
#include "type.h"
#include "stringutil.h"


void DestructureTupleAssignments::postProcessStmt(Stmt* stmt) {
  ExprStmt* assign_stmt = dynamic_cast<ExprStmt*>(stmt);

  if (!assign_stmt) {
    return;
  }

  AssignOp* assign_expr = dynamic_cast<AssignOp*>(assign_stmt->expr);

  if (!assign_expr) {
    return;
  }

  TupleType* left_type = dynamic_cast<TupleType*>(assign_expr->left->typeInfo()->getType());
  TupleType* right_type = dynamic_cast<TupleType*>(assign_expr->right->typeInfo()->getType());
  
  //RED -- temporary workaround the situation when a index is assigned to a tuple
  //the index->idxType may be a tuple and thus this test will pass
  //the downside is that index to tuple assignment still has to be destructured so this
  //leads to incorrect code generation
  if (left_type && right_type && (assign_expr->left->typeInfo() == assign_expr->right->typeInfo())) {
    return;
  }

  Tuple* left_tuple = dynamic_cast<Tuple*>(assign_expr->left);
  Tuple* right_tuple = dynamic_cast<Tuple*>(assign_expr->right);

  if (!left_type && !left_tuple && !right_type && !right_tuple) {
    return;
  }

  if (!left_type && !left_tuple) {
    INT_FATAL(stmt, "Tuple to non-tuple assignment should be caught earlier");
  }

  if (!right_type && !right_tuple) {
    INT_FATAL(stmt, "Non-tuple to tuple assignment should be caught earlier");
  }

  int left_n =
    (left_type) 
    ? left_type->components.n 
    : left_tuple->exprs->length();

  int right_n =
    (right_type) 
    ? right_type->components.n 
    : right_tuple->exprs->length();
  
  if (left_n != right_n) {
    INT_FATAL(stmt, "Non-matching tuple assign should be caught earlier");
  }

  if (assign_expr->type != GETS_NORM) {
    INT_FATAL(stmt, "Non-standard tuple assign should be caught earlier");
  }

  for (int i = 1; i <= left_n; i++) {
    TupleSelect* new_left =
      new TupleSelect(assign_expr->left->copy(), new IntLiteral(intstring(i), i));
    TupleSelect* new_right =
      new TupleSelect(assign_expr->right->copy(), new IntLiteral(intstring(i), i));
    AssignOp* new_assign_expr =
      new AssignOp(assign_expr->type, new_left, new_right);
    ExprStmt* new_assign_stmt = new ExprStmt(new_assign_expr);
    stmt->insertBefore(new_assign_stmt);
  }
  stmt->extract();
}
