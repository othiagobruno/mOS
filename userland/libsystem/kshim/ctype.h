#pragma once
#define isspace(c) ((c)==' '||(c)=='\t'||(c)=='\n'||(c)=='\r'||(c)=='\f'||(c)=='\v')
#define isdigit(c) ((c)>='0'&&(c)<='9')
#define isalpha(c) (((c)|32)>='a'&&((c)|32)<='z')
#define isalnum(c) (isalpha(c)||isdigit(c))
#define isxdigit(c) (isdigit(c)||(((c)|32)>='a'&&((c)|32)<='f'))
#define tolower(c) (isalpha(c)?((c)|32):(c))
#define toupper(c) (isalpha(c)?((c)&~32):(c))
#define isprint(c) ((c)>=32&&(c)<127)
