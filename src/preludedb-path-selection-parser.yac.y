%{
  #include <stdio.h>
  #include <string.h>
  #include "preludedb-path-selection.h"
  #include "preludedb-path-selection-parser.yac.h"
  #include "preludedb-path-selection-parser.lex.h"
%}

%pure-parser
%lex-param {yyscan_t scanner}
%parse-param {yyscan_t scanner}{preludedb_selected_path_t *root}
%define parse.error verbose

%code requires {

#ifndef YY_TYPEDEF_YY_SCANNER_T
# define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif
}

%union {
        int val;
        int flags;
        int error;
        preludedb_selected_object_interval_t interval;
        preludedb_selected_object_type_t type;
        preludedb_selected_object_t *object;
}


%code {
    static void yyerror(yyscan_t scanner, preludedb_selected_path_t *root, const char *msg)
    {
            errno = preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "parser error '%s'", msg);
    }

    struct filter_table {
                    const char *name;
                    int flag;
    };

    static int get_filter(const struct filter_table *table, const char *str)
    {
            size_t i;

            for ( i = 0; table[i].name != NULL; i++ ) {
                    if ( strcmp(str, table[i].name) == 0 )
                            return table[i].flag;
            }

            return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "Invalid time filter '%s'", str);
    }

    static int get_extract_filter(const char *str)
    {
            struct filter_table time_filter_table[] = {
                    { "year", PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR        },
                    { "month", PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH       },
                    { "yday", PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY        },
                    { "mday", PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY        },
                    { "wday", PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY        },
                    { "hour", PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR        },
                    { "min", PRELUDEDB_SQL_TIME_CONSTRAINT_MIN         },
                    { "sec", PRELUDEDB_SQL_TIME_CONSTRAINT_SEC         },
                    { "msec", PRELUDEDB_SQL_TIME_CONSTRAINT_MSEC       },
                    { "usec", PRELUDEDB_SQL_TIME_CONSTRAINT_USEC       },
                    { "quarter", PRELUDEDB_SQL_TIME_CONSTRAINT_QUARTER },
                    { NULL, 0,                                         }
            };

            return get_filter((const struct filter_table *) &time_filter_table, str);
    }

    static int get_interval_filter(const char *str)
    {
            struct filter_table time_filter_table[] = {
                    { "year", PRELUDEDB_SELECTED_OBJECT_INTERVAL_YEAR        },
                    { "month", PRELUDEDB_SELECTED_OBJECT_INTERVAL_MONTH      },
                    { "day", PRELUDEDB_SELECTED_OBJECT_INTERVAL_DAY          },
                    { "hour", PRELUDEDB_SELECTED_OBJECT_INTERVAL_HOUR        },
                    { "min",  PRELUDEDB_SELECTED_OBJECT_INTERVAL_MIN         },
                    { "sec",  PRELUDEDB_SELECTED_OBJECT_INTERVAL_SEC         },
                    { NULL, 0,                                               }
            };

            return get_filter((const struct filter_table *) &time_filter_table, str);
    }
}



%token tSTRING tIDMEF tNUMBER tERROR
%token tLPAREN tRPAREN tCOLON tCOMMA tSLASH
%token tMIN tMAX tSUM tCOUNT tINTERVAL tAVG tEXTRACT
%token tYEAR tQUARTER tMONTH tWEEK tYDAY tMDAY tWDAY tDAY tHOUR tSEC tMSEC tUSEC
%token tORDER_ASC tORDER_DESC tGROUP_BY

%type<object> tIDMEF
%type<object> tSTRING
%type<object> tNUMBER
%type<object> value
%type<object> valuetype
%type<object> func
%type<object> onearg
%type<object> intervalfunc
%type<object> extractfunc
%type<val> expressions
%type<type> oneargfunc
%type<type> extract
%type<type> interval
%type<flags> option
%type<flags> expression_option
%type<flags> modifier
%type<error> tERROR

%%

expressions: value tSLASH expression_option {
                preludedb_selected_path_set_object(root, $1);
                preludedb_selected_path_set_flags(root, $3);
             }
             | value { preludedb_selected_path_set_object(root, $1); }
             | error { return (errno < 0) ? errno : -1; }

option:   tORDER_ASC  { $$ = PRELUDEDB_SELECTED_PATH_FLAGS_ORDER_ASC; }
        | tORDER_DESC { $$ = PRELUDEDB_SELECTED_PATH_FLAGS_ORDER_DESC; }
        | tGROUP_BY   { $$ = PRELUDEDB_SELECTED_PATH_FLAGS_GROUP_BY; }

expression_option: expression_option tCOMMA option { $$ = $1|$3; }
                   | option { $$ = $1; }


oneargfunc:  tMIN { $$ = PRELUDEDB_SELECTED_OBJECT_TYPE_MIN; }
           | tMAX { $$ = PRELUDEDB_SELECTED_OBJECT_TYPE_MAX; }
           | tCOUNT { $$ = PRELUDEDB_SELECTED_OBJECT_TYPE_COUNT; }
           | tAVG { $$ = PRELUDEDB_SELECTED_OBJECT_TYPE_AVG; }


onearg: oneargfunc tLPAREN value tRPAREN {
        preludedb_selected_object_t *parent;
        preludedb_selected_object_new(&parent, $1, NULL);
        preludedb_selected_object_push_arg(parent, $3);
        $$ = parent;
}

extract: tEXTRACT { $$ = PRELUDEDB_SELECTED_OBJECT_TYPE_EXTRACT; }
extractfunc: extract tLPAREN value tCOMMA tSTRING tRPAREN {
        int tf;
        preludedb_selected_object_t *parent, *arg;

        tf = get_extract_filter(((const char *)preludedb_selected_object_get_data($5)));
        preludedb_selected_object_destroy($5);

        if ( tf < 0 ) {
                errno = tf;
                YYERROR;
        }

        errno = preludedb_selected_object_new(&parent, $1, NULL);
        if ( errno < 0 )
                YYERROR;

        errno = preludedb_selected_object_new(&arg, PRELUDEDB_SELECTED_OBJECT_TYPE_INT, &tf);
        if ( errno < 0 ) {
                preludedb_selected_object_destroy(parent);
                YYERROR;
        }

        preludedb_selected_object_push_arg(parent, $3);
        preludedb_selected_object_push_arg(parent, arg);
        $$ = parent;
}

interval: tINTERVAL { $$ = PRELUDEDB_SELECTED_OBJECT_TYPE_INTERVAL; }
intervalfunc: interval tLPAREN value tCOMMA value tCOMMA tSTRING tRPAREN {
        int tf;
        preludedb_selected_object_t *parent, *arg;

        tf = get_interval_filter(((const char *)preludedb_selected_object_get_data($7)));
        preludedb_selected_object_destroy($7);

        if ( tf < 0 ) {
                errno = tf;
                YYERROR;
        }

        errno = preludedb_selected_object_new(&parent, $1, NULL);
        if ( errno < 0 )
                YYERROR;

        errno = preludedb_selected_object_new(&arg, PRELUDEDB_SELECTED_OBJECT_TYPE_INT, &tf);
        if ( errno < 0 ) {
                preludedb_selected_object_destroy(parent);
                YYERROR;
        }

        preludedb_selected_object_push_arg(parent, $3);
        preludedb_selected_object_push_arg(parent, $5);
        preludedb_selected_object_push_arg(parent, arg);
        $$ = parent;
}

func: onearg | intervalfunc | extractfunc

modifier:  tYEAR { $$ = PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR; }
         | tMONTH { $$ = PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH; }
         | tYDAY { $$ = PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY; }
         | tMDAY { $$ = PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY; }
         | tWDAY { $$ = PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY; }
         | tHOUR { $$ = PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR; }
         | tMIN { $$ = PRELUDEDB_SQL_TIME_CONSTRAINT_MIN; }
         | tSEC { $$ = PRELUDEDB_SQL_TIME_CONSTRAINT_SEC; }

valuetype: tIDMEF | tSTRING | tNUMBER | func
value:  valuetype tCOLON modifier {
        int ret;
        preludedb_selected_object_t *f, *num;

        errno = preludedb_selected_object_new(&f, PRELUDEDB_SELECTED_OBJECT_TYPE_EXTRACT, NULL);
        if ( errno < 0 )
                YYERROR;

        errno = preludedb_selected_object_push_arg(f, $1);
        if ( errno < 0 ) {
                preludedb_selected_object_destroy(f);
                return ret;
        }

        errno = preludedb_selected_object_new(&num, PRELUDEDB_SELECTED_OBJECT_TYPE_INT, &$3);
        if ( errno < 0 ) {
                preludedb_selected_object_destroy(f);
                return ret;
        }

        ret = preludedb_selected_object_push_arg(f, num);
        if ( errno < 0 ) {
                preludedb_selected_object_destroy(f);
                preludedb_selected_object_destroy(num);
                return ret;
        }

        $$ = f;
}
      | valuetype
      | tERROR { errno = $1; YYERROR; }

%%

int preludedb_path_selection_parse(preludedb_selected_path_t *root, const char *str)
{
        int ret;
        yyscan_t myscanner;

        yylex_init(&myscanner);
        yy_scan_string(str, myscanner);
        ret = yyparse(myscanner, root);
        yylex_destroy(myscanner);

        return ret;
}


