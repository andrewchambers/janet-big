# big

A big calculator library for janet. Currently only big integers are supported
(no floating point).

Janet-big supports 

* conversion to big/int from Janet numbers, strings, int/u64 and int/s64
* conversion to string from big/int (using `string` function)
* The basic Janet math operators:  +, -, *, /, %, mod, band, bor, bxor,
  where at least one operand is a big/int, and the other may be big/int
  or number or int/u64 or int/s64.
* comparison operations (between big/ints) and
* polymorphic comparison (big/int vs Janet numbers and int/u64 int/s64,
  but **not** vs strings).  This means predicates from the core like
  even? odd? zero? pos? neg? and one? should work correctly.
* includes big/pow (exponentiation), big/sqrt (integer square root), and
  big/divmod (quotient and remainder returned as a tuple.  These functions
  also accept numbers, big/ints, or int/u64,int/s64 as arguments.
* **Note** -- other numeric functions in the Janet core, like the math/ 
  will generally not work with big/ints.
* **Note** -- of the functions and methods within: only big/int accepts strings 
  as arguments and converts to big/int.  The other functions/methods will generally
  coerce arguments to big/int as needed, but only from Janet numbers, int/u64, int/s64.

# Install

```
jpm install
```

# Quick Usage

```
(import big)

(+ (big/int "999999999999999999999999999999999999999999") 1)

# forward and reverse addition and multiplication
(assert (= (string (+ (big/int 1) (big/int "2") 3)) "6"))
(assert (= (string (+ 1 (big/int 1))) "2"))
(assert (= (big/int 8) (* (big/int 4) 2)))
(assert (= (big/int 77) (* 11 (big/int 7))))

# test that you can create a big/int from string, number, int/s64 int/u64
(assert (= (big/int "77") (big/int 77) (big/int (int/s64 77)) (big/int (int/u64 77))))
(assert (= (big/int -77) (big/int (int/s64 -77))))

# supports polymorphic comparison
(assert (= -1 (compare (big/int 7) 8)))
(assert (= 0 (compare (big/int 7) 7)))
(assert (= 1 (compare (big/int 7) 6)))

# power and sqrt
(assert (= (big/pow 234 567) (product (seq [:repeat 567] (big/int 234)))))
(assert (= (big/sqrt (big/int "12345678987654321234567890987654321")) (big/int "111111111000000001")))
(assert (deep= (tuple (big/int "4764666551245775863") (big/int 708)) (big/divmod (big/int "23456453431782954574257") (big/int 4923))))

```

See many more examples in test/big.janet


