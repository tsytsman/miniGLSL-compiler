#include "codegen.h"
#include "common.h"
#include <vector>
#include <map>
#include <string>

#define START_INSTR(instr) { \
  fprintf(outputFile, instr " "); \
}

#define INSTR(fmt, ...) { \
  fprintf(outputFile, fmt, ##__VA_ARGS__); \
}

#define FINISH_INSTR() { \
  fprintf(outputFile, ";\n"); \
}

// Map variables to registers
std::vector<std::map<std::string, std::string> > register_tables;

// Map expression nodes to intermediate register number
std::map<node *, unsigned int> intermediate_registers;

// Map constant nodes to PARAM registers
std::map<node *, unsigned int> constant_registers;

typedef struct {
  std::vector<unsigned int> scope_id_stack;
  unsigned int constant_id;
} visit_data;

std::pair<const char *, unsigned int> get_register_name(const std::vector<unsigned int> &scope_id_stack,
                                                        node *var);
void print_index(int i);
void print_register_name(const std::vector<unsigned int> &scope_id_stack,
                         node *n,
                         bool force_print_index = false);
bool is_register_temporary(node *expr);
void generate_expression(visit_data *vd, node *n);
void generate_const_int(visit_data *vd, node *int_expr);
void generate_const_float(visit_data *vd, node *float_expr);
void generate_if_statement_code(const std::vector<unsigned int> &scope_id_stack,
                                node *if_statement);
void generate_assignment_code(const std::vector<unsigned int> &scope_id_stack,
                              node *assign);
void generate_unary_expr_code(const std::vector<unsigned int> &scope_id_stack,
                               node *n);
void generate_binary_expr_code(const std::vector<unsigned int> &scope_id_stack,
                               node *n);
void generate_function_code(const std::vector<unsigned int> &scope_id_stack,
                            node *func);
void generate_constructor_code(const std::vector<unsigned int> &scope_id_stack,
                               node *assign);

void codegen_preorder(node *n, void *data) {
  visit_data *vd = (visit_data *) data;

  char *str;

  switch (n->kind) {
  case SCOPE_NODE:
    vd->scope_id_stack.push_back(n->scope.scope_id);
    if (vd->scope_id_stack.back() != 0) {
      register_tables.push_back(std::map<std::string, std::string>());
    }
    break;

  case DECLARATIONS_NODE:
    break;
  case DECLARATION_NODE:
    str = n->declaration.identifier->expression.ident.val;
    // Assign this variable to the corresponding register
    register_tables[vd->scope_id_stack.back()][str] = str;

    START_INSTR("TEMP");
    print_register_name(vd->scope_id_stack, n->declaration.identifier);
    FINISH_INSTR();
    break;

  case STATEMENTS_NODE:
    break;
  case IF_STATEMENT_NODE:
    // The if statement is responsible for generating its own condition code
    // since the if is generated in preorder but expressions are generated in
    // postorder
    generate_expression(vd, n->statement.if_else_statement.condition);
    generate_if_statement_code(vd->scope_id_stack, n);
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

void codegen_postorder(node *n, void *data) {
  visit_data *vd = (visit_data *) data;

  switch (n->kind) {
  case SCOPE_NODE:
    vd->scope_id_stack.pop_back();
    break;

  case DECLARATIONS_NODE:
    break;
  case DECLARATION_NODE:
    if (n->declaration.assignment_expr != NULL){
      START_INSTR("MOV");
      print_register_name(vd->scope_id_stack, n->declaration.identifier);
      START_INSTR(", ");
      print_register_name(vd->scope_id_stack, n->declaration.assignment_expr);    
      FINISH_INSTR();
      // If this is a scalar
      if (!(n->declaration.assignment_expr->expression.expr_type & TYPE_ANY_VEC)) {
        // Copy the first entry into all entries
        START_INSTR("POW");
        print_register_name(vd->scope_id_stack, n->declaration.identifier, true);
        INSTR(", ");
        print_register_name(vd->scope_id_stack, n->declaration.identifier, true);
        INSTR(", ONE.x");
        FINISH_INSTR();
      }
    }
    break;

  case STATEMENTS_NODE:
    break;
  case IF_STATEMENT_NODE:
    break;
  case ASSIGNMENT_NODE:
    generate_assignment_code(vd->scope_id_stack, n);
    break;
  case NESTED_SCOPE_NODE:
    break;

  case EXPRESSION_NODE:
  case UNARY_EXPRESSION_NODE:
  case BINARY_EXPRESSION_NODE:
  case INT_NODE:
  case FLOAT_NODE:
  case BOOL_NODE:
  case IDENT_NODE:
  case VAR_NODE:
  case FUNCTION_NODE:
  case CONSTRUCTOR_NODE:
    // The if statement is responsible for generating its own condition code
    // since the if is generated in preorder but expressions are generated in
    // postorder
    if (n->parent->kind != IF_STATEMENT_NODE ||
        n != n->parent->statement.if_else_statement.condition) {
      generate_expression(vd, n);
    }
    break;

  case TYPE_NODE:
    break;

  case ARGUMENT_NODE:
    break;

  default: break;
  }
}

void register_assign_postorder(node *n, void *data) {
  int *i = (int *) data;
  if (is_register_temporary(n)) {
    intermediate_registers[n] = *i;
    START_INSTR("TEMP");
    INSTR("tempVar%d", *i);
    FINISH_INSTR();
    *i = *i + 1;
  }
}

void assign_registers(node *ast) {
  int i = 0;

  ast_visit(ast, NULL, register_assign_postorder, &i);

  // Mappings for global registers
  register_tables.push_back(std::map<std::string, std::string>());
  register_tables[0].insert(std::pair<std::string, std::string>("gl_FragColor", "result.color"));
  register_tables[0].insert(std::pair<std::string, std::string>("gl_FragDepth", "result.depth"));
  register_tables[0].insert(std::pair<std::string, std::string>("gl_FragCoord", "fragment.position"));
  register_tables[0].insert(std::pair<std::string, std::string>("gl_TexCoord", "fragment.texcoord"));
  register_tables[0].insert(std::pair<std::string, std::string>("gl_Color", "fragment.color"));
  register_tables[0].insert(std::pair<std::string, std::string>("gl_Secondary", "fragment.color.secondary"));
  register_tables[0].insert(std::pair<std::string, std::string>("gl_FogFragCoord", "fragment.fogcoord"));
  register_tables[0].insert(std::pair<std::string, std::string>("gl_Light_Half", "state.light[0].half"));
  register_tables[0].insert(std::pair<std::string, std::string>("gl_Light_Ambient", "state.lightmodel.ambient"));
  register_tables[0].insert(std::pair<std::string, std::string>("gl_Material_Shininess", "state.material.shininess"));
  register_tables[0].insert(std::pair<std::string, std::string>("env1", "program.env[1]"));
  register_tables[0].insert(std::pair<std::string, std::string>("env2", "program.env[2]"));
  register_tables[0].insert(std::pair<std::string, std::string>("env3", "program.env[3]"));
}

void genCode(node *ast) {
  // Print the fragment shader header
  INSTR("!!ARBfp1.0\n");

  START_INSTR("PARAM");
  INSTR("FALSE = { 0, 0, 0, 0 }");
  FINISH_INSTR();
  START_INSTR("PARAM");
  INSTR("TRUE = { -1, -1, -1, -1 }");
  FINISH_INSTR();
  START_INSTR("PARAM");
  INSTR("ONE = { 1, 1, 1, 1 }");
  FINISH_INSTR();
  START_INSTR("TEMP");
  INSTR("_TEMP");
  FINISH_INSTR();

  // Perform code generation
  visit_data vd;
  // Reserve tempVar[0..1] for binary and unary expressions
  vd.constant_id = 0;

  assign_registers(ast);

  ast_visit(ast, codegen_preorder, codegen_postorder, &vd);

  // Print the fragment shader footer
  INSTR("END\n");
}

bool is_register_temporary(node *expr) {
  switch (expr->kind) {
  case UNARY_EXPRESSION_NODE:
  case BINARY_EXPRESSION_NODE:
  case FUNCTION_NODE:
  case CONSTRUCTOR_NODE:
  case IF_STATEMENT_NODE: // We need a register to store the if condition
    return true;
  case VAR_NODE:
    // If this is a variable node that is referenced on the rhs and indexed
    return (expr->parent->kind != ASSIGNMENT_NODE || expr->parent->statement.assignment.variable != expr) &&
           expr->expression.variable.index != NULL;
  case INT_NODE:
  case FLOAT_NODE:
  case BOOL_NODE:
  case IDENT_NODE:
    return false;
  default:
    return false;
  }
}

bool is_register_constant(node *expr) {
  switch (expr->kind) {
  case INT_NODE:
  case FLOAT_NODE:
  case BOOL_NODE:
    return true;
  case UNARY_EXPRESSION_NODE:
  case BINARY_EXPRESSION_NODE:
  case FUNCTION_NODE:
  case CONSTRUCTOR_NODE:
  case IDENT_NODE:
  case VAR_NODE:
    return false;
  default:
    return false;
  }
}

std::pair<const char *, unsigned int> get_register_name(const std::vector<unsigned int> &scope_id_stack,
                                                        node *n) {
  if (n->kind == VAR_NODE) {
    return get_register_name(scope_id_stack, n->expression.variable.identifier);
  }

  char *variable_name = n->expression.ident.val;

  std::vector<unsigned int>::const_reverse_iterator iter;
  std::map<std::string, std::string>::iterator variable_iter;

  // Traverse the scope id stack backwards
  for (iter = scope_id_stack.rbegin(); iter != scope_id_stack.rend(); iter++) {

    // Look at the register table for each scope
    std::map<std::string, std::string> &register_table = register_tables[*iter];

    // Search for the variable in the table
    variable_iter = register_table.find(variable_name);

    // If the symbol was found, return it
    if (variable_iter != register_table.end()) {
      break;
    }
  }
  return std::make_pair(variable_iter->second.c_str(), *iter);
}

void print_index(int i) {
  switch (i) {
  case 0: INSTR(".x"); break;
  case 1: INSTR(".y"); break;
  case 2: INSTR(".z"); break;
  case 3: INSTR(".w"); break;
  default: break;
  }
}

void print_register_name(const std::vector<unsigned int> &scope_id_stack,
                         node *n,
                         bool force_print_index) {
  bool printed_index = false;
  if (is_register_temporary(n)) {
    INSTR("tempVar%d", intermediate_registers[n]);
  } else if (is_register_constant(n)) {
    if (n->kind == BOOL_NODE) {
      INSTR(n->expression.bool_expr.val ? "TRUE" : "FALSE");
    } else {
      INSTR("const%d", constant_registers[n]);
    }
  } else {
    std::pair<const char *, unsigned int> pair = get_register_name(scope_id_stack, n);
    // Append the scope id to the register if the scope is greater than 1
    if (pair.second > 0) {
      INSTR("%s_%d", pair.first, pair.second);
    } else {
      INSTR("%s", pair.first);
    }
    // Print the explicit index
    if (n->expression.variable.index != NULL) {
      print_index(n->expression.variable.index->expression.int_expr.val);
      printed_index = true;
    }
  }
  // If this register should be used as a scalar
  if (!printed_index && force_print_index && !(n->expression.expr_type & TYPE_ANY_VEC)) {
    print_index(0);
  }
}

void generate_expression(visit_data *vd, node *expr) {
  switch (expr->kind) {
  case EXPRESSION_NODE:
    // EXPRESSION_NODE is an abstract node
    break;
  case UNARY_EXPRESSION_NODE:
    // Only traverse the root of the expression
    /*if (n->parent != BINARY_EXPRESSION_NODE &&
        n->parent != UNARY_EXPRESSION_NODE) {
      generate_unary_expr_code(vd->scope_id_stack, n);
    }*/
    generate_unary_expr_code(vd->scope_id_stack, expr);
    break;
  case BINARY_EXPRESSION_NODE:
    // Only traverse the root of the expression
    /*if (n->parent != BINARY_EXPRESSION_NODE &&
        n->parent != UNARY_EXPRESSION_NODE) {
      generate_binary_expr_code(vd->scope_id_stack, n);
    }*/
    generate_binary_expr_code(vd->scope_id_stack, expr);
    break;
  case INT_NODE:
    if (expr->parent->kind != VAR_NODE) {
      generate_const_int(vd, expr);
    }
    break;
  case FLOAT_NODE:
    generate_const_float(vd, expr);
    break;
  case BOOL_NODE:
    break;
  case IDENT_NODE:
    break;
  case VAR_NODE:
    // If this is a variable node that is referenced on the rhs
    if (is_register_temporary(expr)) {
      START_INSTR("MOV");
      print_register_name(vd->scope_id_stack, expr);
      INSTR(", ");
      print_register_name(vd->scope_id_stack, expr->expression.variable.identifier);
      FINISH_INSTR();
      // If this is a scalar
      if (!(expr->expression.expr_type & TYPE_ANY_VEC)) {
        // Copy the first entry into all entries
        START_INSTR("POW");
        print_register_name(vd->scope_id_stack, expr, true);
        INSTR(", ");
        print_register_name(vd->scope_id_stack, expr, true);
        INSTR(", ONE.x");
        FINISH_INSTR();
      }
    }
    break;
  case FUNCTION_NODE:
    generate_function_code(vd->scope_id_stack, expr);
    break;
  case CONSTRUCTOR_NODE:
    generate_constructor_code(vd->scope_id_stack, expr);
    break;
  default:
    break;
  }
}

void generate_const_int(visit_data *vd, node *int_expr) {
  constant_registers[int_expr] = vd->constant_id;
  START_INSTR("PARAM");
  INSTR("const%d = %d", vd->constant_id++, int_expr->expression.int_expr.val);
  FINISH_INSTR();
}

void generate_const_float(visit_data *vd, node *float_expr) {
  constant_registers[float_expr] = vd->constant_id;
  START_INSTR("PARAM");
  INSTR("const%d = %f", vd->constant_id++, float_expr->expression.float_expr.val);
  FINISH_INSTR();
}

void generate_if_statement_code(const std::vector<unsigned int> &scope_id_stack,
                                node *if_statement) {
  // Move the condition into the if statement's dedicated register
  START_INSTR("MOV");
  print_register_name(scope_id_stack, if_statement);
  INSTR(", ");
  print_register_name(scope_id_stack, if_statement->statement.if_else_statement.condition);
  FINISH_INSTR();

  // Find the parent if statement
  node *parent = if_statement->parent;
  while (parent != NULL && parent->kind != IF_STATEMENT_NODE) {
    parent = parent->parent;
  }
  // And this condition with that of the parent if statement, if there is one
  if (parent != NULL) {
    START_INSTR("MAX");
    print_register_name(scope_id_stack, if_statement);
    INSTR(", ");
    print_register_name(scope_id_stack, if_statement);
    INSTR(", ");
    print_register_name(scope_id_stack, parent);
    FINISH_INSTR();
  }
}

void generate_assignment_code(const std::vector<unsigned int> &scope_id_stack,
                              node *assign) {
  // Find the parent if statement
  node *parent = assign->parent;
  node *parents_child = assign;
  while (parent != NULL && parent->kind != IF_STATEMENT_NODE) {
    parents_child = parent;
    parent = parent->parent;
  }
  if (parent != NULL) {
    START_INSTR("CMP");
    print_register_name(scope_id_stack, assign->statement.assignment.variable);
    INSTR(", ");
    print_register_name(scope_id_stack, parent);
    INSTR(", ");
    if (parents_child == parent->statement.if_else_statement.if_statement) {
      // If the assignment statement is in the if statement then assign the
      // expression when the condition is true
      print_register_name(scope_id_stack, assign->statement.assignment.expression);
      INSTR(", ");
      print_register_name(scope_id_stack, assign->statement.assignment.variable);
    } else {
      // If the assignment statement is in the else statement then assign the
      // expression when the condition is false
      print_register_name(scope_id_stack, assign->statement.assignment.variable);
      INSTR(", ");
      print_register_name(scope_id_stack, assign->statement.assignment.expression);
    }
    FINISH_INSTR();
  } else {
    // If the assignment statement isn't within an if or else statement
    START_INSTR("MOV");
    print_register_name(scope_id_stack, assign->statement.assignment.variable);
    INSTR(", ");
    print_register_name(scope_id_stack, assign->statement.assignment.expression);
    FINISH_INSTR();
  }
}

void generate_unary_expr_code(const std::vector<unsigned int> &scope_id_stack,
                               node *n) {
  unary_op op = n->expression.unary.op;
  node *right = n->expression.unary.right;

  switch (op) {
  case OP_NOT:
    START_INSTR("CMP");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    // true and false values are swapped here
    INSTR(", ");
    INSTR("FALSE");
    INSTR(", ");
    INSTR("TRUE");
    FINISH_INSTR();
    break;
  case OP_UMINUS:
    START_INSTR("MUL");
    print_register_name(scope_id_stack, right);
    INSTR(", ");
    INSTR("TRUE");
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    FINISH_INSTR();
    break;
  default:
    break;
  }
}

void generate_binary_expr_code(const std::vector<unsigned int> &scope_id_stack,
                               node *n) {
  binary_op op = n->expression.binary.op;

  node *left = n->expression.binary.left;
  node *right = n->expression.binary.right;

  switch (op) {
  case OP_AND:
    // Take the max of left and right - if one of them is false
    // (0, 0, 0, 0) then that is the result
    START_INSTR("MAX");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();
    break;
  case OP_OR:
    // Take the min of left and right - if one of them is true
    // (-1, -1, -1, -1) then that is the result
    START_INSTR("MIN");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();
    break;
  case OP_PLUS:
    START_INSTR("ADD");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();
    break;
  case OP_MINUS:
    START_INSTR("SUB");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();
    break;
  case OP_DIV:
    // Take reciprocal of the RHS
    START_INSTR("RCP");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();
    // Multiply the result of the reciprocal by the LHS
    START_INSTR("MUL");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    FINISH_INSTR();
    break;
  case OP_XOR:
    START_INSTR("POW");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left, true);
    INSTR(", ");
    print_register_name(scope_id_stack, right, true);
    FINISH_INSTR();
    break;
  case OP_MUL:
    START_INSTR("MUL");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();
    break;
  case OP_LT:
    // Compare left < right
    START_INSTR("SLT");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();

    // Copy the first entry into all entries
    START_INSTR("POW");
    print_register_name(scope_id_stack, n, true);
    INSTR(", ");
    print_register_name(scope_id_stack, n, true);
    INSTR(", ONE.x");
    FINISH_INSTR();

    // Multiply by TRUE to get (-1, -1, -1, -1) for true
    START_INSTR("MUL");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    INSTR(", TRUE");
    FINISH_INSTR();
    break;
  case OP_LEQ:
    // Compare right >= left
    START_INSTR("SGE");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    FINISH_INSTR();

    // Copy the first entry into all entries
    START_INSTR("POW");
    print_register_name(scope_id_stack, n, true);
    INSTR(", ");
    print_register_name(scope_id_stack, n, true);
    INSTR(", ONE.x");
    FINISH_INSTR();

    // Multiply by TRUE to get (-1, -1, -1, -1) for true
    START_INSTR("MUL");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    INSTR(", TRUE");
    FINISH_INSTR();
    break;
  case OP_GT:
    // Compare right < left
    START_INSTR("SLT");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    FINISH_INSTR();

    // Copy the first entry into all entries
    START_INSTR("POW");
    print_register_name(scope_id_stack, n, true);
    INSTR(", ");
    print_register_name(scope_id_stack, n, true);
    INSTR(", ONE.x");
    FINISH_INSTR();

    // Multiply by TRUE to get (-1, -1, -1, -1) for true
    START_INSTR("MUL");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    INSTR(", TRUE");
    FINISH_INSTR();
    break;
  case OP_GEQ:
    // Compare left >= right
    START_INSTR("SGE");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();

    // Copy the first entry into all entries
    START_INSTR("POW");
    print_register_name(scope_id_stack, n, true);
    INSTR(", ");
    print_register_name(scope_id_stack, n, true);
    INSTR(", ONE.x");
    FINISH_INSTR();

    // Multiply by TRUE to get (-1, -1, -1, -1) for true
    START_INSTR("MUL");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    INSTR(", TRUE");
    FINISH_INSTR();
    break;
  case OP_EQ:
    // Check if left >= right
    START_INSTR("SGE");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();

    // Check if right >= left
    START_INSTR("SGE");
    INSTR("_TEMP");
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    FINISH_INSTR();

    // MUL results of previous operations
    START_INSTR("MUL");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    INSTR("_TEMP");
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    FINISH_INSTR();

    // MUL by TRUE
    START_INSTR("MUL");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    INSTR("TRUE");
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    FINISH_INSTR();
    break;
  case OP_NEQ:
     // Check if left >= right
    START_INSTR("SGE");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    FINISH_INSTR();

    // Check if right >= left
    START_INSTR("SGE");
    INSTR("_TEMP");
    INSTR(", ");
    print_register_name(scope_id_stack, right);
    INSTR(", ");
    print_register_name(scope_id_stack, left);
    FINISH_INSTR();

    // MUL results of previous operations
    START_INSTR("MUL");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    INSTR("_TEMP");
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    FINISH_INSTR();

    // Subtract 1
    START_INSTR("ADD");
    print_register_name(scope_id_stack, n);
    INSTR(", ");
    INSTR("TRUE");
    INSTR(", ");
    print_register_name(scope_id_stack, n);
    FINISH_INSTR();
    break;
  default:
    break;
  }
}

void generate_function_code(const std::vector<unsigned int> &scope_id_stack,
                            node *func) {
  node *first_arg = func->expression.function.arguments;
  node *first_expr = first_arg->argument.expression;
  node *second_arg = first_arg->argument.next_argument;
  node *second_expr = second_arg != NULL ? second_arg->argument.expression : NULL;

  switch (func->expression.function.func_id) {
  case FUNC_DP3:
    START_INSTR("DP3");
    print_register_name(scope_id_stack, func);
    INSTR(", ");
    print_register_name(scope_id_stack, first_expr);
    INSTR(", ");
    print_register_name(scope_id_stack, second_expr);
    FINISH_INSTR();
    break;
  case FUNC_RSQ:
    START_INSTR("RSQ");
    print_register_name(scope_id_stack, func, true);
    INSTR(", ");
    print_register_name(scope_id_stack, first_expr, true);
    FINISH_INSTR();
    break;
  case FUNC_LIT:
    START_INSTR("LIT");
    print_register_name(scope_id_stack, func);
    INSTR(", ");
    print_register_name(scope_id_stack, first_expr);
    FINISH_INSTR();
    break;
  }
}

void generate_constructor_code(const std::vector<unsigned int> &scope_id_stack,
                               node *constr) {
  int i = 0;
  node *argument = constr->expression.constructor.arguments;
  while (argument != NULL) {
    START_INSTR("MOV");
    print_register_name(scope_id_stack, constr);
    print_index(i++);
    INSTR(", ");
    print_register_name(scope_id_stack, argument->argument.expression, true);
    FINISH_INSTR();
    argument = argument->argument.next_argument;
  }
  // If this is a scalar
  if (!(constr->expression.constructor.type->type.type & TYPE_ANY_VEC)) {
    // Copy the first entry into all entries
    START_INSTR("POW");
    print_register_name(scope_id_stack, constr, true);
    INSTR(", ");
    print_register_name(scope_id_stack, constr, true);
    INSTR(", ONE.x");
    FINISH_INSTR();
  }
}

