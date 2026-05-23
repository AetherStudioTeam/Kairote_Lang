#ifndef KRT_TYPE_CHECK_H
#define KRT_TYPE_CHECK_H
#include "../parser/Ast.h"

typedef enum {
    ACCESS_PUBLIC,
    ACCESS_PRIVATE,
    ACCESS_PROTECTED
} AccessModifier;

typedef enum {
    MEMBER_FIELD,
    MEMBER_METHOD,
    MEMBER_CONSTRUCTOR,
    MEMBER_DESTRUCTOR,
    MEMBER_PROPERTY
} MemberType;

typedef struct ClassMember {
    char* name;
    MemberType member_type;
    AccessModifier access;
    int is_static;
    struct Type* type;
} ClassMember;

typedef struct ClassInfo {
    char* class_name;
    char* base_class_name;
    ClassMember* members;
    int member_count;
    int member_capacity;
    struct TypeCheckSymbolTable* member_scope;
} ClassInfo;

typedef enum {
    TYPE_VOID,
    TYPE_INT8,
    TYPE_INT16,
    TYPE_INT32,
    TYPE_INT64,
    TYPE_UINT8,
    TYPE_UINT16,
    TYPE_UINT32,
    TYPE_UINT64,
    TYPE_FLOAT32,
    TYPE_FLOAT64,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_CLASS,
    TYPE_UNKNOWN
} TypeKind;

typedef struct Type {
    TypeKind kind;
    union {

        struct Type* pointer_to;

        struct {
            struct Type* element_type;
            int size;
        } array;

        struct {
            struct Type* return_type;
            struct Type** parameter_types;
            int parameter_count;
            struct TypeCheckSymbolTable* function_scope;
        } function;

        struct {
            char* class_name;
            struct Type** field_types;
            char** field_names;
            int field_count;
            struct ClassInfo* class_info;
        } class;
    } data;
} Type;

typedef struct TypeCheckSymbol {
    char* name;
    Type* type;
    int is_constant;
    int line_number;
    int owns_name;
} TypeCheckSymbol;

typedef struct TypeCheckSymbolTable {
    TypeCheckSymbol* symbols;
    int symbol_count;
    int capacity;
    struct TypeCheckSymbolTable* parent;
    int ref_count;
} TypeCheckSymbolTable;

typedef struct TypeCheckContext {
    TypeCheckSymbolTable* current_scope;
    char* current_class;
    ClassInfo* current_class_info;
    int error_count;
    int warning_count;
    int suppress_errors;
    int current_function_is_static;
    int scope_ownership;
    
    void* semantic_analyzer;
} TypeCheckContext;

Type* type_create_basic(TypeKind kind);
Type* type_create_pointer(Type* base_type);
Type* type_create_array(Type* element_type, int size);
Type* type_create_function(Type* return_type, Type** param_types, int param_count);
Type* type_create_class(const char* class_name);
void type_destroy(Type* type);
Type* type_copy(Type* type);

ClassInfo* class_info_create(const char* class_name);
void class_info_destroy(ClassInfo* class_info);
void class_info_add_member(ClassInfo* class_info, ClassMember member);
ClassMember* class_info_find_member(ClassInfo* class_info, const char* member_name);

AccessModifier token_to_access_modifier(KrtTokenType token);

TypeCheckContext* type_check_context_create(void* semantic_analyzer);
void type_check_context_destroy(TypeCheckContext* context);

int type_check_program(TypeCheckContext* context, ASTNode* program);
int type_check_statement(TypeCheckContext* context, ASTNode* statement);
Type* type_check_expression(TypeCheckContext* context, ASTNode* expression);

int type_is_compatible(Type* type1, Type* type2);
int type_is_assignable(Type* target, Type* source);

void type_check_init_builtin_types(TypeCheckContext* context);
void type_check_init_builtin_functions(TypeCheckContext* context);

TypeCheckSymbolTable* type_check_symbol_table_create(TypeCheckSymbolTable* parent);
void type_check_symbol_table_unref(TypeCheckSymbolTable* table);
TypeCheckSymbol* type_check_symbol_table_lookup(TypeCheckSymbolTable* table, const char* name);
int type_check_symbol_table_add(TypeCheckSymbolTable* table, TypeCheckSymbol symbol);

Type* type_create_from_token(KrtTokenType token_type);

#endif