#include <janet.h>
#if defined(JANET_APPLE)
#include <malloc/malloc.h>
#elif defined(JANET_BSD)
#include <malloc_np.h>
#elif defined(JANET_LINUX) || defined(JANET_WINDOWS)
#include <malloc.h>
#endif
#include <math.h>
#include "libbf.h"

static bf_context_t bf_ctx; // is this ok?  threads? &&&

static void *big_bf_realloc(void *opaque, void *ptr, size_t size) {
  (void) opaque;
  // All libbf internal allocations and frees go through this point.
  // Hook this function to figure out GC pressure
  if (ptr != NULL) {
#if defined(JANET_APPLE)
    janet_gcpressure(-malloc_size(ptr));
#elif defined(JANET_BSD) || defined(JANET_LINUX)
    janet_gcpressure(-malloc_usable_size(ptr));
#elif defined(JANET_WINDOWS)
    janet_gcpressure(-_msize(ptr));
#endif
  }
  void *new_alloc = realloc(ptr, size);
  if (new_alloc != NULL) {
#if defined(JANET_APPLE)
    janet_gcpressure(malloc_size(new_alloc));
#elif defined(JANET_BSD) || defined(JANET_LINUX)
    janet_gcpressure(malloc_usable_size(new_alloc));
#elif defined(JANET_WINDOWS)
    janet_gcpressure(_msize(new_alloc));
#endif
  }
  return new_alloc;
}

static int big_int_get(void *p, Janet key, Janet *out);

// Any bf_t that gets wrapped into a Janet will get automatically
// bf_delete on gc.  This means you must make sure you bf_init any
// bf_t that has been wrapped in a Janet, but also that you do not
// have to bf_delete any bf_t that has been wrapped in a Janet as
// the GC will handle it.
static int big_int_gc(void *p, size_t len) {
  (void)len;
  bf_t *b = (bf_t *)p;
  bf_delete(b);
  return 0;
}

static char *big_digits(bf_t *b, size_t *size) {
  size_t sz;
  //size_t prec = b->len * LIMB_BITS; // &&& correct? too high?
  //char *digits = bf_ftoa(&sz, b, 10, prec, BF_RNDN | BF_FTOA_FORMAT_FREE_MIN);
  char *digits = bf_ftoa(&sz, b, 10, 0, BF_RNDZ | BF_FTOA_FORMAT_FRAC);
  if (size != NULL)
    *size = sz;
  return digits;
}

static void big_int_to_string(void *p, JanetBuffer *buf) {
  bf_t *b = (bf_t *) p;
  size_t sz;
  char *digits = big_digits(b, &sz);
  if (digits == NULL)
    janet_panic("unable to convert big/int to string");
  janet_buffer_push_cstring(buf, digits);
  bf_free(&bf_ctx, digits);
}

static uint32_t hash_add_int64(uint32_t hash, uint64_t v) {
  char *p = (char *) &v;
  for (size_t i = 0; i < 8; i++) {
    hash = (hash << 5) + hash + *(p + i);
  }
  return hash;
}

static int32_t big_int_hash(void *p, size_t size) {
  (void) size;
  bf_t *b = (bf_t *)p;
  uint32_t hash = 5381;
  hash = (hash << 5) + hash + b->sign; // 1 for negative, else 0
  // assumes LIMB_BITS = 64 -- see preprocessor #error line above
  hash = hash_add_int64(hash, b->expn);
  for (size_t i = 0; i < b->len; i++) {
    hash = hash_add_int64(hash, b->tab[i]);
  }
  return (int32_t)hash;
}

static int big_int_compare(void *p1, void *p2) {
  bf_t *x = (bf_t *) p1;
  bf_t *y = (bf_t *) p2;
  return bf_cmp(x, y);
}

static void big_int_marshal(void *p, JanetMarshalContext *ctx) {
  bf_t *b = (bf_t *) p;
  size_t sz;
  char *digits = big_digits(b, &sz);
  if (digits == 0)
    janet_panic("unable to marshall big/int");
  janet_marshal_abstract(ctx, p);
  janet_marshal_size(ctx, sz+1);
  janet_marshal_bytes(ctx, (uint8_t *)digits, sz+1);
  bf_free(&bf_ctx, digits);
}

static int digits_to_big(bf_t *b, const uint8_t *jstring, size_t sz) {
  // Safe to assume that jstring (which is a janet_string) is a zero terminated
  // cstring. (per discussion w/ @bakpakin on gitter.)  But you need to check for
  // embedded zeros.
  char *bytes = (char *) jstring;
  size_t ln = strlen(bytes);
  if (ln < sz) {
    // allow for extra zeros at end due to behavior of unmarshall
    int found_nonzero = 0;
    for (int i = ln; i < sz; i++) {
      found_nonzero += (bytes[i] != 0 ? 1 : 0);
    }
    if (found_nonzero)
      return -1; // embedded zeros not at end
    sz = ln;
  }

  const char *p = bytes;
  if (*p == '-') p++;
  int nd = 0;
  int has_underscore = 0;
  int most_recent_digit_is_underscore = 0;
  const char *end = bytes + sz;
  for (;p < end; p++) {
    most_recent_digit_is_underscore = 0;
    if ((*p < '0' || *p > '9') && *p != '_')
      return -1;
    if (*p == '_') {
      // underscore can not be first digit
      if (nd == 0)
        return -1;
      has_underscore++;
      most_recent_digit_is_underscore = 1;
    } else {
      nd++;
    }
  }
  if (nd == 0)
    return -1;
  if (most_recent_digit_is_underscore)
    return -1;

  if (has_underscore) {
    char *buf = (char *)janet_smalloc(sz);
    p = bytes;
    end = bytes + sz;
    char *t = buf;
    for ( ; p < end; p++) {
      if (*p != '_') {
        *t = *p;
        t++;
      }
    }
    bytes = buf;
  }

  int r = bf_atof(b, (char *)bytes, 0, 10, BF_PREC_INF, BF_RNDZ | BF_ATOF_NO_NAN_INF);

  if (has_underscore)
    janet_sfree(bytes);
  return r;
}

static void *big_int_unmarshal(JanetMarshalContext *ctx) {
  bf_t *b = janet_unmarshal_abstract(ctx, sizeof(bf_t));
  bf_init(&bf_ctx, b);
  size_t sz = janet_unmarshal_size(ctx);
  uint8_t *bytes = janet_smalloc(sz);
  janet_unmarshal_bytes(ctx, bytes, sz);
  if (bytes[sz-1] != 0)
    janet_panicf("invalid big/int data in unmarshall");
  // in case of bad data in unmarshall, panic -- don't return nil.
  if (digits_to_big(b, bytes, sz) != 0)
    janet_panic("unable to unmarshall big/int");
  janet_sfree(bytes);
  return b;
}

static const JanetAbstractType big_int_type = {
    "big/int",
    big_int_gc,
    NULL,
    big_int_get,
    NULL,
    big_int_marshal,
    big_int_unmarshal,
    big_int_to_string,
    big_int_compare,
    big_int_hash,
    JANET_ATEND_HASH
};

static bf_t *big_coerce_janet_to_int(Janet *argv, int i) {
  if (janet_checkabstract(argv[i], &big_int_type))
    return (bf_t *)janet_unwrap_abstract(argv[i]);

  bf_t *b = janet_abstract(&big_int_type, sizeof(bf_t));
  bf_init(&bf_ctx, b);

  switch (janet_type(argv[i])) {
    case JANET_NUMBER:
      bf_set_si(b, (int64_t) janet_unwrap_number(argv[i]));
      break;
    case JANET_ABSTRACT: {
       void *abst = janet_unwrap_abstract(argv[i]);
       if (janet_abstract_type(abst) == &janet_s64_type) {
         bf_set_si(b, *(int64_t *)abst);
       } else if (janet_abstract_type(abst) == &janet_u64_type) {
         bf_set_ui(b, *(uint64_t *)abst);
       } else {
         janet_panicf("unable to coerce slot #%d to big int", i);
       }
       break;
     }
    default:
       janet_panicf("unable to coerce slot #%d to big int", i);
       break;
  }
  return b;
}

static Janet big_int(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);

  if (janet_checkabstract(argv[0], &big_int_type))
    return argv[0];

  bf_t *b = janet_abstract(&big_int_type, sizeof(bf_t));
  bf_init(&bf_ctx, b);

  switch (janet_type(argv[0])) {
  case JANET_NUMBER:
    {
      double x = janet_unwrap_number(argv[0]);
      if (!isfinite(x)) {
        janet_panicf("unable to initialize big int from non-finite number");
      } else {
        bf_set_float64(b, x);
      }
      break;
    }
  case JANET_STRING: {
      JanetString s = janet_unwrap_string(argv[0]);
      // mirroring janet scan-number, bad input leads to returning nil, not panicing
      if (digits_to_big(b, s, janet_string_length(s)) != 0)
        return janet_wrap_nil();
      break;
    }
  case JANET_ABSTRACT: {
       void *abst = janet_unwrap_abstract(argv[0]);
       if (janet_abstract_type(abst) == &janet_s64_type) {
         bf_set_si(b, *(int64_t *)abst);
       } else if (janet_abstract_type(abst) == &janet_u64_type) {
         bf_set_ui(b, *(uint64_t *)abst);
       } else {
         janet_panicf("unable to initilize big int from provided type");
       }
       break;
     }
  default:
    // XXX print type properly.
    janet_panic("unable to initialize big int from provided type");
    break;
  }
  return janet_wrap_abstract(b);
}

static Janet big_int_compare_meth(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);

  if (!janet_checkabstract(argv[0], &big_int_type))
    janet_panic("compare method requires big/int as first argument");
  bf_t *a = (bf_t *)janet_getabstract(argv, 0, &big_int_type);               \

  int r;
  bf_t b; // not wrapped in a Janet: must bf_delete if bf_init called on it

  switch (janet_type(argv[1])) {
  case JANET_NUMBER:
    bf_init(&bf_ctx, &b);
    bf_set_si(&b, (int64_t) janet_unwrap_number(argv[1]));
    r = bf_cmp(a, &b);
    bf_delete(&b);
    break;
  case JANET_ABSTRACT: {
       void *abst = janet_unwrap_abstract(argv[1]);
       if (janet_abstract_type(abst) == &janet_s64_type) {
         bf_init(&bf_ctx, &b);
         bf_set_si(&b, *(int64_t *)abst);
         r = bf_cmp(a, &b);
         bf_delete(&b);
       } else if (janet_abstract_type(abst) == &janet_u64_type) {
         bf_init(&bf_ctx, &b);
         bf_set_ui(&b, *(uint64_t *)abst);
         r = bf_cmp(a, &b);
         bf_delete(&b);
       } else if (janet_abstract_type(abst) == &big_int_type) {
         bf_t *c = (bf_t *)janet_getabstract(argv, 1, &big_int_type);               \
         r = bf_cmp(a, c);
       } else {
         return janet_wrap_nil();
       }
       break;
     }
  default:
    return janet_wrap_nil();
    break;
  }
  return janet_wrap_number(r);
}

#define BIGINT_OPMETHOD(NAME, OP, L, R)                                        \
  static Janet big_int_##NAME(int32_t argc, Janet *argv) {                     \
    janet_fixarity(argc, 2);                                                   \
    bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));                     \
    bf_init(&bf_ctx, r);                                                       \
    bf_t *L = (bf_t *)janet_getabstract(argv, 0, &big_int_type);               \
    bf_t *R = big_coerce_janet_to_int(argv, 1);                                \
    bf_##OP(r, L, R, BF_PREC_INF, BF_RNDZ);                                    \
    return janet_wrap_abstract(r);                                             \
  }

#define BIGINT_ROPMETHOD(NAME, OP, L, R)                                        \
  static Janet big_int_##NAME(int32_t argc, Janet *argv) {                     \
    janet_fixarity(argc, 2);                                                   \
    bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));                     \
    bf_init(&bf_ctx, r);                                                       \
    bf_t *L = (bf_t *)janet_getabstract(argv, 0, &big_int_type);               \
    bf_t *R = big_coerce_janet_to_int(argv, 1);                                \
    bf_##OP(r, R, L, BF_PREC_INF, BF_RNDZ);                                    \
    return janet_wrap_abstract(r);                                             \
  }

#define BIGINT_LOGICMETHOD(NAME, OP, L, R)                                     \
  static Janet big_int_##NAME(int32_t argc, Janet *argv) {                     \
    janet_fixarity(argc, 2);                                                   \
    bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));                     \
    bf_init(&bf_ctx, r);                                                       \
    bf_t *L = (bf_t *)janet_getabstract(argv, 0, &big_int_type);               \
    bf_t *R = big_coerce_janet_to_int(argv, 1);                                \
    bf_##OP(r, L, R);                                                          \
    return janet_wrap_abstract(r);                                             \
  }

#define BIGINT_RLOGICMETHOD(NAME, OP, L, R)                                     \
  static Janet big_int_##NAME(int32_t argc, Janet *argv) {                     \
    janet_fixarity(argc, 2);                                                   \
    bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));                     \
    bf_init(&bf_ctx, r);                                                       \
    bf_t *L = (bf_t *)janet_getabstract(argv, 0, &big_int_type);               \
    bf_t *R = big_coerce_janet_to_int(argv, 1);                                \
    bf_##OP(r, R, L);                                                          \
    return janet_wrap_abstract(r);                                             \
  }

static void big_int_divop(int32_t argc, Janet *argv, int reverse, int mod, bf_t **qp, bf_t **rp) {
  janet_fixarity(argc, 2);
  bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));
  bf_init(&bf_ctx, r);
  bf_t *q = janet_abstract(&big_int_type, sizeof(bf_t));
  bf_init(&bf_ctx, q);
  //bf_t *L = (bf_t *)janet_getabstract(argv, 0, &big_int_type);
  bf_t *L = big_coerce_janet_to_int(argv, 0);
  bf_t *R = big_coerce_janet_to_int(argv, 1);
  int e;
  if (reverse) {
    e = bf_divrem(q, r, R, L, BF_PREC_INF, BF_RNDZ, BF_RNDZ);
  } else {
    e = bf_divrem(q, r, L, R, BF_PREC_INF, BF_RNDZ, BF_RNDZ);
  }
  if (e == BF_ST_INVALID_OP || e == BF_ST_DIVIDE_ZERO) {
    janet_panicf("Invalid argument to divide");
  }
  if (mod && L->sign != R->sign) {
    if (reverse) {
      bf_add(r, r, L, BF_PREC_INF, BF_RNDZ);                                    \
    } else {
      bf_add(r, r, R, BF_PREC_INF, BF_RNDZ);                                    \
    }
  }
  *qp = q;
  *rp = r;
  return;
}

static Janet big_int_div(int32_t argc, Janet *argv) {
  bf_t *q, *r;
  big_int_divop(argc, argv, 0, 0, &q, &r);
  return janet_wrap_abstract(q);
}

static Janet big_int_rem(int32_t argc, Janet *argv) {
  bf_t *q, *r;
  big_int_divop(argc, argv, 0, 0, &q, &r);
  return janet_wrap_abstract(r);
}

static Janet big_int_mod(int32_t argc, Janet *argv) {
  bf_t *q, *r;
  big_int_divop(argc, argv, 0, 1, &q, &r);
  return janet_wrap_abstract(r);
}

static Janet big_int_rdiv(int32_t argc, Janet *argv) {
  bf_t *q, *r;
  big_int_divop(argc, argv, 1, 0, &q, &r);
  return janet_wrap_abstract(q);
}

static Janet big_int_rrem(int32_t argc, Janet *argv) {
  bf_t *q, *r;
  big_int_divop(argc, argv, 1, 0, &q, &r);
  return janet_wrap_abstract(r);
}

static Janet big_int_rmod(int32_t argc, Janet *argv) {
  bf_t *q, *r;
  big_int_divop(argc, argv, 1, 1, &q, &r);
  return janet_wrap_abstract(r);
}

static Janet big_int_divrem(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  bf_t *q, *r;
  big_int_divop(argc, argv, 0, 0, &q, &r);

  Janet tup[2];
  tup[0] = janet_wrap_abstract(q);
  tup[1] = janet_wrap_abstract(r);
  return janet_wrap_tuple(janet_tuple_n(tup, 2));
}

static Janet big_int_divmod(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  bf_t *q, *r;
  big_int_divop(argc, argv, 0, 1, &q, &r);

  Janet tup[2];
  tup[0] = janet_wrap_abstract(q);
  tup[1] = janet_wrap_abstract(r);
  return janet_wrap_tuple(janet_tuple_n(tup, 2));
}

BIGINT_OPMETHOD(add, add, a, b)
BIGINT_OPMETHOD(sub, sub, a, b)
BIGINT_OPMETHOD(mul, mul, a, b)
//BIGINT_OPMETHOD(div, div, a, b)
BIGINT_LOGICMETHOD(and, logic_and, a, b)
BIGINT_LOGICMETHOD(or, logic_or, a, b)
BIGINT_LOGICMETHOD (xor, logic_xor, a, b)
BIGINT_ROPMETHOD(radd, add, b, a)
BIGINT_ROPMETHOD(rsub, sub, b, a)
BIGINT_ROPMETHOD(rmul, mul, b, a)
//BIGINT_OPMETHOD(rdiv, div, a, b)
BIGINT_RLOGICMETHOD(rand, logic_and, b, a)
BIGINT_RLOGICMETHOD(ror, logic_or, b, a)
BIGINT_RLOGICMETHOD(rxor, logic_xor, b, a)

static JanetMethod big_int_methods[] = {{"+", big_int_add},
                                        {"-", big_int_sub},
                                        {"*", big_int_mul},
                                        {"/", big_int_div},
                                        {"%", big_int_rem},
                                        {"mod", big_int_mod},
                                        {"&", big_int_and},
                                        {"|", big_int_or},
                                        {"^", big_int_xor},
                                        {"r+", big_int_radd},
                                        {"r-", big_int_rsub},
                                        {"r*", big_int_rmul},
                                        {"r/", big_int_rdiv},
                                        {"r%", big_int_rrem},
                                        {"rmod", big_int_rmod},
                                        {"r&", big_int_rand},
                                        {"r|", big_int_ror},
                                        {"r^", big_int_rxor},
                                        //{"<<", big_int_lshift},
                                        //{">>", big_int_rshift},
                                        {"compare", big_int_compare_meth},
                                        {NULL, NULL}};

static int big_int_get(void *p, Janet key, Janet *out) {
  (void)p;
  if (!janet_checktype(key, JANET_KEYWORD))
    return 0;
  return janet_getmethod(janet_unwrap_keyword(key), big_int_methods, out);
}

static Janet big_int_pow(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  bf_t *x = big_coerce_janet_to_int(argv, 0);
  bf_t *y = big_coerce_janet_to_int(argv, 1);
  if (y->sign)
    janet_panicf("big/pow called with negative exponent");
  bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));
  bf_init(&bf_ctx, r);
  int e = bf_pow(r, x, y, BF_PREC_INF, BF_RNDZ);
  if (e == BF_ST_INVALID_OP)
    janet_panicf("big/pow invalid operand");
  return janet_wrap_abstract(r);
}

static Janet big_int_sqrt(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  bf_t *x = big_coerce_janet_to_int(argv, 0);
  bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));
  bf_init(&bf_ctx, r);
  int e = bf_sqrtrem(r, NULL, x);
  if (e == BF_ST_INVALID_OP)
    janet_panicf("big/sqrt invalid operand");
  return janet_wrap_abstract(r);
}

static const JanetReg cfuns[] = {
  {"int", big_int,
    "(big/int v)\n\n"
      "Create a new big/int with value of v -- which can be another big/int, a Janet number, int/u64, int/s64, or a string representing a decimal number.  In other functions in this module, arguments can generally be any of the above, except strings.  Strings are only accepted in big/int."},
  {"divrem", big_int_divrem,
    "(big/divrem x y)\n\n"
      "Divide x by y, returning [quotient, remainder] as big/ints. (y != 0)"},
  {"divmod", big_int_divmod,
    "(big/divmod x y)\n\n"
      "Divide x by y, returning [quotient, mod(x,y)] as big/ints. (y != 0)"},
  {"pow", big_int_pow,
    "(big/pow x y)\n\n"
      "Create a new big/int equal to x raised to the y power.  (y >= 0)"},
  {"sqrt", big_int_sqrt,
    "(big/sqrt x)\n\n"
      "Create a new big/int equal to the integer portion of the square root of x. (x bigint >= 0)"},
  {NULL, NULL, NULL}};

JANET_MODULE_ENTRY(JanetTable *env) {
  bf_context_init(&bf_ctx, big_bf_realloc, NULL);
  janet_cfuns(env, "big", cfuns);
  janet_register_abstract_type(&big_int_type);
}

// vim: ts=2:sts=2:sw=2:et:
