#include <janet.h>
#include <tommath.h>

static int bigcalc_int_get(void *p, Janet key, Janet *out);

static int bigcalc_int_gc(void *p, size_t len) {
  (void)len;
  mp_int *b = (mp_int *)p;
  // GCTOTALS -= mp->alloc;
  mp_clear(b);
  return 0;
}

static void bigcalc_int_to_string(void *p, JanetBuffer *buf) {
  mp_int *b = (mp_int *)p;
  int sz;
  if (mp_radix_size(b, 10, &sz) != MP_OKAY)
    janet_panic("unable to format string");

  janet_buffer_extra(buf, sz);

  if (mp_toradix_n(b, buf->data + buf->count, 10, sz) != MP_OKAY)
    janet_panic("unable to format string");

  buf->count += sz;
}

static int32_t bigcalc_int_hash(void *p, size_t size) {
  mp_int *b = (mp_int *)p;
  uint32_t hash = 5381;
  for (size_t i = 0; i < USED(b); i++) {
    hash = (hash << 5) + hash + ((char)DIGIT(b, i));
  }
  return (int32_t)hash;
}

static int bigcalc_int_compare(void *p1, void *p2) {
  mp_int *x = (mp_int *)p1;
  mp_int *y = (mp_int *)p2;
  return mp_cmp(x, y);
}

static const JanetAbstractType bigcalc_int_type = {
    "bigcalc/int",
    bigcalc_int_gc,
    NULL,
    bigcalc_int_get,
    NULL,
    NULL, // bigcalc_int_marshal,
    NULL, // bigcalc_int_unmarshal,
    bigcalc_int_to_string,
    bigcalc_int_compare,
    bigcalc_int_hash,
};

static mp_int *bigcalc_coerce_janet_to_int(Janet *argv, int i) {
  mp_err err;

  if (janet_checkabstract(argv[i], &bigcalc_int_type))
    return (mp_int *)janet_unwrap_abstract(argv[i]);

  mp_int *b = janet_abstract(&bigcalc_int_type, sizeof(mp_int));
  if (mp_init(b) != MP_OKAY)
    abort();

  switch (janet_type(argv[i])) {
  case JANET_NUMBER:
    if ((err = mp_set_double(b, janet_unwrap_number(argv[i]))) != MP_OKAY)
      janet_panicf("unable to coerce int: %s", mp_error_to_string(err));
    break;
  case JANET_STRING: {
    JanetString s = janet_unwrap_string(argv[i]);
    if ((err = mp_read_radix(b, (char *)s, 10)) != MP_OKAY)
      janet_panicf("unable to coerce int: %s", mp_error_to_string(err));
    break;
  }
  // TODO u64/s64 types.
  default:
    // XXX print type properly.
    janet_panicf("unable to coerce slot #%d big int", i);
    break;
  }

  return b;
}

static Janet bigcalc_int(int32_t argc, Janet *argv) {
  mp_err err;
  janet_fixarity(argc, 1);

  mp_int *b = janet_abstract(&bigcalc_int_type, sizeof(mp_int));
  if (mp_init(b) != MP_OKAY)
    abort(); /* Simpler to ignore out of memory in this code path for now. */

  switch (janet_type(argv[0])) {
  case JANET_NUMBER:
    if ((err = mp_set_double(b, janet_unwrap_number(argv[0]))) != MP_OKAY)
      janet_panicf("%s", mp_error_to_string(err));
    break;
  case JANET_STRING: {
    JanetString s = janet_unwrap_string(argv[0]);
    if ((err = mp_read_radix(b, (char *)s, 10)) != MP_OKAY)
      janet_panicf("%s", mp_error_to_string(err));
    break;
  }
  // TODO u64/s64 types.
  default:
    // XXX print type properly.
    janet_panic("unable to initialize big int from provided type");
    break;
  }

  return janet_wrap_abstract(b);
}

#define BIGINT_OPMETHOD(name, op)                                              \
  static Janet bigcalc_int_##name(int32_t argc, Janet *argv) {                 \
    mp_err err;                                                                \
    janet_arity(argc, 2, -1);                                                  \
    mp_int *r = janet_abstract(&bigcalc_int_type, sizeof(mp_int));             \
    if (mp_init(r) != MP_OKAY)                                                 \
      abort();                                                                 \
    if ((err =                                                                 \
             mp_copy((mp_int *)janet_getabstract(argv, 0, &bigcalc_int_type),  \
                     r)) != MP_OKAY)                                           \
      janet_panicf("%s", mp_error_to_string(err));                             \
    for (int i = 1; i < argc; i++) {                                           \
      mp_int *b = bigcalc_coerce_janet_to_int(argv, i);                        \
      if ((err = mp_##op(r, b, r)) != MP_OKAY)                                 \
        janet_panicf("%s", mp_error_to_string(err));                           \
    }                                                                          \
    return janet_wrap_abstract(r);                                             \
  }

BIGINT_OPMETHOD(add, add)
BIGINT_OPMETHOD(sub, sub)
BIGINT_OPMETHOD(mul, mul)
BIGINT_OPMETHOD(and, and)
BIGINT_OPMETHOD(or, or)
BIGINT_OPMETHOD (xor, xor)

static Janet bigcalc_int_div(int32_t argc, Janet *argv) {
  mp_err err;
  janet_arity(argc, 2, -1);
  mp_int *r = janet_abstract(&bigcalc_int_type, sizeof(mp_int));
  if (mp_init(r) != MP_OKAY)
    abort();
  mp_int *d = janet_abstract(&bigcalc_int_type, sizeof(mp_int));
  if (mp_init(d) != MP_OKAY)
    abort();
  if ((err = mp_copy((mp_int *)janet_getabstract(argv, 0, &bigcalc_int_type),
                     r)) != MP_OKAY)
    janet_panicf("error during calculation: %s", mp_error_to_string(err));

  for (int i = 1; i < argc; i++) {
    mp_int *b = bigcalc_coerce_janet_to_int(argv, i);
    if ((err = mp_div(r, b, r, d)) != MP_OKAY)
      janet_panicf("error during calculation: %s", mp_error_to_string(err));
  }
  return janet_wrap_abstract(r);
}

static JanetMethod bigcalc_int_methods[] = {{"+", bigcalc_int_add},
                                            {"-", bigcalc_int_sub},
                                            {"*", bigcalc_int_mul},
                                            {"/", bigcalc_int_div},
                                            //{"%", bigcalc_int_mod},
                                            {"&", bigcalc_int_and},
                                            {"|", bigcalc_int_or},
                                            {"^", bigcalc_int_xor},
                                            //{"<<", bigcalc_int_lshift},
                                            //{">>", bigcalc_int_rshift},
                                            {NULL, NULL}};

static int bigcalc_int_get(void *p, Janet key, Janet *out) {
  (void)p;
  if (!janet_checktype(key, JANET_KEYWORD))
    return 0;
  return janet_getmethod(janet_unwrap_keyword(key), bigcalc_int_methods, out);
}

static const JanetReg cfuns[] = {{"int", bigcalc_int,
                                  "(bigcalc/int &opt v)\n\n"
                                  "Create a new integer."},
                                 {NULL, NULL, NULL}};

JANET_MODULE_ENTRY(JanetTable *env) { janet_cfuns(env, "bigcalc", cfuns); }
