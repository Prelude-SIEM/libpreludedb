/* A Bison parser, made by GNU Bison 3.0.5.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY__PRELUDEDBYY_PRELUDEDB_PATH_SELECTION_PARSER_YAC_H_INCLUDED
# define YY__PRELUDEDBYY_PRELUDEDB_PATH_SELECTION_PARSER_YAC_H_INCLUDED
/* Debug traces.  */
#ifndef _PRELUDEDBYYDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define _PRELUDEDBYYDEBUG 1
#  else
#   define _PRELUDEDBYYDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define _PRELUDEDBYYDEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined _PRELUDEDBYYDEBUG */
#if _PRELUDEDBYYDEBUG
extern int _preludedbyydebug;
#endif
/* "%code requires" blocks.  */
#line 39 "preludedb-path-selection-parser.yac.y" /* yacc.c:1910  */


#ifndef YY_TYPEDEF_YY_SCANNER_T
# define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#line 60 "preludedb-path-selection-parser.yac.h" /* yacc.c:1910  */

/* Token type.  */
#ifndef _PRELUDEDBYYTOKENTYPE
# define _PRELUDEDBYYTOKENTYPE
  enum _preludedbyytokentype
  {
    tSTRING = 258,
    tIDMEF = 259,
    tNUMBER = 260,
    tERROR = 261,
    tLPAREN = 262,
    tRPAREN = 263,
    tCOLON = 264,
    tCOMMA = 265,
    tSLASH = 266,
    tMIN = 267,
    tMAX = 268,
    tSUM = 269,
    tCOUNT = 270,
    tINTERVAL = 271,
    tAVG = 272,
    tEXTRACT = 273,
    tTIMEZONE = 274,
    tDISTINCT = 275,
    tYEAR = 276,
    tQUARTER = 277,
    tMONTH = 278,
    tWEEK = 279,
    tYDAY = 280,
    tMDAY = 281,
    tWDAY = 282,
    tDAY = 283,
    tHOUR = 284,
    tSEC = 285,
    tMSEC = 286,
    tUSEC = 287,
    tORDER_ASC = 288,
    tORDER_DESC = 289,
    tGROUP_BY = 290
  };
#endif
/* Tokens.  */
#define tSTRING 258
#define tIDMEF 259
#define tNUMBER 260
#define tERROR 261
#define tLPAREN 262
#define tRPAREN 263
#define tCOLON 264
#define tCOMMA 265
#define tSLASH 266
#define tMIN 267
#define tMAX 268
#define tSUM 269
#define tCOUNT 270
#define tINTERVAL 271
#define tAVG 272
#define tEXTRACT 273
#define tTIMEZONE 274
#define tDISTINCT 275
#define tYEAR 276
#define tQUARTER 277
#define tMONTH 278
#define tWEEK 279
#define tYDAY 280
#define tMDAY 281
#define tWDAY 282
#define tDAY 283
#define tHOUR 284
#define tSEC 285
#define tMSEC 286
#define tUSEC 287
#define tORDER_ASC 288
#define tORDER_DESC 289
#define tGROUP_BY 290

/* Value type.  */
#if ! defined _PRELUDEDBYYSTYPE && ! defined _PRELUDEDBYYSTYPE_IS_DECLARED

union _PRELUDEDBYYSTYPE
{
#line 47 "preludedb-path-selection-parser.yac.y" /* yacc.c:1910  */

        int val;
        int flags;
        int error;
        preludedb_selected_object_interval_t interval;
        preludedb_selected_object_type_t type;
        preludedb_selected_object_t *object;

#line 151 "preludedb-path-selection-parser.yac.h" /* yacc.c:1910  */
};

typedef union _PRELUDEDBYYSTYPE _PRELUDEDBYYSTYPE;
# define _PRELUDEDBYYSTYPE_IS_TRIVIAL 1
# define _PRELUDEDBYYSTYPE_IS_DECLARED 1
#endif



int _preludedbyyparse (yyscan_t scanner, preludedb_selected_path_t *root);

#endif /* !YY__PRELUDEDBYY_PRELUDEDB_PATH_SELECTION_PARSER_YAC_H_INCLUDED  */
