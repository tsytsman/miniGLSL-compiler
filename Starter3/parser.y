%{
/***********************************************************************
 * Braden Watling
 * Nikita Tsytsarkin
 * g467-006
 *
 *   Interface to the parser module for CSC467 course project.
 *
 *   Phase 2: Implement context free grammar for source language, and
 *            parse tracing functionality.
 *   Phase 3: Construct the AST for the source language program.
 ***********************************************************************/

/***********************************************************************
 *  C Definitions and external declarations for this module.
 *
 *  Phase 3: Include ast.h if needed, and declarations for other global or
 *           external vars, functions etc. as needed.
 ***********************************************************************/

#include <string.h>

#include "common.h"
#include "ast.h"
#include "symbol.h"
#include "semantic.h"

#define YYERROR_VERBOSE
#define yTRACE(x)    { if (traceParser) fprintf(traceFile, "%s\n", x); }

void yyerror(const char* s);    /* what to do in case of error            */
int yylex();                    /* procedure for calling lexical analyzer */
extern int yyline;              /* variable holding current line number   */

enum {
  DP3 = 0,
  LIT = 1,
  RSQ = 2
};

%}

/***********************************************************************
 *  Yacc/Bison declarations.
 *  Phase 2:
 *    1. Add precedence declarations for operators (after %start declaration)
 *    2. If necessary, add %type declarations for some nonterminals
 *  Phase 3:
 *    1. Add fields to the union below to facilitate the construction of the
 *       AST (the two existing fields allow the lexical analyzer to pass back
 *       semantic info, so they shouldn't be touched).
 *    2. Add <type> modifiers to appropriate %token declarations (using the
 *       fields of the union) so that semantic information can by passed back
 *       by the scanner.
 *    3. Make the %type declarations for the language non-terminals, utilizing
 *       the fields of the union as well.
 ***********************************************************************/

%{
#define YYDEBUG 1
%}

// defines the yyval union
%union {
  int as_int;
  int as_vec;
  float as_float;
  char *as_str;
  int as_func;
  node *as_ast;
}

%token          FLOAT_T
%token          INT_T
%token          BOOL_T
%token          CONST
%token          FALSE_C TRUE_C
%token          IF ELSE
%token          AND OR NEQ EQ LEQ GEQ

// links specific values of tokens to yyval
%token <as_vec>   VEC_T
%token <as_vec>   BVEC_T
%token <as_vec>   IVEC_T
%token <as_float> FLOAT_C
%token <as_int>   INT_C
%token <as_str>   ID
%token <as_func>  FUNC

// operator precdence
%left     OR                        // 7
%left     AND                       // 6
%left     EQ NEQ '<' LEQ '>' GEQ    // 5
%left     '+' '-'                   // 4
%left     '*' '/'                   // 3
%right    '^'                       // 2
%right    '!' UMINUS                // 1
%left     '(' '['                   // 0

// Give the shifting of else a higher precedence than
// the reduction of if-then
%precedence     IF_THEN
%precedence     ELSE

// type declarations
// TODO: fill this out
%type <as_ast> scope
%type <as_ast> declarations
%type <as_ast> declaration
%type <as_ast> statements
%type <as_ast> statement
%type <as_ast> expression
%type <as_ast> variable
%type <as_ast> arguments_opt
%type <as_ast> arguments
%type <as_ast> type

%start    program

%%

/***********************************************************************
 *  Yacc/Bison rules
 *  Phase 2:
 *    1. Replace grammar found here with something reflecting the source
 *       language grammar
 *    2. Implement the trace parser option of the compiler
 ***********************************************************************/
program
  : scope
      { yTRACE("program -> scope\n") ast = $1; }
  ;

scope
  : '{' declarations statements '}'
      { yTRACE("scope -> { declarations statements }\n") $$ = ast_allocate(SCOPE_NODE, $2, $3); }
  ;

declarations
  : declarations declaration
      { yTRACE("declarations -> declarations declaration\n") if ($1 != NULL ) $1->declaration.next_declaration = $2; $$ = $2; }
  | %empty
      { yTRACE("declarations -> \n") $$ = NULL; }
  ;

statements
  : statements statement
      { yTRACE("statements -> statements statement\n") if ($1 != NULL) $1->statement.next_statement = $2; $$ = $2; }
  | %empty
      { yTRACE("statements -> \n") $$ = NULL; }
  ;

declaration
  : type ID ';'
      { yTRACE("declaration -> type ID ;\n") $$ = ast_allocate(DECLARATION_NODE, false, $1, $2, NULL); }
  | type ID '=' expression ';'
      { yTRACE("declaration -> type ID = expression ;\n") $$ = ast_allocate(DECLARATION_NODE, false, $1, $2, $4); }
  | CONST type ID '=' expression ';'
      { yTRACE("declaration -> CONST type ID = expression ;\n") $$ = ast_allocate(DECLARATION_NODE, true, $2, $3, $5); }
  ;

statement
  : variable '=' expression ';'
      { yTRACE("statement -> variable = expression ;\n") $$ = ast_allocate(ASSIGNMENT_NODE, $1, $3); }
  | IF '(' expression ')' statement %prec IF_THEN
      { yTRACE("statement -> IF ( expression ) statement \n") $$ = ast_allocate(IF_STATEMENT_NODE, $3, $5, NULL); }
  | IF '(' expression ')' statement ELSE statement
      { yTRACE("statement -> IF ( expression ) statement ELSE statement \n") $$ = ast_allocate(IF_STATEMENT_NODE, $3, $5, $7); }
  | scope
      { yTRACE("statement -> scope \n") }
  | ';'
      { yTRACE("statement -> ; \n") $$ = NULL; }
  ;

type
  : INT_T
      { yTRACE("type -> INT_T \n") $$ = ast_allocate(TYPE_NODE, TYPE_INT, 0); }
  | IVEC_T
      { yTRACE("type -> IVEC_T \n") $$ = ast_allocate(TYPE_NODE, TYPE_IVEC, $1); }
  | BOOL_T
      { yTRACE("type -> BOOL_T \n") $$ = ast_allocate(TYPE_NODE, TYPE_BOOL, 0); }
  | BVEC_T
      { yTRACE("type -> BVEC_T \n") $$ = ast_allocate(TYPE_NODE, TYPE_BVEC, $1); }
  | FLOAT_T
      { yTRACE("type -> FLOAT_T \n") $$ = ast_allocate(TYPE_NODE, TYPE_FLOAT, 0); }
  | VEC_T
      { yTRACE("type -> VEC_T \n") $$ = ast_allocate(TYPE_NODE, TYPE_VEC, $1); }
  ;

expression

  /* function-like operators */
  : type '(' arguments_opt ')' %prec '('
      { yTRACE("expression -> type ( arguments_opt ) \n") $$ = ast_allocate(CONSTRUCTOR_NODE, $1, $3); }
  | FUNC '(' arguments_opt ')' %prec '('
      { yTRACE("expression -> FUNC ( arguments_opt ) \n") $$ = ast_allocate(FUNCTION_NODE, $1, $3); }

  /* unary opterators */
  | '-' expression %prec UMINUS
      { yTRACE("expression -> - expression \n") $$ = ast_allocate(UNARY_EXPRESSION_NODE, OP_UMINUS, $2); }
  | '!' expression %prec '!'
      { yTRACE("expression -> ! expression \n") $$ = ast_allocate(UNARY_EXPRESSION_NODE, OP_NOT, $2); }

  /* binary operators */
  | expression AND expression %prec AND
      { yTRACE("expression -> expression AND expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_AND, $1, $3); }
  | expression OR expression %prec OR
      { yTRACE("expression -> expression OR expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_OR, $1, $3); }
  | expression EQ expression %prec EQ
      { yTRACE("expression -> expression EQ expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_EQ, $1, $3); }
  | expression NEQ expression %prec NEQ
      { yTRACE("expression -> expression NEQ expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_NEQ, $1, $3); }
  | expression '<' expression %prec '<'
      { yTRACE("expression -> expression < expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_LT, $1, $3); }
  | expression LEQ expression %prec LEQ
      { yTRACE("expression -> expression LEQ expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_LEQ, $1, $3); }
  | expression '>' expression %prec '>'
      { yTRACE("expression -> expression > expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_GT, $1, $3); }
  | expression GEQ expression %prec GEQ
      { yTRACE("expression -> expression GEQ expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_GEQ, $1, $3); }
  | expression '+' expression %prec '+'
      { yTRACE("expression -> expression + expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_PLUS, $1, $3); }
  | expression '-' expression %prec '-'
      { yTRACE("expression -> expression - expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_MINUS, $1, $3); }
  | expression '*' expression %prec '*'
      { yTRACE("expression -> expression * expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_MUL, $1, $3); }
  | expression '/' expression %prec '/'
      { yTRACE("expression -> expression / expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_DIV, $1, $3); }
  | expression '^' expression %prec '^'
      { yTRACE("expression -> expression ^ expression \n") $$ = ast_allocate(BINARY_EXPRESSION_NODE, OP_XOR, $1, $3); }

  /* literals */
  | TRUE_C
      { yTRACE("expression -> TRUE_C \n") $$ = ast_allocate(INT_NODE, 1); }
  | FALSE_C
      { yTRACE("expression -> FALSE_C \n") $$ = ast_allocate(INT_NODE, 0); }
  | INT_C
      { yTRACE("expression -> INT_C \n") $$ = ast_allocate(INT_NODE, $1); }
  | FLOAT_C
      { yTRACE("expression -> FLOAT_C \n") $$ = ast_allocate(FLOAT_NODE, $1); }

  /* misc */
  | '(' expression ')'
      { yTRACE("expression -> ( expression ) \n") $$ = $2; }
  | variable
    { yTRACE("expression -> variable \n") }
  ;

variable
  : ID
      { yTRACE("variable -> ID \n") $$ = ast_allocate(VAR_NODE, $1, false, 0); }
  | ID '[' INT_C ']' %prec '['
      { yTRACE("variable -> ID [ INT_C ] \n") $$ = ast_allocate(VAR_NODE, $1, true, $3); }
  ;

arguments_opt
  : arguments
      { yTRACE("arguments_opt -> arguments \n") }
  | %empty
      { yTRACE("arguments_opt -> \n") $$ = NULL; $$ = NULL; }
  ;

arguments
  : arguments ',' expression
      { yTRACE("arguments -> arguments , expression \n") if ($1 != NULL) $1->argument.next_argument = $3; $$ = $3; }
  | expression
      { yTRACE("arguments -> expression \n") $$ = ast_allocate(ARGUMENT_NODE, $1); }
  ;

%%

/***********************************************************************ol
 * Extra C code.
 *
 * The given yyerror function should not be touched. You may add helper
 * functions as necessary in subsequent phases.
 ***********************************************************************/
void yyerror(const char* s) {
  if(errorOccurred) {
    return;    /* Error has already been reported by scanner */
  } else {
    errorOccurred = 1;
  }

  fprintf(errorFile, "\nPARSER ERROR, LINE %d", yyline);

  if(strcmp(s, "parse error")) {
    if(strncmp(s, "parse error, ", 13)) {
      fprintf(errorFile, ": %s\n", s);
    } else {
      fprintf(errorFile, ": %s\n", s+13);
    }
  } else {
    fprintf(errorFile, ": Reading token %s\n", yytname[YYTRANSLATE(yychar)]);
  }
}
