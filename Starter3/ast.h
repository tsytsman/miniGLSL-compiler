#ifndef AST_H_
#define AST_H_ 1

#include <stdarg.h>
#include <vector>
#include <list>

// Dummy node just so everything compiles, create your own node/nodes
//
// The code provided below is an example ONLY. You can use/modify it,
// but do not assume that it is correct or complete.
//
// There are many ways of making AST nodes. The approach below is an example
// of a descriminated union. If you choose to use C++, then I suggest looking
// into inheritance.

// forward declare
struct node_;
typedef struct node_ node;
extern node *ast;

typedef enum {
  UNKNOWN                = 0,

  SCOPE_NODE             = (1 << 0),

  EXPRESSION_NODE        = (1 << 2),
  UNARY_EXPRESSION_NODE  = (1 << 2) | (1 << 3),
  BINARY_EXPRESSION_NODE = (1 << 2) | (1 << 4),
  INT_NODE               = (1 << 2) | (1 << 5),
  FLOAT_NODE             = (1 << 2) | (1 << 6),
  BOOL_NODE              = (1 << 2) | (1 << 7),
  IDENT_NODE             = (1 << 2) | (1 << 8),
  VAR_NODE               = (1 << 2) | (1 << 9),
  FUNCTION_NODE          = (1 << 2) | (1 << 10),
  CONSTRUCTOR_NODE       = (1 << 2) | (1 << 11),

  STATEMENTS_NODE        = (1 << 1),
  IF_STATEMENT_NODE      = (1 << 1) | (1 << 12),
  ASSIGNMENT_NODE        = (1 << 1) | (1 << 13),
  NESTED_SCOPE_NODE      = (1 << 1) | (1 << 14),

  DECLARATIONS_NODE      = (1 << 15),
  DECLARATION_NODE       = (1 << 16),

  TYPE_NODE              = (1 << 17),

  ARGUMENT_NODE         = (1 << 18),
} node_kind;

typedef enum {
  OP_UMINUS,
  OP_NOT,
} unary_op;

typedef enum {
  OP_AND,
  OP_OR,
  OP_EQ,
  OP_NEQ,
  OP_LT,
  OP_LEQ,
  OP_GT,
  OP_GEQ,
  OP_PLUS,
  OP_MINUS,
  OP_MUL,
  OP_DIV,
  OP_XOR,
} binary_op;

/*
 * TYPE_VEC, TYPE_IVEC, and TYPE_BVEC are abstract types.
 * They shouldn't ever be assigned to a type, but can be
 * used to check if a type is in their category. For example,
 * we can check if a type belongs to the TYPE_VEC category:
 *   my_type = TYPE_VEC3;
 *   if (my_type & TYPE_VEC)
 *     do_something();
 */
typedef enum {
  TYPE_UNKNOWN          = 0,
  TYPE_INT              = (1 << 0),
  TYPE_BOOL             = (1 << 1),
  TYPE_FLOAT            = (1 << 2),
  TYPE_ANY_VEC          = (1 << 3) + (1 << 4) + (1 << 5),
  TYPE_VEC              = (1 << 3),
  TYPE_VEC2             = (1 << 3) + 2,
  TYPE_VEC3             = (1 << 3) + 3,
  TYPE_VEC4             = (1 << 3) + 4,
  TYPE_IVEC             = (1 << 4),
  TYPE_IVEC2            = (1 << 4) + 2,
  TYPE_IVEC3            = (1 << 4) + 3,
  TYPE_IVEC4            = (1 << 4) + 4,
  TYPE_BVEC             = (1 << 5),
  TYPE_BVEC2            = (1 << 5) + 2,
  TYPE_BVEC3            = (1 << 5) + 3,
  TYPE_BVEC4            = (1 << 5) + 4,
} symbol_type;

typedef enum {
  FUNC_DP3,
  FUNC_RSQ,
  FUNC_LIT,
} function_id;

struct node_ {
  node_kind kind;
  node *parent;

  int line, column;

  union {
    struct {
      node *declarations;
      node *statements;

      unsigned int scope_id;
    } scope;

    struct {
      std::list<node *> *declarations;
    } declarations;

    struct {
      bool is_const;
      node *type;
      node *identifier;
      node *assignment_expr;
    } declaration;

    struct {
      std::list<node *> *statements;
    } statements;

    struct {
      union {
        struct {
          node *condition;
          node *if_statement;
          node *else_statement;
        } if_else_statement;

        struct {
          node *variable;
          node *expression;
        } assignment;

        struct {
          node *scope;
        } nested_scope;
      };
    } statement;

    struct {
      union {
        struct {
          unary_op op;
          node *right;
        } unary;

        struct {
          binary_op op;
          node *left;
          node *right;
        } binary;

        struct {
          int val;
        } int_expr;

        struct {
          float val;
        } float_expr;

        struct {
          bool val;
        } bool_expr;

        struct {
          char *val;
        } ident;

        struct {
          node *identifier;
          node *index;
        } variable;

        struct {
          function_id func_id;
          node *arguments;
        } function;

        struct {
          node *type;
          node *arguments;
        } constructor;
      };

      symbol_type expr_type;
    } expression;

    struct {
      symbol_type type;
    } type;

    struct {
      node *expression;
      node *next_argument;

      int num_arguments; // Only meaningful for the first argument
      node *last_argument; // Only used during construction
    } argument;
  };
};

node *ast_allocate(node_kind type, ...);
void ast_free(node *n);
void ast_print(node *n);

void ast_visit(node *n,
               void (*preorder)(node *, void *),
               void (*postorder)(node *, void *),
               void *data);

// Helper functions for getting the name of various things
const char *get_function_name(function_id func_id);
const char *get_type_name(symbol_type type);
const char *get_unary_op_name(unary_op op);
const char *get_binary_op_name(binary_op op);

#endif /* AST_H_ */
