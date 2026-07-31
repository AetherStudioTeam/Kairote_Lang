#ifndef PARSER_BASE_H
#define PARSER_BASE_H

#include "Parser.h"

void parser_add_declared_function(Parser* parser, const char* func_name);
int parser_is_type_keyword(KrtTokenType type);
void parser_advance(Parser* parser);
int parser_parse_parameter_list(Parser* parser, char*** parameters, KrtTokenType** parameter_types, int** parameter_is_params, int** parameter_is_array, ASTNode*** parameter_default_values, int* parameter_count);
void parser_free_parameter_list(char** parameters, KrtTokenType* parameter_types, int* parameter_is_params, int* parameter_is_array, ASTNode** parameter_default_values, int parameter_count);

#endif