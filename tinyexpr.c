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

/* COMPILE TIME OPTIONS */

/* Exponentiation associativity:
For a^b^c = (a^b)^c and -a^b = (-a)^b do nothing.
For a^b^c = a^(b^c) and -a^b = -(a^b) uncomment the next line.*/
/* #define TE_POW_FROM_RIGHT */

/* Logarithms
For log = base 10 log do nothing
For log = natural log uncomment the next line. */
/* #define TE_NAT_LOG */

#include "tinyexpr.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <alloca.h>

#ifndef NAN
#define NAN (0.0/0.0)
#endif

#ifndef INFINITY
#define INFINITY (1.0/0.0)
#endif


typedef struct te_expr {
	int type;
	union {double value; int ivar; const void *function;};
	void *parameters[1];
} te_expr;

typedef struct te_expression {
	te_expr *root;
	int nvar;
	int idx[1];
} te_expression;

typedef double (*te_fun2)(void *, const double *, double *);

enum {
	TOK_NULL = TE_FUNCTION7+1, TOK_ERROR, TOK_END, TOK_SEP,
	TOK_OPEN, TOK_CLOSE, TOK_NUMBER, TOK_VARIABLE, TOK_INFIX
};


enum {TE_CONSTANT = 1};


typedef struct state {
	const char *start;
	const char *next;
	int type;
	union {double value; int ivar; const void *function;};
	void *context;

	const struct te_variable *lookup;
	int lookup_len;
} state;


#define TYPE_MASK(TYPE) ((TYPE)&0x0000001F)

#define IS_PURE(TYPE) (((TYPE) & TE_FLAG_PURE) != 0)
#define IS_FUNCTION(TYPE) (((TYPE) & TE_FUNCTION0) != 0)
#define ARITY(TYPE) ( ((TYPE) & TE_FUNCTION0) ? ((TYPE) & 0x00000007) : 0 )
#define NEW_EXPR(type, ...) new_expr((type), (const te_expr*[]){__VA_ARGS__})

static te_expr *new_expr(const int type, const te_expr *parameters[]) {
	const int arity = ARITY(type);
	const int psize = sizeof(void*) * arity;
	const int size = sizeof(te_expr) + psize;
	te_expr *ret = malloc(size);
	memset(ret, 0, size);
	if (arity && parameters) {
		memcpy(ret->parameters, parameters, psize);
	}
	ret->type = type;
	ret->ivar = -1;
	return ret;
}

static void te_free2(te_expr *n);
static void te_free_parameters(te_expr *n) {
	if (!n) return;
	switch (TYPE_MASK(n->type)) {
		case TE_FUNCTION7: te_free2(n->parameters[6]);
		case TE_FUNCTION6: te_free2(n->parameters[5]);
		case TE_FUNCTION5: te_free2(n->parameters[4]);
		case TE_FUNCTION4: te_free2(n->parameters[3]);
		case TE_FUNCTION3: te_free2(n->parameters[2]);
		case TE_FUNCTION2: te_free2(n->parameters[1]);
		case TE_FUNCTION1: te_free2(n->parameters[0]);
	}
}

static void te_free2(te_expr *n) {
	te_free_parameters(n);
	free(n);
}

void te_free(te_expression *expr) {
	if (!expr) return;
	te_free2(expr->root);
	free(expr);
}


static double pi(void *p, const double *x, double *g) {return 3.14159265358979323846;}
static double e(void *p, const double *x, double *g) {return 2.71828182845904523536;}
static double fac(double a) {/* simplest version of fac */
	if (a < 0.0)
		return NAN;
	if (a > UINT_MAX)
		return INFINITY;
	unsigned int ua = (unsigned int)(a);
	unsigned long int result = 1, i;
	for (i = 1; i <= ua; i++) {
		if (i > ULONG_MAX / result)
			return INFINITY;
		result *= i;
	}
	return (double)result;
}
static double ncr(double n, double r) {
	if (n < 0.0 || r < 0.0 || n < r) return NAN;
	if (n > UINT_MAX || r > UINT_MAX) return INFINITY;
	unsigned long int un = (unsigned int)(n), ur = (unsigned int)(r), i;
	unsigned long int result = 1;
	if (ur > un / 2) ur = un - ur;
	for (i = 1; i <= ur; i++) {
		if (result > ULONG_MAX / (un - ur + i))
			return INFINITY;
		result *= un - ur + i;
		result /= i;
	}
	return result;
}
static double npr(double n, double r) {return ncr(n, r) * fac(r);}
static double myround(double x) {return floor(x+0.5);}
static double myrandom(){
	static const double nrm = 1./(1.+(double)RAND_MAX);
	double f = nrm*rand();
	double f1 = nrm*rand();
	double f2= nrm*rand();
	return f+nrm*(f1+nrm*f2);
}

static double te_wrapped_fabs(void *p, const double *x, double *g){
	if(x[0] == 0){ g[0] = 0; return 0; }
	if(x[0] < 0){ g[0] = -1; return -x[0]; }
	g[0] = 1; return x[0];
}
static double te_wrapped_acos(void *p, const double *x, double *g){
	g[0] = -1./sqrt(1-x[0]*x[0]); return acos(x[0]);
}
static double te_wrapped_asin(void *p, const double *x, double *g){
	g[0] = 1./sqrt(1-x[0]*x[0]); return asin(x[0]);
}
static double te_wrapped_atan(void *p, const double *x, double *g){
	g[0] = 1./(1+x[0]*x[0]); return atan(x[0]);
}
static double te_wrapped_atan2(void *p, const double *x, double *g){
	double d = 1./(x[0]*x[0] + x[1]*x[1]);
	g[0] = x[0]*d; g[1] = -x[1]*d; return atan2(x[0], x[1]);
}
static double te_wrapped_ceil(void *p, const double *x, double *g){
	return ceil(x[0]);
}
static double te_wrapped_cos(void *p, const double *x, double *g){
	g[0] = -sin(x[0]); return cos(x[0]);
}
static double te_wrapped_cosh(void *p, const double *x, double *g){
	g[0] = sinh(x[0]); return cosh(x[0]);
}
static double te_wrapped_exp(void *p, const double *x, double *g){
	double e = exp(x[0]);
	g[0] = e; return e;
}
static double te_wrapped_floor(void *p, const double *x, double *g){
	return floor(x[0]);
}
static double te_wrapped_fmod(void *p, const double *x, double *g){
	double r = fmod(x[0], x[1]);
	g[0] = 1; g[1] = -r; return r;
}
static double te_wrapped_log(void *p, const double *x, double *g){
	g[0] = 1./x[0]; return log(x[0]);
}
static double te_wrapped_log10(void *p, const double *x, double *g){
	g[0] = 1./(log(10.)*x[0]); return log10(x[0]);
}
static double te_wrapped_ncr(void *p, const double *x, double *g){
	return ncr(x[0], x[1]);
}
static double te_wrapped_npr(void *p, const double *x, double *g){
	return npr(x[0], x[1]);
}
static double te_wrapped_pow(void *p, const double *x, double *g){
	double r = pow(x[0], x[1]);
	g[0] = x[1]/x[0]*r; g[1] = r*log(x[0]); return r;
}
static double te_wrapped_random(void *p, const double *x, double *g){
	return myrandom();
}
static double te_wrapped_myround(void *p, const double *x, double *g){
	return myround(x[0]);
}
static double te_wrapped_sign(void *p, const double *x, double *g){
	if(0 == x[0]){ return 0; }
	if(x[0] > 0){ return 1; }
	return -1;
}
static double te_wrapped_sin(void *p, const double *x, double *g){
	g[0] = cos(x[0]); return sin(x[0]);
}
static double te_wrapped_sinh(void *p, const double *x, double *g){
	g[0] = cosh(x[0]); return sinh(x[0]);
}
static double te_wrapped_sqrt(void *p, const double *x, double *g){
	double r = sqrt(x[0]);
	g[0] = 0.5/r; return r;
}
static double te_wrapped_tan(void *p, const double *x, double *g){
	double c = cos(x[0]);
	g[0] = 1./(c*c); return tan(x[0]);
}
static double te_wrapped_tanh(void *p, const double *x, double *g){
	double c = cosh(x[0]);
	g[0] = 1./(c*c); return tanh(x[0]);
}

static const struct te_variable functions[] = {
	/* must be in alphabetical order */
	{"abs"   , te_wrapped_fabs,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"acos"  , te_wrapped_acos,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"asin"  , te_wrapped_asin,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"atan"  , te_wrapped_atan,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"atan2" , te_wrapped_atan2,   TE_FUNCTION2 | TE_FLAG_PURE, 0},
	{"ceil"  , te_wrapped_ceil,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"cos"   , te_wrapped_cos,     TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"cosh"  , te_wrapped_cosh,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"e"     , e,                  TE_FUNCTION0 | TE_FLAG_PURE, 0},
	{"exp"   , te_wrapped_exp,     TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"fac"   , fac,                TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"floor" , te_wrapped_floor,   TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"ln"    , te_wrapped_log,     TE_FUNCTION1 | TE_FLAG_PURE, 0},
#ifdef TE_NAT_LOG
	{"log"   , te_wrapped_log,     TE_FUNCTION1 | TE_FLAG_PURE, 0},
#else
	{"log"   , te_wrapped_log10,   TE_FUNCTION1 | TE_FLAG_PURE, 0},
#endif
	{"log10" , te_wrapped_log10,   TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"ncr"   , te_wrapped_ncr,     TE_FUNCTION2 | TE_FLAG_PURE, 0},
	{"npr"   , te_wrapped_npr,     TE_FUNCTION2 | TE_FLAG_PURE, 0},
	{"pi"    , pi,                 TE_FUNCTION0 | TE_FLAG_PURE, 0},
	{"pow"   , te_wrapped_pow,     TE_FUNCTION2 | TE_FLAG_PURE, 0},
	{"random", te_wrapped_random,  TE_FUNCTION0 | TE_FLAG_PURE, 0},
	{"round" , te_wrapped_myround, TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"sign"  , te_wrapped_sign,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"sin"   , te_wrapped_sin,     TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"sinh"  , te_wrapped_sinh,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"sqrt"  , te_wrapped_sqrt,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"tan"   , te_wrapped_tan,     TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{"tanh"  , te_wrapped_tanh,    TE_FUNCTION1 | TE_FLAG_PURE, 0},
	{0, 0, 0, 0}
};

static const struct te_variable *find_builtin(const char *name, int len) {
	int imin = 0;
	int imax = sizeof(functions) / sizeof(struct te_variable) - 2;

	/*Binary search.*/
	while (imax >= imin) {
		const int i = (imin + ((imax-imin)/2));
		int c = strncmp(name, functions[i].name, len);
		if (!c) c = '\0' - functions[i].name[len];
		if (c == 0) {
			return functions + i;
		} else if (c > 0) {
			imin = i + 1;
		} else {
			imax = i - 1;
		}
	}

	return 0;
}

static const struct te_variable *find_lookup(const state *s, const char *name, int len) {
	int iters;
	const struct te_variable *var;
	if (!s->lookup) return NULL;
	for (var = s->lookup, iters = s->lookup_len; iters; ++var, --iters) {
		if (strncmp(name, var->name, len) == 0 && var->name[len] == '\0') {
			return var;
		}
	}
	return NULL;
}



static double add(void *p, const double *x, double *g) {g[0] = 1; g[1] = 1; return x[0] + x[1];}
static double sub(void *p, const double *x, double *g) {g[0] = 1; g[1] = -1; return x[0] - x[1];}
static double mul(void *p, const double *x, double *g) {g[0] = x[1]; g[1] = x[0]; return x[0] * x[1];}
static double divide(void *p, const double *x, double *g) {double r = 1./x[1]; g[0] = r; g[1] = -x[0]*r*r; return x[0]*r;}
static double negate(void *p, const double *x, double *g) {g[0] = -1; return -x[0];}
static double comma(void *p, const double *x, double *g) {g[0] = 0; g[1] = 1; return x[1];}

static void next_token(state *s) {
	s->type = TOK_NULL;

	do {

		if (!*s->next){
			s->type = TOK_END;
			return;
		}

		/* Try reading a number. */
		if ((s->next[0] >= '0' && s->next[0] <= '9') || s->next[0] == '.') {
			s->value = strtod(s->next, (char**)&s->next);
			s->type = TOK_NUMBER;
		} else {
			/* Look for a variable or builtin function call. */
			if (s->next[0] >= 'a' && s->next[0] <= 'z') {
				const char *start;
				start = s->next;
				while ((s->next[0] >= 'a' && s->next[0] <= 'z') || (s->next[0] >= '0' && s->next[0] <= '9') || (s->next[0] == '_')) s->next++;

				const struct te_variable *var = find_lookup(s, start, s->next - start);
				if (!var) var = find_builtin(start, s->next - start);

				if (!var) {
					s->type = TOK_ERROR;
				} else {
					switch(TYPE_MASK(var->type))
					{
						case TE_VARIABLE:
							s->type = TOK_VARIABLE;
							s->ivar = var - s->lookup;
							break;
						case TE_FUNCTION0: case TE_FUNCTION1: case TE_FUNCTION2: case TE_FUNCTION3:
						case TE_FUNCTION4: case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
							s->context = var->context;
							s->type = var->type;
							s->function = var->address;
							break;
					}
				}

			} else {
				/* Look for an operator or special character. */
				switch (s->next++[0]) {
					case '+': s->type = TOK_INFIX; s->function = add; break;
					case '-': s->type = TOK_INFIX; s->function = sub; break;
					case '*': s->type = TOK_INFIX; s->function = mul; break;
					case '/': s->type = TOK_INFIX; s->function = divide; break;
					case '^': s->type = TOK_INFIX; s->function = te_wrapped_pow; break;
					case '%': s->type = TOK_INFIX; s->function = te_wrapped_fmod; break;
					case '(': s->type = TOK_OPEN; break;
					case ')': s->type = TOK_CLOSE; break;
					case ',': s->type = TOK_SEP; break;
					case ' ': case '\t': case '\n': case '\r': break;
					default: s->type = TOK_ERROR; break;
				}
			}
		}
	} while (s->type == TOK_NULL);
}


static te_expr *list(state *s);
static te_expr *expr(state *s);
static te_expr *power(state *s);

static te_expr *base(state *s) {
	/* <base>      =    <constant> | <variable> | <function-0> {"(" ")"} | <function-1> <power> | <function-X> "(" <expr> {"," <expr>} ")" | "(" <list> ")" */
	te_expr *ret;
	int arity;

	switch (TYPE_MASK(s->type)) {
		case TOK_NUMBER:
			ret = new_expr(TE_CONSTANT, 0);
			ret->value = s->value;
			next_token(s);
			break;

		case TOK_VARIABLE:
			ret = new_expr(TE_VARIABLE, 0);
			ret->ivar = s->ivar;
			next_token(s);
			break;

		case TE_FUNCTION0:
			ret = new_expr(s->type, 0);
			ret->function = s->function;
			ret->parameters[0] = s->context;
			next_token(s);
			if (s->type == TOK_OPEN) {
				next_token(s);
				if (s->type != TOK_CLOSE) {
					s->type = TOK_ERROR;
				} else {
					next_token(s);
				}
			}
			break;

		case TE_FUNCTION1:
			ret = new_expr(s->type, 0);
			ret->function = s->function;
			ret->parameters[1] = s->context;
			next_token(s);
			ret->parameters[0] = power(s);
			break;

		case TE_FUNCTION2: case TE_FUNCTION3: case TE_FUNCTION4:
		case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
			arity = ARITY(s->type);

			ret = new_expr(s->type, 0);
			ret->function = s->function;
			ret->parameters[arity] = s->context;
			next_token(s);

			if (s->type != TOK_OPEN) {
				s->type = TOK_ERROR;
			} else {
				int i;
				for(i = 0; i < arity; i++) {
					next_token(s);
					ret->parameters[i] = expr(s);
					if(s->type != TOK_SEP) {
						break;
					}
				}
				if(s->type != TOK_CLOSE || i != arity - 1) {
					s->type = TOK_ERROR;
				} else {
					next_token(s);
				}
			}

			break;

		case TOK_OPEN:
			next_token(s);
			ret = list(s);
			if (s->type != TOK_CLOSE) {
				s->type = TOK_ERROR;
			} else {
				next_token(s);
			}
			break;

		default:
			ret = new_expr(0, 0);
			s->type = TOK_ERROR;
			ret->value = NAN;
			break;
	}

	return ret;
}


static te_expr *power(state *s) {
	/* <power>     =    {("-" | "+")} <base> */
	int sign = 1;
	while (s->type == TOK_INFIX && (s->function == add || s->function == sub)) {
		if (s->function == sub) sign = -sign;
		next_token(s);
	}

	te_expr *ret;

	if (sign == 1) {
		ret = base(s);
	} else {
		ret = NEW_EXPR(TE_FUNCTION1 | TE_FLAG_PURE, base(s));
		ret->function = negate;
	}

	return ret;
}

#ifdef TE_POW_FROM_RIGHT
static te_expr *factor(state *s) {
	/* <factor>    =    <power> {"^" <power>} */
	te_expr *ret = power(s);

	int neg = 0;
	te_expr *insertion = 0;

	if (ret->type == (TE_FUNCTION1 | TE_FLAG_PURE) && ret->function == negate) {
		te_expr *se = ret->parameters[0];
		free(ret);
		ret = se;
		neg = 1;
	}

	while (s->type == TOK_INFIX && (s->function == te_wrapped_pow)) {
		te_fun2 t = s->function;
		next_token(s);

		if (insertion) {
			/* Make exponentiation go right-to-left. */
			te_expr *insert = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, insertion->parameters[1], power(s));
			insert->function = t;
			insertion->parameters[1] = insert;
			insertion = insert;
		} else {
			ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, power(s));
			ret->function = t;
			insertion = ret;
		}
	}

	if (neg) {
		ret = NEW_EXPR(TE_FUNCTION1 | TE_FLAG_PURE, ret);
		ret->function = negate;
	}

	return ret;
}
#else
static te_expr *factor(state *s) {
	/* <factor>    =    <power> {"^" <power>} */
	te_expr *ret = power(s);

	while (s->type == TOK_INFIX && (s->function == te_wrapped_pow)) {
		te_fun2 t = s->function;
		next_token(s);
		ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, power(s));
		ret->function = t;
	}

	return ret;
}
#endif



static te_expr *term(state *s) {
	/* <term>      =    <factor> {("*" | "/" | "%") <factor>} */
	te_expr *ret = factor(s);

	while (s->type == TOK_INFIX && (s->function == mul || s->function == divide || s->function == te_wrapped_fmod)) {
		te_fun2 t = s->function;
		next_token(s);
		ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, factor(s));
		ret->function = t;
	}

	return ret;
}


static te_expr *expr(state *s) {
	/* <expr>      =    <term> {("+" | "-") <term>} */
	te_expr *ret = term(s);

	while (s->type == TOK_INFIX && (s->function == add || s->function == sub)) {
		te_fun2 t = s->function;
		next_token(s);
		ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, term(s));
		ret->function = t;
	}

	return ret;
}


static te_expr *list(state *s) {
	/* <list>      =    <expr> {"," <expr>} */
	te_expr *ret = expr(s);

	while (s->type == TOK_SEP) {
		next_token(s);
		ret = NEW_EXPR(TE_FUNCTION2 | TE_FLAG_PURE, ret, expr(s));
		ret->function = comma;
	}

	return ret;
}


#define TE_FUN ((double(*)(void *, const double *))n->function)
#define M(e) te_eval(n->parameters[e], val)

static double te_eval2(const te_expression *expr, const te_expr *n, const double *val, double *g) {
	if (!n) return NAN;
	switch(TYPE_MASK(n->type)) {
		case TE_CONSTANT: return n->value;
		case TE_VARIABLE: g[expr->idx[n->ivar]] = 1; return val[n->ivar];
		
		case TE_FUNCTION0: case TE_FUNCTION1: case TE_FUNCTION2: case TE_FUNCTION3:
		case TE_FUNCTION4: case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
		{
			int m = ARITY(n->type);
			int i, j;
			double x[7];
			double df[7] = { 0 };
			double *work = alloca(sizeof(double) * m * expr->nvar);
			double *g1 = work;
			for(i = 0; i < m; ++i){
				for(j = 0; j < expr->nvar; ++j){ g1[j] = 0; }
				x[i] = te_eval2(expr, n->parameters[i], val, g1);
				g1 += expr->nvar;
			}
			double r = ((double(*)(void *, const double *, double *))n->function)(n->parameters[m], x, df);
			for(j = 0; j < expr->nvar; ++j){ g[j] = 0; }
			g1 = work;
			for(i = 0; i < m; ++i){
				if(0 != df[i]){
					for(j = 0; j < expr->nvar; ++j){
						g[j] += df[i]*g1[j];
					}
				}
				g1 += expr->nvar;
			}
			return r;
		}

		default: return NAN;
	}
}
double te_eval(const te_expression *expr, const double *val, double *g) {
	return te_eval2(expr, expr->root, val, g);
}

#undef TE_FUN
#undef M

static void optimize(te_expression *expr, te_expr *n) {
	/* Evaluates as much as possible. */
	if (n->type == TE_CONSTANT) return;
	if (n->type == TE_VARIABLE) return;

	/* Only optimize out functions flagged as pure. */
	if (IS_PURE(n->type)) {
		const int arity = ARITY(n->type);
		int known = 1;
		int i;
		for (i = 0; i < arity; ++i) {
			optimize(expr, n->parameters[i]);
			if (((te_expr*)(n->parameters[i]))->type != TE_CONSTANT) {
				known = 0;
			}
		}
		if (known) {
			double *work = alloca(sizeof(double) * arity * expr->nvar);
			const double value = te_eval2(expr, n, NULL, work);
			te_free_parameters(n);
			n->type = TE_CONSTANT;
			n->value = value;
		}
	}
}


te_expression *te_compile(const char *expression, int var_count, const struct te_variable *variables, int *error) {
	state s;
	s.start = s.next = expression;
	s.lookup = variables;
	s.lookup_len = var_count;

	next_token(&s);
	te_expr *root = list(&s);

	if (s.type != TOK_END) {
		te_free2(root);
		if (error) {
			*error = (s.next - s.start);
			if (*error == 0) *error = 1;
		}
		return NULL;
	}
	if (error) *error = 0;
	
	te_expression *ret = malloc(sizeof(te_expression) + sizeof(int)*(var_count-1));
	ret->root = root;
	ret->nvar = 0;
	{
		int i;
		for(i = 0; i < var_count; ++i){
			if(TE_VARIABLE == variables[i].type){
				ret->idx[i] = ret->nvar++;
			}else{
				ret->idx[i] = 0;
			}
		}
	}
	optimize(ret, root);
	return ret;
}

static void pn (const te_expr *n, int depth) {
	int i, arity;
	printf("%*s", depth, "");

	switch(TYPE_MASK(n->type)) {
	case TE_CONSTANT: printf("%f\n", n->value); break;
	case TE_VARIABLE: printf("ivar %d\n", n->ivar); break;

	case TE_FUNCTION0: case TE_FUNCTION1: case TE_FUNCTION2: case TE_FUNCTION3:
	case TE_FUNCTION4: case TE_FUNCTION5: case TE_FUNCTION6: case TE_FUNCTION7:
		arity = ARITY(n->type);
		printf("f%d", arity);
		for(i = 0; i < arity; i++) {
			printf(" %p", n->parameters[i]);
		}
		printf("\n");
		for(i = 0; i < arity; i++) {
			pn(n->parameters[i], depth + 1);
		}
		break;
	}
}


void te_print(const te_expression *n) {
	pn(n->root, 0);
}
