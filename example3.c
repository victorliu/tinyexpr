#include "tinyexpr.h"
#include <stdio.h>


/* An example of calling a C function. */
double my_sum(void *ctx, double *ab, double *g) {
    printf("Called C function with %f and %f.\n", ab[0], ab[1]);
	g[0] = 1; g[1] = 1;
    return ab[0] + ab[1];
}


int main(int argc, char *argv[])
{
   struct te_variable vars[] = {
        {"mysum", my_sum, TE_FUNCTION2}
    };

    const char *expression = "mysum(5, 6)";
    printf("Evaluating:\n\t%s\n", expression);

    int err;
    struct te_expression *n = te_compile(expression, 1, vars, &err);

    if (n) {
        const double r = te_eval(n, NULL, NULL);
        printf("Result:\n\t%f\n", r);
        te_free(n);
    } else {
        /* Show the user where the error is at. */
        printf("\t%*s^\nError near here", err-1, "");
    }


    return 0;
}
