/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     IDENTIFIER = 258,
     FUNCNAME = 259,
     DECLARATOR = 260,
     CONSTANT = 261,
     VARIABLE = 262,
     IF = 263,
     ELSE = 264,
     BADTOKEN = 265,
     EXPONEQ = 266,
     DIVEQ = 267,
     MULTEQ = 268,
     MINUSEQ = 269,
     PLUSEQ = 270,
     LOGICAND = 271,
     LOGICOR = 272,
     CMPEQ = 273,
     NOTEQ = 274,
     LESSEQ = 275,
     GREQ = 276,
     UPLUS = 277,
     UMINUS = 278
   };
#endif
/* Tokens.  */
#define IDENTIFIER 258
#define FUNCNAME 259
#define DECLARATOR 260
#define CONSTANT 261
#define VARIABLE 262
#define IF 263
#define ELSE 264
#define BADTOKEN 265
#define EXPONEQ 266
#define DIVEQ 267
#define MULTEQ 268
#define MINUSEQ 269
#define PLUSEQ 270
#define LOGICAND 271
#define LOGICOR 272
#define CMPEQ 273
#define NOTEQ 274
#define LESSEQ 275
#define GREQ 276
#define UPLUS 277
#define UMINUS 278




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



