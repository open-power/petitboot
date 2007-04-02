
#define BOOL int

BOOL pm_process( char *FileName,
                 BOOL (*sfunc)(char *),
                 BOOL (*pfunc)(char *, char *) );
