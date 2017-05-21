#include "tinyexpr.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: example2 \"expression\"\n");
        return 0;
    }

    const char *expression = argv[1];
    printf("Evaluating:\n\t%s\n", expression);

    /* This shows an example where the variables
     * x and y are bound at eval-time. */
    struct te_variable vars[] = {{"x"}, {"y"}};

    /* This will compile the expression and check for errors. */
    int err;
	struct te_expression *n = te_compile(expression, 2, vars, &err);

    if (n) {
        /* The variables can be changed here, and eval can be called as many
         * times as you like. This is fairly efficient because the parsing has
         * already been done. */
        double val[2] = { 2, 3 };
        double g[2];
        const double r = te_eval(n, val, g); printf("Result:\n\t%f, g = %f, %f\n", r, g[0], g[1]);

        te_free(n);
    } else {
        /* Show the user where the error is at. */
        printf("\t%*s^\nError near here", err-1, "");
    }


    return 0;
}
