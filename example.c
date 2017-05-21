#include "tinyexpr.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    const char *c = "sqrt(5^2+7^2+11^2+(8-2)^2)";
	struct te_expression *n = te_compile(c, 0, NULL, NULL);
    double r = te_eval(n, NULL, NULL);
	te_free(n);
    printf("The expression:\n\t%s\nevaluates to:\n\t%f\n", c, r);
    return 0;
}
