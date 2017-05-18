/*
 * TINYEXPR - Tiny recursive descent parser and evaluation engine in C
 *
 * Copyright (c) 2015, 2016 Lewis Van Winkle
 *
 * http://CodePlea.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgement in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "tinyexpr.h"
#include <stdio.h>
#include "minctest.h"


typedef struct {
    const char *expr;
    double answer;
} test_case;

typedef struct {
    const char *expr1;
    const char *expr2;
} test_equ;



void test_results() {
    test_case cases[] = {
        {"1", 1},
        {"1 ", 1},
        {"(1)", 1},

        {"pi", 3.14159},
        {"atan(1)*4 - pi", 0},
        {"e", 2.71828},

        {"2+1", 2+1},
        {"(((2+(1))))", 2+1},
        {"3+2", 3+2},

        {"3+2+4", 3+2+4},
        {"(3+2)+4", 3+2+4},
        {"3+(2+4)", 3+2+4},
        {"(3+2+4)", 3+2+4},

        {"3*2*4", 3*2*4},
        {"(3*2)*4", 3*2*4},
        {"3*(2*4)", 3*2*4},
        {"(3*2*4)", 3*2*4},

        {"3-2-4", 3-2-4},
        {"(3-2)-4", (3-2)-4},
        {"3-(2-4)", 3-(2-4)},
        {"(3-2-4)", 3-2-4},

        {"3/2/4", 3.0/2.0/4.0},
        {"(3/2)/4", (3.0/2.0)/4.0},
        {"3/(2/4)", 3.0/(2.0/4.0)},
        {"(3/2/4)", 3.0/2.0/4.0},

        {"(3*2/4)", 3.0*2.0/4.0},
        {"(3/2*4)", 3.0/2.0*4.0},
        {"3*(2/4)", 3.0*(2.0/4.0)},

        {"asin sin .5", 0.5},
        {"sin asin .5", 0.5},
        {"ln exp .5", 0.5},
        {"exp ln .5", 0.5},

        {"asin sin-.5", -0.5},
        {"asin sin-0.5", -0.5},
        {"asin sin -0.5", -0.5},
        {"asin (sin -0.5)", -0.5},
        {"asin (sin (-0.5))", -0.5},
        {"asin sin (-0.5)", -0.5},
        {"(asin sin (-0.5))", -0.5},

        {"log10 1000", 3},
        {"log10 1e3", 3},
        {"log10 1000", 3},
        {"log10 1e3", 3},
        {"log10(1000)", 3},
        {"log10(1e3)", 3},
        {"log10 1.0e3", 3},
        {"10^5*5e-5", 5},

#ifdef TE_NAT_LOG
        {"log 1000", 6.9078},
        {"log e", 1},
        {"log (e^10)", 10},
#else
        {"log 1000", 3},
#endif

        {"ln (e^10)", 10},
        {"100^.5+1", 11},
        {"100 ^.5+1", 11},
        {"100^+.5+1", 11},
        {"100^--.5+1", 11},
        {"100^---+-++---++-+-+-.5+1", 11},

        {"100^-.5+1", 1.1},
        {"100^---.5+1", 1.1},
        {"100^+---.5+1", 1.1},
        {"1e2^+---.5e0+1e0", 1.1},
        {"--(1e2^(+(-(-(-.5e0))))+1e0)", 1.1},

        {"sqrt 100 + 7", 17},
        {"sqrt 100 * 7", 70},
        {"sqrt (100 * 100)", 100},

        {"1,2", 2},
        {"1,2+1", 3},
        {"1+1,2+2,2+1", 3},
        {"1,2,3", 3},
        {"(1,2),3", 3},
        {"1,(2,3)", 3},
        {"-(1,(2,3))", -3},

        {"2^2", 4},
        {"pow(2,2)", 4},

        {"atan2(1,1)", 0.7854},
        {"atan2(1,2)", 0.4636},
        {"atan2(2,1)", 1.1071},
        {"atan2(3,4)", 0.6435},
        {"atan2(3+3,4*2)", 0.6435},
        {"atan2(3+3,(4*2))", 0.6435},
        {"atan2((3+3),4*2)", 0.6435},
        {"atan2((3+3),(4*2))", 0.6435},

    };


    int i;
    for (i = 0; i < sizeof(cases) / sizeof(test_case); ++i) {
        const char *expr = cases[i].expr;
        const double answer = cases[i].answer;

        int err;
        struct te_expression *n = te_compile(expr, 0, 0, &err);
        lok(n);
        lequal(err, 0);
        const double ev = te_eval(n, NULL, NULL);
        lfequal(ev, answer);
        te_free(n);
        
        if (err) {
            printf("FAILED: %s (%d)\n", expr, err);
        }
    }
}


void test_syntax() {
    test_case errors[] = {
        {"", 1},
        {"1+", 2},
        {"1)", 2},
        {"(1", 2},
        {"1**1", 3},
        {"1*2(+4", 4},
        {"1*2(1+4", 4},
        {"a+5", 1},
        {"A+5", 1},
        {"Aa+5", 1},
        {"1^^5", 3},
        {"1**5", 3},
        {"sin(cos5", 8},
    };


    int i;
    for (i = 0; i < sizeof(errors) / sizeof(test_case); ++i) {
        const char *expr = errors[i].expr;
        const int e = errors[i].answer;

        int err;
        /*
        const double r = te_interp(expr, &err);
        lequal(err, e);
        lok(r != r);
		*/
        struct te_expression *n = te_compile(expr, 0, 0, &err);
        lequal(err, e);
        lok(!n);

        if (err != e) {
            printf("FAILED: %s\n", expr);
        }

        /*const double k = te_interp(expr, 0);
        lok(k != k);*/
    }
}


void test_nans() {

    const char *nans[] = {
        "0/0",
        "1%0",
        "1%(1%0)",
        "(1%0)%1",
        "fac(-1)",
        "ncr(2, 4)",
        "ncr(-2, 4)",
        "ncr(2, -4)",
        "npr(2, 4)",
        "npr(-2, 4)",
        "npr(2, -4)",
    };

    int i;
    for (i = 0; i < sizeof(nans) / sizeof(const char *); ++i) {
        const char *expr = nans[i];

        int err;
        /*
        const double r = te_interp(expr, &err);
        lequal(err, 0);
        lok(r != r);
        */

        struct te_expression *n = te_compile(expr, 0, 0, &err);
        lok(n);
        lequal(err, 0);
        const double c = te_eval(n, NULL, NULL);
        lok(c != c);
        te_free(n);
    }
}


void test_infs() {

    const char *infs[] = {
            "1/0",
            "log(0)",
            "pow(2,10000000)",
            "fac(300)",
            "ncr(300,100)",
            "ncr(300000,100)",
            "ncr(300000,100)*8",
            "npr(3,2)*ncr(300000,100)",
            "npr(100,90)",
            "npr(30,25)",
    };

    int i;
    for (i = 0; i < sizeof(infs) / sizeof(const char *); ++i) {
        const char *expr = infs[i];

        int err;
        /*
        const double r = te_interp(expr, &err);
        lequal(err, 0);
        lok(r == r + 1);
        */

        struct te_expression *n = te_compile(expr, 0, 0, &err);
        lok(n);
        lequal(err, 0);
        const double c = te_eval(n, NULL, NULL);
        lok(c == c + 1);
        te_free(n);
    }
}


void test_variables() {
	const te_variable vars[] = { {"x"}, {"y"}, {"te_st"} };
    double x, y, test = 0;
    double g[3];

    int err;

    struct te_expression *expr1 = te_compile("cos x + sin y", 2, vars, &err);
    if(!expr1){ printf("err = %d\n", err); }
    lok(expr1);
    lok(!err);

    struct te_expression *expr2 = te_compile("x+x+x-y", 2, vars, &err);
    lok(expr2);
    lok(!err);

    struct te_expression *expr3 = te_compile("x*y^3", 2, vars, &err);
    lok(expr3);
    lok(!err);

    struct te_expression *expr4 = te_compile("te_st+5", 3, vars, &err);
    lok(expr4);
    lok(!err);

    for (y = 2; y < 3; ++y) {
        for (x = 0; x < 5; ++x) {
            double ev;
			double val[3];
			val[0] = x; val[1] = y; val[2] = test;
            ev = te_eval(expr1, val, g);
            lfequal(ev, cos(x) + sin(y));

			val[0] = x; val[1] = y; val[2] = test;
            ev = te_eval(expr2, val, g);
            lfequal(ev, x+x+x-y);

			val[0] = x; val[1] = y; val[2] = test;
            ev = te_eval(expr3, val, g);
            lfequal(ev, x*y*y*y);

            test = x;
			val[0] = x; val[1] = y; val[2] = test;
            ev = te_eval(expr4, val, g);
            lfequal(ev, x+5);
        }
    }

    te_free(expr1);
    te_free(expr2);
    te_free(expr3);
    te_free(expr4);



    struct te_expression *expr5 = te_compile("xx*y^3", 2, vars, &err);
    lok(!expr5);
    lok(err);

    struct te_expression *expr6 = te_compile("tes", 3, vars, &err);
    lok(!expr6);
    lok(err);

    struct te_expression *expr7 = te_compile("sinn x", 2, vars, &err);
    lok(!expr7);
    lok(err);

    struct te_expression *expr8 = te_compile("si x", 2, vars, &err);
    lok(!expr8);
    lok(err);
}



#define cross_check(a, b) do {\
    if ((b)!=(b)) break;\
    expr = te_compile((a), 2, vars, &err);\
    lfequal(te_eval(expr, val, g), (b));\
    lok(!err);\
    te_free(expr);\
}while(0)

void test_functions() {
	const te_variable vars[] = { {"x"}, {"y"} };
    double val[2], g[2];

    int err;
    struct te_expression *expr;

    for (val[0] = -5; val[0] < 5; val[0] += .2) {
        cross_check("abs x", fabs(val[0]));
        cross_check("acos x", acos(val[0]));
        cross_check("asin x", asin(val[0]));
        cross_check("atan x", atan(val[0]));
        cross_check("ceil x", ceil(val[0]));
        cross_check("cos x", cos(val[0]));
        cross_check("cosh x", cosh(val[0]));
        cross_check("exp x", exp(val[0]));
        cross_check("floor x", floor(val[0]));
        cross_check("ln x", log(val[0]));
        cross_check("log10 x", log10(val[0]));
        cross_check("sin x", sin(val[0]));
        cross_check("sinh x", sinh(val[0]));
        cross_check("sqrt x", sqrt(val[0]));
        cross_check("tan x", tan(val[0]));
        cross_check("tanh x", tanh(val[0]));

        for (val[1] = -2; val[1] < 2; val[1] += .2) {
            if (fabs(val[0]) < 0.01) break;
            cross_check("atan2(x,y)", atan2(val[0], val[1]));
            cross_check("pow(x,y)", pow(val[0], val[1]));
        }
    }
}


double sum0(void *p, const double *x) {
    return 6;
}
double sum1(void *p, const double *x) {
    return x[0] * 2;
}
double sum2(void *p, const double *x) {
    return x[0] + x[1];
}
double sum3(void *p, const double *x) {
    return x[0] + x[1] + x[2];
}
double sum4(void *p, const double *x) {
    return x[0] + x[1] + x[2] + x[3];
}
double sum5(void *p, const double *x) {
    return x[0] + x[1] + x[2] + x[3] + x[4];
}
double sum6(void *p, const double *x) {
    return x[0] + x[1] + x[2] + x[3] + x[4] + x[5];
}
double sum7(void *p, const double *x) {
    return x[0] + x[1] + x[2] + x[3] + x[4] + x[5] + x[6];
}


void test_dynamic() {

    double val[2], g[2];
    te_variable lookup[] = {
        {"x"},
        {"f"},
        {"sum0", sum0, TE_FUNCTION0},
        {"sum1", sum1, TE_FUNCTION1},
        {"sum2", sum2, TE_FUNCTION2},
        {"sum3", sum3, TE_FUNCTION3},
        {"sum4", sum4, TE_FUNCTION4},
        {"sum5", sum5, TE_FUNCTION5},
        {"sum6", sum6, TE_FUNCTION6},
        {"sum7", sum7, TE_FUNCTION7},
    };

    test_case cases[] = {
        {"x", 2},
        {"f+x", 7},
        {"x+x", 4},
        {"x+f", 7},
        {"f+f", 10},
        {"f+sum0", 11},
        {"sum0+sum0", 12},
        {"sum0()+sum0", 12},
        {"sum0+sum0()", 12},
        {"sum0()+(0)+sum0()", 12},
        {"sum1 sum0", 12},
        {"sum1(sum0)", 12},
        {"sum1 f", 10},
        {"sum1 x", 4},
        {"sum2 (sum0, x)", 8},
        {"sum3 (sum0, x, 2)", 10},
        {"sum2(2,3)", 5},
        {"sum3(2,3,4)", 9},
        {"sum4(2,3,4,5)", 14},
        {"sum5(2,3,4,5,6)", 20},
        {"sum6(2,3,4,5,6,7)", 27},
        {"sum7(2,3,4,5,6,7,8)", 35},
    };

    val[0] = 2;
    val[1] = 5;

    int i;
    for (i = 0; i < sizeof(cases) / sizeof(test_case); ++i) {
        const char *expr = cases[i].expr;
        const double answer = cases[i].answer;

        int err;
        struct te_expression *ex = te_compile(expr, sizeof(lookup)/sizeof(te_variable), lookup, &err);
        lok(ex);
        lfequal(te_eval(ex, val, g), answer);
        te_free(ex);
    }
}


double clo0(void *context, const double *x) {
    if (context) return *((double*)context) + 6;
    return 6;
}
double clo1(void *context, const double *x) {
    if (context) return *((double*)context) + x[0] * 2;
    return x[0] * 2;
}
double clo2(void *context, const double *x) {
    if (context) return *((double*)context) + x[0] + x[1];
    return x[0] + x[1];
}

double cell(void *context, const double *x) {
    double *c = context;
    return c[(int)x[0]];
}

void test_closure() {

    double extra;
    double c[] = {5,6,7,8,9};

    te_variable lookup[] = {
        {"c0", clo0, TE_FUNCTION0, &extra},
        {"c1", clo1, TE_FUNCTION1, &extra},
        {"c2", clo2, TE_FUNCTION2, &extra},
        {"cell", cell, TE_FUNCTION1, c},
    };

    test_case cases[] = {
        {"c0", 6},
        {"c1 4", 8},
        {"c2 (10, 20)", 30},
    };

    int i;
    for (i = 0; i < sizeof(cases) / sizeof(test_case); ++i) {
        const char *expr = cases[i].expr;
        const double answer = cases[i].answer;

        int err;
        struct te_expression *ex = te_compile(expr, sizeof(lookup)/sizeof(te_variable), lookup, &err);
        lok(ex);

        extra = 0;
        lfequal(te_eval(ex, NULL, NULL), answer + extra);

        extra = 10;
        lfequal(te_eval(ex, NULL, NULL), answer + extra);

        te_free(ex);
    }


    test_case cases2[] = {
        {"cell 0", 5},
        {"cell 1", 6},
        {"cell 0 + cell 1", 11},
        {"cell 1 * cell 3 + cell 4", 57},
    };

    for (i = 0; i < sizeof(cases2) / sizeof(test_case); ++i) {
        const char *expr = cases2[i].expr;
        const double answer = cases2[i].answer;

        int err;
        struct te_expression *ex = te_compile(expr, sizeof(lookup)/sizeof(te_variable), lookup, &err);
        lok(ex);
        lfequal(te_eval(ex, NULL, NULL), answer);
        te_free(ex);
    }
}

void test_optimize() {

    test_case cases[] = {
        {"5+5", 10},
        {"pow(2,2)", 4},
        {"sqrt 100", 10},
        {"pi * 2", 6.2832},
    };

    int i;
    for (i = 0; i < sizeof(cases) / sizeof(test_case); ++i) {
        const char *expr = cases[i].expr;
        const double answer = cases[i].answer;

        int err;
        struct te_expression *ex = te_compile(expr, 0, 0, &err);
        lok(ex);

        /* The answer should be know without
         * even running eval. */
        lfequal(te_eval(ex, NULL, NULL), answer);

        te_free(ex);
    }
}

void test_pow() {
#ifdef TE_POW_FROM_RIGHT
    test_equ cases[] = {
        {"2^3^4", "2^(3^4)"},
        {"-2^2", "-(2^2)"},
        {"--2^2", "(2^2)"},
        {"---2^2", "-(2^2)"},
        {"-(2)^2", "-(2^2)"},
        {"-(2*1)^2", "-(2^2)"},
        {"-2^2", "-4"},
        {"2^1.1^1.2^1.3", "2^(1.1^(1.2^1.3))"},
        {"-a^b", "-(a^b)"},
        {"-a^-b", "-(a^-b)"}
    };
#else
    test_equ cases[] = {
        {"2^3^4", "(2^3)^4"},
        {"-2^2", "(-2)^2"},
        {"--2^2", "2^2"},
        {"---2^2", "(-2)^2"},
        {"-2^2", "4"},
        {"2^1.1^1.2^1.3", "((2^1.1)^1.2)^1.3"},
        {"-a^b", "(-a)^b"},
        {"-a^-b", "(-a)^(-b)"}
    };
#endif

	double val[2] = { 2, 3 };
	double g[2];

    te_variable lookup[] = {
        {"a"},
        {"b"}
    };

    int i;
    for (i = 0; i < sizeof(cases) / sizeof(test_equ); ++i) {
        const char *expr1 = cases[i].expr1;
        const char *expr2 = cases[i].expr2;

        struct te_expression *ex1 = te_compile(expr1, sizeof(lookup)/sizeof(te_variable), lookup, 0);
        struct te_expression *ex2 = te_compile(expr2, sizeof(lookup)/sizeof(te_variable), lookup, 0);

        lok(ex1);
        lok(ex2);

        double r1 = te_eval(ex1, val, g);
        double r2 = te_eval(ex2, val, g);

        fflush(stdout);
        lfequal(r1, r2);

        te_free(ex1);
        te_free(ex2);
    }

}

void test_combinatorics() {
    test_case cases[] = {
            {"fac(0)", 1},
            {"fac(0.2)", 1},
            {"fac(1)", 1},
            {"fac(2)", 2},
            {"fac(3)", 6},
            {"fac(4.8)", 24},
            {"fac(10)", 3628800},

            {"ncr(0,0)", 1},
            {"ncr(10,1)", 10},
            {"ncr(10,0)", 1},
            {"ncr(10,10)", 1},
            {"ncr(16,7)", 11440},
            {"ncr(16,9)", 11440},
            {"ncr(100,95)", 75287520},

            {"npr(0,0)", 1},
            {"npr(10,1)", 10},
            {"npr(10,0)", 1},
            {"npr(10,10)", 3628800},
            {"npr(20,5)", 1860480},
            {"npr(100,4)", 94109400},
    };


    int i;
    for (i = 0; i < sizeof(cases) / sizeof(test_case); ++i) {
        const char *expr = cases[i].expr;
        const double answer = cases[i].answer;

        int err;
        struct te_expression *n = te_compile(expr, 0, 0, &err);
        lok(n);
        lequal(err, 0);
        const double ev = te_eval(n, NULL, NULL);
        lfequal(ev, answer);
        te_free(n);
        
        if (err) {
            printf("FAILED: %s (%d)\n", expr, err);
        }
    }
}


int main(int argc, char *argv[])
{
    lrun("Results", test_results);
    lrun("Syntax", test_syntax);
    lrun("NaNs", test_nans);
    lrun("INFs", test_infs);
    lrun("Variables", test_variables);
    lrun("Functions", test_functions);
    lrun("Dynamic", test_dynamic);
    lrun("Closure", test_closure);
    lrun("Optimize", test_optimize);
    lrun("Pow", test_pow);
    lrun("Combinatorics", test_combinatorics);
    lresults();

    return lfails != 0;
}
