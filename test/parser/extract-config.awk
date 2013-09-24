BEGIN {
	config=0
}

/^#if[ \t]*0[ \t]*\/\*[ \t]*PARSER_EMBEDDED_CONFIG/ {
	config=1
	print "#include <stdlib.h>"
	print "const char __embedded_config[] = "
	next
}
!config {
	next
}
/^#endif/ {
	print ";"
	print "const size_t __embedded_config_size = sizeof(__embedded_config);"
	exit
}
{
	gsub("\"", "\\\"");
	print "\t\"" $0 "\\n\""
}
