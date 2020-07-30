# big

A big calculator library for janet. Currently only big integers are supported.

# Install

Currently links to libtommath, though this may change in the future.

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

```

See many more examples in test/big.janet


