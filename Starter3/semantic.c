#include <cstdlib>
#include "semantic.h"
#include "common.h"

#define SEM_ERROR(n, fmt, ...) { \
  fprintf(errorFile, "SEMANTIC ERROR (line %d, column %d): " fmt "\n", n->line, n->column, ##__VA_ARGS__); \
  errorOccurred = true; \
}

typedef struct {
  std::vector<unsigned int> scope_id_stack;
} visit_data;

void semantic_preorder(node *n, void *data) {
  visit_data *vd = (visit_data *) data;

  switch (n->kind) {
  case SCOPE_NODE:
    vd->scope_id_stack.push_back(n->scope.scope_id);
    break;

  case DECLARATIONS_NODE:
    break;
  case DECLARATION_NODE:
    break;

  case STATEMENTS_NODE:
    break;
  case IF_STATEMENT_NODE:
    break;
  case ASSIGNMENT_NODE:
    break;
  case NESTED_SCOPE_NODE:
    break;

  case EXPRESSION_NODE:
    // EXPRESSION_NODE is an abstract node
    break;
  case UNARY_EXPRESSION_NODE:
    break;
  case BINARY_EXPRESSION_NODE:
    break;
  case INT_NODE:
    break;
  case FLOAT_NODE:
    break;
  case BOOL_NODE:
    break;
  case IDENT_NODE:
    break;
  case VAR_NODE:
    break;
  case FUNCTION_NODE:
    break;
  case CONSTRUCTOR_NODE:
    break;

  case TYPE_NODE:
    break;

  case ARGUMENT_NODE:
    break;

  default: break;
  }
}

void semantic_postorder(node *n, void *data) {
  visit_data *vd = (visit_data *) data;

  switch (n->kind) {
  case SCOPE_NODE:
    vd->scope_id_stack.pop_back();
    break;

  case DECLARATIONS_NODE:
    break;
  case DECLARATION_NODE:
    validate_declaration_node(vd->scope_id_stack, n);
    if (n->declaration.assignment_expr != NULL) {
      validate_declaration_assignment_node(vd->scope_id_stack, n);
    }
    break;

  case STATEMENTS_NODE:
    break;
  case IF_STATEMENT_NODE:
    // An if condition must be a boolean
    if (n->statement.if_else_statement.condition->expression.expr_type != TYPE_BOOL) {
      SEM_ERROR(n->statement.if_else_statement.condition,
                "If statement condition must be a boolean value");
    }
    break;
  case ASSIGNMENT_NODE:
    validate_assignment_node(vd->scope_id_stack, n);
    break;
  case NESTED_SCOPE_NODE:
    break;

  case EXPRESSION_NODE:
    // EXPRESSION_NODE is an abstract node
    break;
  case UNARY_EXPRESSION_NODE:
    validate_unary_expr_node(n);
    break;
  case BINARY_EXPRESSION_NODE:
    validate_binary_expr_node(n);
    break;
  case INT_NODE:
    break;
  case FLOAT_NODE:
    break;
  case BOOL_NODE:
    break;
  case IDENT_NODE:
    break;
  case VAR_NODE:
    validate_variable_node(vd->scope_id_stack, n);
    break;
  case FUNCTION_NODE:
    validate_function_node(n);
    break;
  case CONSTRUCTOR_NODE:
    validate_constructor_node(n);
    break;

  case TYPE_NODE:
    break;

  case ARGUMENT_NODE:
    break;

  default: break;
  }
}

void semantic_check(node *ast) {
  // Perform semantic analysis
  visit_data vd;
  ast_visit(ast, semantic_preorder, semantic_postorder, &vd);
}

/****** SEMANTIC VALIDATION FUNCTIONS ******/
symbol_type validate_binary_expr_node(node *binary_node, bool log_errors) {
  node *right = binary_node->expression.binary.right;
  node *left = binary_node->expression.binary.left;

  symbol_type r_type = right->expression.expr_type;
  symbol_type l_type = left->expression.expr_type;

  symbol_type r_base_type = get_base_type(r_type);
  symbol_type l_base_type = get_base_type(l_type);

  bool r_is_vec = r_type & TYPE_ANY_VEC;
  bool l_is_vec = l_type & TYPE_ANY_VEC;

  binary_op op = binary_node->expression.binary.op;

  // TODO: In alot of the cases below we can just log something and fall
  // through to the return statement at the bottom of the function. This
  // would remove alot of extra code and make it more readable

  // TODO: we can also print out the actual operator using get_binary_op_name()

  // For a binary op, base types must match
  if (r_base_type != l_base_type) {
    if (log_errors){
      SEM_ERROR(binary_node,
                "Binary operator %s has operands with incompatible base types %s and %s",
                get_binary_op_name(op),
                get_type_name(l_base_type),
                get_type_name(r_base_type));
    }
    return TYPE_UNKNOWN;
  }

  // Check for unknown input type
  if (l_type == TYPE_UNKNOWN || r_type == TYPE_UNKNOWN) {
    return TYPE_UNKNOWN;
  }

  switch (op) {
  case OP_AND: case OP_OR:
    // The types must be logical and equal
    if (r_type == l_type && r_base_type == TYPE_BOOL) {
      return r_type;
    } else {
      if(log_errors){
        SEM_ERROR(binary_node,
                  "Binary operator %s has operands with invalid types %s and %s, expected them to be equal and logical",
                  get_binary_op_name(op),
                  get_type_name(l_type),
                  get_type_name(r_type));
      };
      return TYPE_UNKNOWN;
    }
    break;
  case OP_PLUS: case OP_MINUS:
    // The types must be equal and arithmetic
    if (r_type == l_type && r_base_type != TYPE_BOOL) {
      return r_type;
    } else {
      if(log_errors){
        SEM_ERROR(binary_node,
                  "Binary operator %s has operands with invalid types %s and %s, expected them to be equal and arithmetic",
                  get_binary_op_name(op),
                  get_type_name(l_type),
                  get_type_name(r_type));
      };
      return TYPE_UNKNOWN;
    }
    break;
  case OP_DIV: case OP_XOR:
    // The types must be equal, non-vector and arithmetic
    if (r_type == l_type && !r_is_vec && !l_is_vec && r_base_type != TYPE_BOOL) {
      return r_type;
    } else {
      if(log_errors){
        SEM_ERROR(binary_node,
                  "Binary operator %s has operands with invalid types %s and %s, expected them to be equal, scalar, and arithmetic",
                  get_binary_op_name(op),
                  get_type_name(l_type),
                  get_type_name(r_type));
      }
      return TYPE_UNKNOWN;
    }
    break;
  case OP_MUL:
    if (r_base_type == TYPE_BOOL) {
      if(log_errors){
        SEM_ERROR(binary_node,
                  "Binary operator %s has operands with invalid types %s and %s, expected them to be arithmetic",
                  get_binary_op_name(op),
                  get_type_name(l_type),
                  get_type_name(r_type));
      }
      return TYPE_UNKNOWN;
    }
    if (l_is_vec && r_is_vec) {
      if (r_type == l_type) {
        return r_type;
      } else {
        // trying to multiply 2 vectors with different size
        if(log_errors){
          SEM_ERROR(binary_node,
                    "Binary operator %s has operands with invalid types %s and %s, expected them to be vectors of the same dimension",
                    get_binary_op_name(op),
                    get_type_name(l_type),
                    get_type_name(r_type));
        }
        return TYPE_UNKNOWN;
      }
    } else if (l_is_vec && !r_is_vec) {
      return l_type;
    } else if (!l_is_vec && r_is_vec) {
      return r_type;
    } else /* !l_is_vec && !r_is_vec */ {
      return r_base_type;
    }
  case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ:
    // The types must be equal, non-vector and arithmetic
    if (r_type == l_type && !r_is_vec && !l_is_vec && r_base_type != TYPE_BOOL) {
        return TYPE_BOOL;
    } else {
      if(log_errors){
        SEM_ERROR(binary_node,
                  "Binary operator %s has operands with invalid types %s and %s, expected them to be equal, scalar, and arithmetic",
                  get_binary_op_name(op),
                  get_type_name(l_type),
                  get_type_name(r_type));
      }
      return TYPE_UNKNOWN;
    }
  case OP_EQ: case OP_NEQ:
    // The types for OP_EQ and OP_NEQ must be equal and arithmetic
    if (r_type == l_type && r_base_type != TYPE_BOOL) {
      return TYPE_BOOL;
    }
    if(log_errors && r_type != l_type){
      SEM_ERROR(binary_node,
                "Binary operator %s has operands with invalid types %s and %s, expected them to be equal",
                get_binary_op_name(op),
                get_type_name(l_type),
                get_type_name(r_type));
    }
    if(log_errors && r_base_type == TYPE_BOOL){
      SEM_ERROR(binary_node,
                "Binary operator %s has operands with invalid types %s and %s, expected them to be arithmetic",
                get_binary_op_name(op),
                get_type_name(l_type),
                get_type_name(r_type));
    }
    return TYPE_UNKNOWN;
  default:
    break;
  }
  return TYPE_UNKNOWN;
}

symbol_type validate_unary_expr_node(node *unary_node, bool log_errors) {
  symbol_type type = unary_node->expression.unary.right->expression.expr_type;
  symbol_type base_type = get_base_type(type);

  switch (unary_node->expression.unary.op) {
  case OP_UMINUS:
    // Unary minus is an arithmetic operator, so only allow
    // base types of int and float
    if (base_type == TYPE_INT || base_type == TYPE_FLOAT) {
      // Return the original type (preserving scalar/vector)
      return type;
    } else if (log_errors) {
      SEM_ERROR(unary_node,
                "Operand of unary operator %s is of type %s but expected an arithmetic type",
                get_unary_op_name(unary_node->expression.unary.op),
                get_type_name(type));
    }
    break;
  case OP_NOT:
    // Unary not is a logical operator, so only allow a boolean
    // base type
    if (base_type == TYPE_BOOL) {
      // Return the original type (preserving scalar/vector)
      return type;
    } else if (log_errors) {
      SEM_ERROR(unary_node,
                "Operand of unary operator %s is of type %s but expected a logical type",
                get_unary_op_name(unary_node->expression.unary.op),
                get_type_name(type));
    }
    break;
  default: break;
  }
  // Fall through to an unknown type
  return TYPE_UNKNOWN;
}

void validate_function_node(node *func_node, bool log_errors) {
  node *first_arg = func_node->expression.function.arguments,
       *second_arg = first_arg != NULL ? first_arg->argument.next_argument : NULL;

  node *first_expr = first_arg != NULL ? first_arg->argument.expression : NULL,
       *second_expr = second_arg != NULL ? second_arg->argument.expression : NULL;

  symbol_type first_type = first_expr != NULL ? first_expr->expression.expr_type : TYPE_UNKNOWN,
              second_type = second_expr != NULL ? second_expr->expression.expr_type : TYPE_UNKNOWN;

  int num_args = first_arg != NULL ? first_arg->argument.num_arguments : 0;

  switch (func_node->expression.function.func_id) {
  case FUNC_DP3:
    if (num_args > 2) {
      if (log_errors) {
        SEM_ERROR(func_node, "Too many arguments provided for dp3");
      }
    } else if (num_args < 2) {
      if (log_errors) {
        SEM_ERROR(func_node, "Too few arguments provided for dp3");
      }
    } else {
      switch (first_type) {
      case TYPE_VEC3: case TYPE_VEC4: case TYPE_IVEC3: case TYPE_IVEC4:
        if (second_type != first_type) {
          if (log_errors) {
            SEM_ERROR(second_expr,
                      "Argument 2 of dp3 has type %s but expected %s",
                      get_type_name(second_type),
                      get_type_name(first_type));
          }
        }
        break;
      default:
        if (log_errors) {
          SEM_ERROR(first_expr,
                    "Argument 1 of dp3 has type %s but expected one of vec3, vec4, ivec3, ivec4",
                    get_type_name(first_type));
        }
        break;
      }
    }
    break;
  case FUNC_RSQ:
    if (num_args > 1) {
      if (log_errors) {
        SEM_ERROR(func_node, "Too many arguments provided for rsq");
      }
    } else if (num_args < 1) {
      if (log_errors) {
        SEM_ERROR(func_node, "Too few arguments provided for rsq");
      }
    } else {
      if (first_type != TYPE_INT && first_type != TYPE_FLOAT) {
        if (log_errors) {
          SEM_ERROR(first_expr,
                    "Argument 1 of rsq has type %s but expected one of int, float",
                    get_type_name(first_type));
        }
      }
    }
    break;
  case FUNC_LIT:
    if (num_args > 1) {
      if (log_errors) {
        SEM_ERROR(func_node, "Too many arguments provided for lit");
      }
    } else if (num_args < 1) {
      if (log_errors) {
        SEM_ERROR(func_node, "Too few arguments provided for lit");
      }
    } else {
      if (first_type != TYPE_VEC4) {
        if (log_errors) {
          SEM_ERROR(first_expr,
                    "Argument 1 of lit has type %s but expected vec4",
                    get_type_name(first_type));
        }
      }
    }
    break;
  }
}

void validate_constructor_node(node *constructor_node, bool log_errors) {
  symbol_type constructor_type = constructor_node->expression.constructor.type->type.type;
  symbol_type expected_arg_type = get_base_type(constructor_type);

  node *first_arg = constructor_node->expression.constructor.arguments;
  int num_args = first_arg != NULL ? first_arg->argument.num_arguments : 0,
      expected_num_args;

  if (constructor_type & TYPE_ANY_VEC) {
    // We know that the type is a vector, the dimension of the type is given
    // by the lowest 3 bits of the type
    expected_num_args = constructor_type & 0x7;
  } else {
    switch (constructor_type) {
    case TYPE_INT: case TYPE_FLOAT: case TYPE_BOOL:
      expected_num_args = 1;
      break;
    default:
      // This should never happen.
      if (log_errors) {
        SEM_ERROR(constructor_node, "Unknown constructor type");
      }
      return;
    }
  }

  if (num_args > expected_num_args) {
    if (log_errors) {
      SEM_ERROR(constructor_node,
                "Too many arguments provided for %s constructor",
                get_type_name(constructor_type));
    }
  } else if (num_args < expected_num_args) {
    if (log_errors) {
      SEM_ERROR(constructor_node,
                "Too few arguments provided for %s constructor",
                get_type_name(constructor_type));
    }
  }

  node *cur_arg;
  int i;
  for (i = 1, cur_arg = constructor_node->expression.constructor.arguments;
       i <= expected_num_args && cur_arg != NULL;
       i++, cur_arg = cur_arg->argument.next_argument) {
    node *cur_expr = cur_arg->argument.expression;
    symbol_type arg_type = cur_expr->expression.expr_type;
    if (arg_type != expected_arg_type) {
      if (log_errors) {
        SEM_ERROR(cur_expr,
                  "Argument %d of %s constructor has type %s but expected type %s",
                  i,
                  get_type_name(constructor_type),
                  get_type_name(arg_type),
                  get_type_name(expected_arg_type));
      }
    }
  }
}

void validate_variable_index_node(node *var_node, bool log_errors) {
  node *ident = var_node->expression.variable.identifier;
  node *index = var_node->expression.variable.index;

  symbol_type var_type = ident->expression.expr_type;
  int i = index->expression.int_expr.val;
  if (var_type & TYPE_ANY_VEC) {
    // The dimension of the vector is encoded in the lowest 3 bits of the type
    int dim = var_type & 0x7;
    if (i >= dim) {
      if (log_errors) {
        SEM_ERROR(var_node,
                  "Variable %s of type %s indexed at %d but only has dimension %d",
                  ident->expression.ident.val,
                  get_type_name(var_type),
                  i,
                  dim);
      }
    }
  } else {
    if (log_errors) {
      SEM_ERROR(var_node,
                "Variable %s of type %s cannot be indexed as it is not a vector",
                ident->expression.ident.val,
                get_type_name(var_type));
    }
  }
}

void validate_declaration_node(std::vector<unsigned int> &scope_id_stack,
                                          node *decl_node,
                                          bool log_errors) {
  node *ident = decl_node->declaration.identifier;
  char *symbol_name = ident->expression.ident.val;
  symbol_info &sym_info = get_symbol_info(scope_id_stack, symbol_name);
  if(sym_info.already_declared == true){
    // report error
    if(log_errors){
      SEM_ERROR(decl_node, "Variable %s has alreay been declared in this scope", symbol_name);
    }
  } else {
    sym_info.already_declared = true;
  }
}

void validate_declaration_assignment_node(std::vector<unsigned int> &scope_id_stack,
                                          node *decl_node,
                                          bool log_errors) {
  node *ident = decl_node->declaration.identifier;
  node *expr = decl_node->declaration.assignment_expr;

  symbol_type var_type = ident->expression.expr_type;
  symbol_type expr_type = expr->expression.expr_type;

  // If the expr_type is unknown then the error should be reported somewhere else
  if (expr_type == TYPE_UNKNOWN) {
    return;
  }

  // Ensure that variables declared as const are assigned const values
  if (decl_node->declaration.is_const && !is_const_expr(scope_id_stack, expr)) {
    if (log_errors) {
      SEM_ERROR(decl_node,
                "Const variable %s cannot be assigned a non-const value",
                ident->expression.ident.val);
    }
  }

  // If expr_type isn't the same as var_type and can't be widened to var_type
  if (var_type != expr_type && get_base_type(var_type) != expr_type) {
    if (log_errors) {
      SEM_ERROR(decl_node,
                "Variable %s of type %s cannot be assigned a value of type %s",
                ident->expression.ident.val,
                get_type_name(var_type),
                get_type_name(expr_type));
    }
  }
}

void validate_assignment_node(std::vector<unsigned int> &scope_id_stack,
                              node *assign_node,
                              bool log_errors) {
  node *var = assign_node->statement.assignment.variable;
  node *expr = assign_node->statement.assignment.expression;
  node *ident = var->expression.variable.identifier;

  symbol_type var_type = var->expression.expr_type;
  symbol_type expr_type = expr->expression.expr_type;

  // TODO: We should be making checks for TYPE_UNKNOWN everywhere so we don't log
  // errors for unknown type. Or maybe we shouldn't, i dont know

  // If var_type or expr_type are unknown then the error should be
  // reported somewhere else
  if (var_type == TYPE_UNKNOWN || expr_type == TYPE_UNKNOWN) {
    return;
  }

  // Ensure that variables declared as readonly cannot be assigned to
  if (get_symbol_info(scope_id_stack, ident->expression.ident.val).read_only) {
    if (log_errors) {
      SEM_ERROR(assign_node,
                "Read-only variable %s cannot be assigned to",
                ident->expression.ident.val);
    }
  }

  // If expr_type isn't the same as var_type and can't be widened to var_type
  if (var_type != expr_type && get_base_type(var_type) != expr_type) {
    if (log_errors) {
      SEM_ERROR(assign_node,
                "Variable %s of type %s cannot be assigned a value of type %s",
                ident->expression.ident.val,
                get_type_name(var_type),
                get_type_name(expr_type));
    }
  }
}

void validate_variable_node(std::vector<unsigned int> &scope_id_stack,
                            node *var_node,
                            bool log_errors) {
  // Validate the index of the variable, if there is one
  if (var_node->expression.variable.index != NULL) {
    validate_variable_index_node(var_node);
  }

  node *ident = var_node->expression.variable.identifier;
  symbol_info sym_info = get_symbol_info(scope_id_stack, ident->expression.ident.val);

  // If the variable has TYPE_UNKNOWN then it wasn't declared
  if (sym_info.type == TYPE_UNKNOWN) {
    if (log_errors) {
      SEM_ERROR(var_node,
                "Undeclared variable %s",
                ident->expression.ident.val);
    }
  } else if (!sym_info.already_declared) {
    // If the variable has a type, but isn't already_declared, then it hasn't been declared yet
    if (log_errors) {
      SEM_ERROR(var_node,
                "Variable %s used before it was declared",
                ident->expression.ident.val);
    }
  }

  // If we have a write-only variable, it must appear on the LHS of an assignment node
  if (sym_info.write_only &&
      (var_node->parent->kind != ASSIGNMENT_NODE ||
      var_node->parent->statement.assignment.expression == var_node)) {
    // If the RHS of an ASSIGNMENT_NODE was exactly the var_node, then the parent would
    // still be an ASSIGNMENT_NODE. We thus need the second condition to make sure that
    // the VAR_NODE is appearing on the LHS. Because of lazy evaluation, we know that
    // if the third condition is evaluated, the second must have been false.
    if (log_errors) {
      SEM_ERROR(var_node,
                "Write-only variable %s cannot be read from",
                ident->expression.ident.val);
    }
  }
}

/****** SEMANTIC TYPE FUNCTIONS ******/
symbol_type get_binary_expr_type(node *binary_node) {
  return validate_binary_expr_node(binary_node, false);
}

symbol_type get_unary_expr_type(node *unary_node) {
  return validate_unary_expr_node(unary_node, false);
}

symbol_type get_function_return_type(node *func_node) {
  node *first_arg = func_node->expression.function.arguments;

  switch (func_node->expression.function.func_id) {
  case FUNC_DP3:
    // Look at the first argument to determine the return type.
    if (first_arg != NULL) {
      switch (first_arg->argument.expression->expression.expr_type) {
      // If the first argument is TYPE_VEC[43], return TYPE_FLOAT
      case TYPE_VEC4: case TYPE_VEC3:
        return TYPE_FLOAT;
      // If the first argument is TYPE_IVEC[43], return TYPE_INT
      case TYPE_IVEC4: case TYPE_IVEC3:
        return TYPE_INT;
      // Otherwise fall through to TYPE_UNKNOWN
      default: break;
      }
    }
    break;
  case FUNC_RSQ:
    return TYPE_FLOAT;
  case FUNC_LIT:
    return TYPE_VEC4;
  }
  return TYPE_UNKNOWN;
}

symbol_type get_base_type(symbol_type type) {
  if (type & TYPE_VEC) {
    return TYPE_FLOAT;
  } else if (type & TYPE_IVEC) {
    return TYPE_INT;
  } else if (type & TYPE_BVEC) {
    return TYPE_BOOL;
  } else {
    return type;
  }
}

bool is_const_expr(std::vector<unsigned int> &scope_id_stack, node *expr_node) {
  // TODO: Evaluate constant expressions
  switch (expr_node->kind) {
  case UNARY_EXPRESSION_NODE:
    break;
  case BINARY_EXPRESSION_NODE:
    break;
  case INT_NODE: case FLOAT_NODE: case BOOL_NODE:
    return true;
  case IDENT_NODE:
    return get_symbol_info(scope_id_stack, expr_node->expression.ident.val).constant;
  case VAR_NODE:
    return is_const_expr(scope_id_stack, expr_node->expression.variable.identifier);
  case FUNCTION_NODE:
    break;
  case CONSTRUCTOR_NODE:
    break;
  default: break;
  }
  return false;
}
