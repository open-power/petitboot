
%pure-parser
%lex-param { nscan_t scanner }
%parse-param { struct native_parser *parser }
%parse-param { void *scanner }
%error-verbose

%define api.prefix {n}
%{
#include <talloc/talloc.h>
#include <log/log.h>
#include "discover/resource.h"
#include "discover/parser-utils.h"

#include "native.h"

void yyerror(struct native_parser *parser, void *scanner, const char *fmt, ...);
%}

%union {
	char	*word;
	int	num;
}

%token	<word>	TOKEN_WORD
%token	<num>	TOKEN_NUMBER
%token	<num>	TOKEN_DELIM

%token TOKEN_DEFAULT
%token TOKEN_DEV_DESCRIPTION

%token TOKEN_NAME
%token TOKEN_IMAGE
%token TOKEN_INITRD
%token TOKEN_ARGS
%token TOKEN_DTB
%token TOKEN_DESCRIPTION
%token TOKEN_NEWLINE

%{
#include "native-lexer.h"
%}

%%

native:
      globals boot_options { native_parser_finish(parser); }
      | boot_options { native_parser_finish(parser); }
      ;

globals: globals global
       | global
       ;

global: TOKEN_DEFAULT delims TOKEN_WORD {
		if (parser->default_name)
			pb_log_fn("Duplicate default option, ignoring\n");
		else
			parser->default_name = talloc_strdup(parser, $3);
	}
	| TOKEN_DEV_DESCRIPTION delims TOKEN_WORD {
		native_append_string(parser,
			&parser->ctx->device->device->description, $3);
	}
	;

boot_options:
	    boot_options option
	    | option
	    ;

option: name params
     ;

name: TOKEN_NAME delims TOKEN_WORD {
		native_parser_create_option(parser, $3);
	}
	;

params: params param
      | param
      ;

param: TOKEN_IMAGE delims TOKEN_WORD {
		native_set_resource(parser, &parser->opt->boot_image, $3);
	}
	| TOKEN_INITRD delims TOKEN_WORD {
		native_set_resource(parser, &parser->opt->initrd, $3);
	}
	| TOKEN_DTB delims TOKEN_WORD  {
		native_set_resource(parser, &parser->opt->dtb, $3);
	}
	| TOKEN_ARGS delims TOKEN_WORD {
		native_append_string(parser, &parser->opt->option->boot_args, $3);
	}
	| TOKEN_DESCRIPTION delims TOKEN_WORD  {
		native_append_string(parser, &parser->opt->option->description, $3);
	}
	;

delims: delims TOKEN_DELIM
      | TOKEN_DELIM
      ;

%%

void yyerror(struct native_parser *parser, void *scanner, const char *fmt, ...)
{
	const char *str;
	va_list ap;

	va_start(ap, fmt);
	str = talloc_vasprintf(parser, fmt, ap);
	va_end(ap);

	pb_log("parse error: %d('%s'): %s\n", nget_lineno(scanner),
					nget_text(scanner), str);
}

void native_parser_finish(struct native_parser *parser)
{
	if (parser->opt) {
		discover_context_add_boot_option(parser->ctx, parser->opt);
		parser->opt = NULL;
	}
}

void native_set_resource(struct native_parser *parser,
		struct resource ** resource, const char *path)
{
	if (*resource) {
		pb_log_fn("Duplicate resource at line %d: %s\n",
			nget_lineno(parser->scanner), path);
		return;
	}

	*resource = create_devpath_resource(parser->opt, parser->opt->device,
					path);
}

void native_append_string(struct native_parser *parser,
		char **str, const char *append)
{
	if (*str)
		*str = talloc_asprintf_append(*str, "%s", append);
	else
		*str = talloc_strdup(parser->opt, append);
}

void native_parser_create_option(struct native_parser *parser, const char *name)
{
	struct discover_boot_option *opt = parser->opt;

	if (opt)
		native_parser_finish(parser);

	opt = discover_boot_option_create(parser->ctx, parser->ctx->device);
	opt->option->name = talloc_strdup(opt, name);
	opt->option->id = talloc_asprintf(opt, "%s@%p",
			parser->ctx->device->device->id, opt);
	opt->option->type = DISCOVER_BOOT_OPTION;
	opt->option->is_default = parser->default_name &&
				streq(parser->default_name, name);
	parser->opt = opt;
	return;
}

struct native_parser *native_parser_create(struct discover_context *ctx)
{
	struct native_parser *parser;

	parser = talloc_zero(ctx, struct native_parser);
	parser->ctx = ctx;
	nlex_init_extra(parser, &parser->scanner);

	return parser;
}

void native_parser_parse(struct native_parser *parser, const char *filename,
	char *buf, int len)
{
	YY_BUFFER_STATE bufstate;
	int rc;

	if (!len)
		return;

	parser->filename = filename;

	bufstate = n_scan_bytes(buf, len - 1, parser->scanner);
	nset_lineno(1, parser->scanner);

	rc = nparse(parser, parser->scanner);

	if (rc)
		pb_log("Failed to parse %s\n", filename);

	n_delete_buffer(bufstate, parser->scanner);
}

