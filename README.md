[![Build Status](https://travis-ci.org/codeplea/tinyexpr.svg?branch=master)](https://travis-ci.org/codeplea/tinyexpr)


<img alt="TinyExpr logo" src="https://codeplea.com/public/content/tinyexpr_logo.png" align="right"/>

# TinyExpr

TinyExpr is a very small recursive descent parser and evaluation engine for
math expressions. It's handy when you want to add the ability to evaluation
math expressions at runtime without adding a bunch of cruft to you project.

In addition to the standard math operators and precedence, TinyExpr also supports
the standard C math functions and runtime binding of variables.

## Features

- **ANSI C with no dependencies**.
- Single source file and header file.
- Simple and fast.
- Implements standard operators precedence.
- Exposes standard C math functions (sin, sqrt, ln, etc.).
- Can add custom functions and variables easily.
- Can bind variables at eval-time.
- Released under the zlib license - free for nearly any use.
- Easy to use and integrate with your code
- Thread-safe, provided that your *malloc* is.

## Building

TinyExpr is self-contained in two files: `tinyexpr.c` and `tinyexpr.h`. To use
TinyExpr, simply add those two files to your project.


## Usage

TinyExpr defines only three functions:

```C
    struct te_expression *te_compile(const char *expression, int nvars const te_variable *var, int *error);
    double te_eval(const struct te_expression *expr, const double *val, double *grad);
    void te_free(struct te_expression *expr);
```

## te_compile, te_eval, te_free
```C
    struct te_expression *te_compile(const char *expression, int nlookup, const struct te_variable *lookup, int *error);
    double te_eval(const struct te_expression *n, const double *val, double *grad);
    void te_free(struct te_expression *n);
```

Give `te_compile()` an expression with unbound variables and a list of
variable names and pointers. `te_compile()` will return a `struct te_expression*` which can
be evaluated later using `te_eval()`. On failure, `te_compile()` will return 0
and optionally set the passed in `*error` to the location of the parse error.

You may also compile expressions without variables by passing `te_compile()`'s second
and third arguments as 0.

Give `te_eval()` a `struct te_expression*` from `te_compile()`. `te_eval()` will evaluate the expression
using the given vector of variable values (in the order as they were specified to `te_compile`).
The gradient of expression with respect to each variable is returned in `grad`.

After you're finished, make sure to call `te_free()`.

**example usage:**

```C
    double xy[2], grad[2];
    /* Store variable names and pointers. */
    struct te_variable vars[] = {{"x"}, {"y"}};

    int err;
    /* Compile the expression with variables. */
    struct te_expression *expr = te_compile("sqrt(x^2+y^2)", 2, vars, &err);

    if (expr) {
        xy[0] = 3; xy[1] = 4;
        const double h1 = te_eval(expr, xy, grad); /* Returns 5. */

        xy[0] = 5; xy[1] = 12;
        const double h2 = te_eval(expr, xy, grad); /* Returns 13. */

        te_free(expr);
    } else {
        printf("Parse error at %d\n", err);
    }

```

## Longer Example

Here is a complete example that will evaluate an expression passed in from the command
line. It also does error checking and binds the variables `x` and `y` to *3* and *4*, respectively.

```C
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
        double xy[2], grad[2];
        struct te_variable vars[] = {{"x"}, {"y"}};

        /* This will compile the expression and check for errors. */
        int err;
        struct te_expression *n = te_compile(expression, 2, vars, &err);

        if (n) {
            /* The variables can be changed here, and eval can be called as many
             * times as you like. This is fairly efficient because the parsing has
             * already been done. */
            xy[0] = 3; xy[1] = 4;
            const double r = te_eval(n, xy, grad);
            printf("Result:\n\t%f\n", r);
            printf("Gradient:\n\t%f\t%f\n", grad[0], grad[1]);
            te_free(n);
        } else {
            /* Show the user where the error is at. */
            printf("\t%*s^\nError near here", err-1, "");
        }

        return 0;
    }
```


This produces the output:

    $ example2 "sqrt(x^2+y2)"
        Evaluating:
                sqrt(x^2+y2)
                          ^
        Error near here


    $ example2 "sqrt(x^2+y^2)"
        Evaluating:
                sqrt(x^2+y^2)
        Result:
                5.000000
        Gradient:
                0.600000  0.800000


## Binding to Custom Functions

TinyExpr can also call to custom functions implemented in C. Here is a short example:

```C
double my_sum(void *ctx, double *x, double *g) {
    /* Example C function that adds two numbers together. */
    g[0] = 1; g[1] = 1;
    return x[0] + x[1];
}

struct te_variable vars[] = {
    {"mysum", my_sum, TE_FUNCTION2} /* TE_FUNCTION2 used because my_sum takes two arguments. */
};

struct te_expression *n = te_compile("mysum(5, 6)", 1, vars, 0);

```


## How it works

`te_compile()` uses a simple recursive descent parser to compile your
expression into a syntax tree. For example, the expression `"sin x + 1/4"`
parses as:

![example syntax tree](doc/e1.png?raw=true)

`te_compile()` also automatically prunes constant branches. In this example,
the compiled expression returned by `te_compile()` would become:

![example syntax tree](doc/e2.png?raw=true)

`te_eval()` will automatically load in any variables by their pointer, and then evaluate
and return the result of the expression.

`te_free()` should always be called when you're done with the compiled expression.


## Speed


TinyExpr is pretty fast compared to C when the expression is short, when the
expression does hard calculations (e.g. exponentiation), and when some of the
work can be simplified by `te_compile()`. TinyExpr is slow compared to C when the
expression is long and involves only basic arithmetic.

Here is some example performance numbers taken from the included
**benchmark.c** program:

| Expression | te_eval time | native C time | slowdown  |
| :------------- |-------------:| -----:|----:|
| sqrt(a^1.5+a^2.5) | 15,641 ms | 14,478 ms | 8% slower |
| a+5 | 765 ms | 563 ms | 36% slower |
| a+(5*2) | 765 ms | 563 ms | 36% slower |
| (a+5)*2 | 1422 ms | 563 ms | 153% slower |
| (1/(a+1)+2/(a+2)+3/(a+3)) | 5,516 ms | 1,266 ms | 336% slower |



## Grammar

TinyExpr parses the following grammar:

    <list>      =    <expr> {"," <expr>}
    <expr>      =    <term> {("+" | "-") <term>}
    <term>      =    <factor> {("*" | "/" | "%") <factor>}
    <factor>    =    <power> {"^" <power>}
    <power>     =    {("-" | "+")} <base>
    <base>      =    <constant>
                   | <variable>
                   | <function-0> {"(" ")"}
                   | <function-1> <power>
                   | <function-X> "(" <expr> {"," <expr>} ")"
                   | "(" <list> ")"

In addition, whitespace between tokens is ignored.

Valid variable names consist of a lower case letter followed by any combination
of: lower case letters *a* through *z*, the digits *0* through *9*, and
underscore. Constants can be integers, decimal numbers, or in scientific
notation (e.g.  *1e3* for *1000*). A leading zero is not required (e.g. *.5*
for *0.5*)


## Functions supported

TinyExpr supports addition (+), subtraction/negation (-), multiplication (\*),
division (/), exponentiation (^) and modulus (%) with the normal operator
precedence (the one exception being that exponentiation is evaluated
left-to-right, but this can be changed - see below).

The following C math functions are also supported:

- abs (calls to *fabs*), acos, asin, atan, atan2, ceil, cos, cosh, exp, floor, ln (calls to *log*), log (calls to *log10* by default, see below), log10, pow, sin, sinh, sqrt, tan, tanh

The following functions are also built-in and provided by TinyExpr:

- fac (factorials e.g. `fac 5` == 120)
- ncr (combinations e.g. `ncr(6,2)` == 15)
- npr (permutations e.g. `npr(6,2)` == 30)
- random (random() returns a random number in the half-open interval [0,1))
- round (round(2.2) == 2)
- sign (sign(0) == 0, sign(2.3) == 1, sign(-2.3) == -1)

Also, the following constants are available:

- `pi`, `e`


## Compile-time options


By default, TinyExpr does exponentiation from left to right. For example:

`a^b^c == (a^b)^c` and `-a^b == (-a)^b`

This is by design. It's the way that spreadsheets do it (e.g. Excel, Google Sheets).


If you would rather have exponentiation work from right to left, you need to
define `TE_POW_FROM_RIGHT` when compiling `tinyexpr.c`. There is a
commented-out define near the top of that file. With this option enabled, the
behaviour is:

`a^b^c == a^(b^c)` and `-a^b == -(a^b)`

That will match how many scripting languages do it (e.g. Python, Ruby).

Also, if you'd like `log` to default to the natural log instead of `log10`,
then you can define `TE_NAT_LOG`.

## Hints

- All functions/types start with the letters *te*.

- To allow constant optimization, surround constant expressions in parentheses.
  For example "x+(1+5)" will evaluate the "(1+5)" expression at compile time and
  compile the entire expression as "x+6", saving a runtime calculation. The
  parentheses are important, because TinyExpr will not change the order of
  evaluation. If you instead compiled "x+1+5" TinyExpr will insist that "1" is
  added to "x" first, and "5" is added the result second.

