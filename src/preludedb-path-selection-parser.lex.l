
%{
  #include <stdio.h>
  #include <string.h>
  #include "preludedb-path-selection.h"
  #include "preludedb-path-selection-parser.yac.h"

  #define TOKEN(id) return t##id
%}


%option reentrant bison-bridge
%option noyywrap nounput noinput

NUMBER [0-9](_[0-9]|[0-9])*

%%

\"([^"]|\\.)*\" {
        int ret;

        ret = preludedb_selected_object_new_string(&yylval->object, yytext + 1, strlen(yytext + 1) - 1);
        if ( ret < 0 ) {
                yylval->error = ret;
                TOKEN(ERROR);
        }

        TOKEN(STRING);
}

\'([^']|\\.)*\' {
        int ret;

        ret = preludedb_selected_object_new_string(&yylval->object, yytext + 1, strlen(yytext + 1) - 1);
        if ( ret < 0 ) {
                yylval->error = ret;
                TOKEN(ERROR);
        }

        TOKEN(STRING);
}

-?([0-9]+|[0-9]*\.[0-9]+([eE][-+]?[0-9]+)?) {
        int ret, num = atoi(yytext);

        ret = preludedb_selected_object_new(&yylval->object, PRELUDEDB_SELECTED_OBJECT_TYPE_INT, &num);
        if ( ret < 0 ) {
                yylval->error = ret;
                TOKEN(ERROR);
        }

        TOKEN(NUMBER);
}

"max" { TOKEN(MAX); }
"count" { TOKEN(COUNT); }
"sum" { TOKEN(SUM); }
"avg" { TOKEN(AVG); }
"interval" { TOKEN(INTERVAL); }
"extract" { TOKEN(EXTRACT); }

"year" { TOKEN(YEAR); }
"quarter" { TOKEN(QUARTER); }
"month" { TOKEN(MONTH); }
"week" { TOKEN(WEEK); }
"yday" { TOKEN(YDAY); }
"mday" { TOKEN(MDAY); }
"wday" { TOKEN(WDAY); }
"hour" { TOKEN(HOUR); }
"min" { TOKEN(MIN); }
"sec" { TOKEN(SEC); }
"msec" { TOKEN(MSEC); }
"usec" { TOKEN(USEC); }


"order_asc" { TOKEN(ORDER_ASC); }
"order_desc" { TOKEN(ORDER_DESC); }
"group_by" { TOKEN(GROUP_BY); }

(alert|heartbeat)\.([a-zA-Z0-9_\-]+(\(((\-?[0-9\*]+)|(\"[^"]+\")|(\'[^']+\'))\))?\.?)+ {
        int ret;

        ret = preludedb_selected_object_new(&yylval->object, PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH, yytext);
        if ( ret < 0 ) {
                yylval->error = ret;
                TOKEN(ERROR);
        }

        TOKEN(IDMEF);
}

"(" { TOKEN(LPAREN); }
")" { TOKEN(RPAREN); }
"," { TOKEN(COMMA); }
":" { TOKEN(COLON); }
"/" { TOKEN(SLASH); }
[[:space:]]+    // skip whitespace
. { fprintf(stderr, "Unknown token '%s'\n", yytext); }

%%

