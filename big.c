#include <malloc.h>
#include <janet.h>
#include "libbf.h"

static bf_context_t bf_ctx; // is this ok?  threads? &&&

#if LIMB_BITS != 64
#error janet-big requires libbf with LIMB_BITS = 64
#endif

#define BITS_PER_DIGIT_MAX 4  // upper bound bits per decimal digit
#define BITS_PER_DIGIT_MIN 3  // lower bound bits per decimal digit

static void *big_bf_realloc(void *opaque, void *ptr, size_t size) {
  (void) opaque;
  // All libbf internal allocations and frees go through this point.
  // Hook this function to figure out GC pressure 
  if (ptr != NULL) {
    janet_gcpressure(-malloc_usable_size(ptr));
  }
  void *new_alloc = realloc(ptr, size);
  if (new_alloc != NULL) {
    janet_gcpressure(malloc_usable_size(new_alloc));
  }
  return new_alloc;
}

static int big_int_get(void *p, Janet key, Janet *out);

static int big_int_gc(void *p, size_t len) {
  (void)len;
  bf_t *b = (bf_t *)p;
  bf_delete(b);
  return 0;
}

static char *big_digits(bf_t *b, size_t *size) {
  size_t sz;
  // Works only for 64-bit limb_t so we checked during compilation
  // Estimated precision -- can be too high it's ok
  size_t prec = b->len * 64 / BITS_PER_DIGIT_MIN;
  char *digits = bf_ftoa(&sz, b, 10, prec, BF_RNDN | BF_FTOA_FORMAT_FREE_MIN);
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
  //printf("marshalling: '%s' %ld\n", digits, sz+1);
  janet_marshal_abstract(ctx, p);
  janet_marshal_size(ctx, sz+1);
  janet_marshal_bytes(ctx, (uint8_t *)digits, sz+1);
  bf_free(&bf_ctx, digits);
}

static int digits_to_big(bf_t *b, const uint8_t *bytes, size_t sz) {
  size_t prec = sz * BITS_PER_DIGIT_MAX;
  //printf("string is: %s\n", bytes);
  int r = bf_atof(b, (char *)bytes, 0, 10, prec, BF_ATOF_NO_NAN_INF);
  //bf_print_str("resulting bf", b);
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
  case JANET_STRING: {
    JanetString s = janet_unwrap_string(argv[i]);
    if (digits_to_big(b, s, janet_string_length(s)) != 0)
      janet_panicf("unable to convert string to big/int");
    break;
  }
  // TODO u64/s64 types.
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
    bf_set_si(b, (int64_t) janet_unwrap_number(argv[0]));
    break;
  case JANET_STRING: {
    JanetString s = janet_unwrap_string(argv[0]);
    if (digits_to_big(b, s, janet_string_length(s)) != 0)
      janet_panicf("unable to convert string to big/int");
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

#define BIGINT_OPMETHOD(NAME, OP, L, R)                                        \
  static Janet big_int_##NAME(int32_t argc, Janet *argv) {                     \
    janet_fixarity(argc, 2);                                                   \
    bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));                     \
    bf_init(&bf_ctx, r);                                                       \
    bf_t *L = (bf_t *)janet_getabstract(argv, 0, &big_int_type);               \
    bf_t *R = big_coerce_janet_to_int(argv, 1);                                \
    printf("op: %s name: %s\n", #OP, #NAME);\
    bf_print_str("l", L);\
    bf_print_str("r", R);\
    bf_##OP(r, L, R, BF_PREC_INF, BF_RNDN);                                    \
    bf_print_str("res", r);\
    return janet_wrap_abstract(r);                                             \
  }

#define BIGINT_LOGICMETHOD(NAME, OP, L, R)                                        \
  static Janet big_int_##NAME(int32_t argc, Janet *argv) {                     \
    janet_fixarity(argc, 2);                                                   \
    bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));                     \
    bf_init(&bf_ctx, r);                                                       \
    bf_t *L = (bf_t *)janet_getabstract(argv, 0, &big_int_type);               \
    bf_t *R = big_coerce_janet_to_int(argv, 1);                                \
    bf_##OP(r, L, R);                                    \
    return janet_wrap_abstract(r);                                             \
  }


static Janet big_int_mod(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  bf_t *r = janet_abstract(&big_int_type, sizeof(bf_t));
  bf_init(&bf_ctx, r);
  bf_t *L = (bf_t *)janet_getabstract(argv, 0, &big_int_type);
  bf_t *R = big_coerce_janet_to_int(argv, 1);
  bf_rem(r, L, R, BF_PREC_INF, BF_RNDN, BF_RNDN);
  return janet_wrap_abstract(r);
}

BIGINT_OPMETHOD(add, add, a, b)
BIGINT_OPMETHOD(sub, sub, a, b)
BIGINT_OPMETHOD(mul, mul, a, b)
BIGINT_OPMETHOD(div, div, a, b)
BIGINT_LOGICMETHOD(and, logic_and, a, b)
BIGINT_LOGICMETHOD(or, logic_or, a, b)
BIGINT_LOGICMETHOD (xor, logic_xor, a, b)
BIGINT_OPMETHOD(radd, add, b, a)
BIGINT_OPMETHOD(rsub, sub, b, a)
BIGINT_OPMETHOD(rmul, mul, b, a)
BIGINT_OPMETHOD(rdiv, div, a, b)
BIGINT_LOGICMETHOD(rand, logic_and, b, a)
BIGINT_LOGICMETHOD(ror, logic_or, b, a)
BIGINT_LOGICMETHOD(rxor, logic_xor, b, a)

static JanetMethod big_int_methods[] = {{"+", big_int_add},
                                        {"-", big_int_sub},
                                        {"*", big_int_mul},
                                        {"/", big_int_div},
                                        {"%", big_int_mod},
                                        {"&", big_int_and},
                                        {"|", big_int_or},
                                        {"^", big_int_xor},
                                        {"r+", big_int_radd},
                                        {"r-", big_int_rsub},
                                        {"r*", big_int_rmul},
                                        {"r/", big_int_rdiv},
                                        {"r%", big_int_mod},
                                        {"r&", big_int_rand},
                                        {"r|", big_int_ror},
                                        {"r^", big_int_rxor},
                                        //{"<<", big_int_lshift},
                                        //{">>", big_int_rshift},
                                        {NULL, NULL}};

static int big_int_get(void *p, Janet key, Janet *out) {
  (void)p;
  if (!janet_checktype(key, JANET_KEYWORD))
    return 0;
  return janet_getmethod(janet_unwrap_keyword(key), big_int_methods, out);
}

static const JanetReg cfuns[] = {{"int", big_int,
                                  "(big/int &opt v)\n\n"
                                  "Create a new integer."},
                                 {NULL, NULL, NULL}};

JANET_MODULE_ENTRY(JanetTable *env) {
  bf_context_init(&bf_ctx, big_bf_realloc, NULL);
  janet_cfuns(env, "big", cfuns);
  janet_register_abstract_type(&big_int_type);
}

// vim: ts=2:sts=2:sw=2:et:
