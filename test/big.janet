(import ../build/big :as big)
(import spork/test)

(defmacro assert-error
  "Test passes if forms error."
  [msg & forms]
  (def errsym (keyword (gensym)))
  ~(,assert (= ,errsym (try (do ,;forms) ([_] ,errsym))) ,msg))

(defn all-tests [&opt quiet] 
(default quiet false)
(def assert
  (if quiet assert test/assert))

(unless quiet
  (test/start-suite 0))

# stringification (low precision)
(assert (= "77" (string (big/int 77))))

# marshall and unmarshall
(assert (= (unmarshal (marshal (big/int "3435174324234893242542544"))) (big/int "3435174324234893242542544")))

# forward and reverse addition and multiplication
(assert (= (string (+ (big/int 1) (big/int "2") 3)) "6"))
(assert (= (string (+ 1 (big/int 1))) "2"))
(assert (= (big/int 8) (* (big/int 4) 2)))
(assert (= (big/int 77) (* 11 (big/int 7))))

# forward and reverse subtraction and division and mod
(assert (= (big/int 5) (- (big/int 8) 3)))
(assert (= (big/int 5) (- 8 (big/int 3))))
(assert (= (string (/ 4 (big/int 2))) "2"))
(assert (= (string (/ (big/int 4) 2)) "2"))
(assert-error "divide by 0" (/ (big/int 3) 0))

# forward and reverse mod
(assert (= (big/int 708) (% (big/int "23456453431782954574257") 4923)))
(assert (= (big/int 1427) (% 2345678992 (big/int 4711))))

# divmod
(assert (deep= (tuple (big/int "4764666551245775863") (big/int 708)) (big/divmod (big/int "23456453431782954574257") (big/int 4923))))
(assert (deep= (tuple (big/int 6) (big/int 3)) (big/divmod 123 20))) # argument coercion
(assert-error "divide by zero" (big/divmod 123 0))

# test that you can create a big/int from string, number, int/s64 int/u64
(assert (= (big/int "77") (big/int 77) (big/int (int/s64 77)) (big/int (int/u64 77))))
(assert (= (big/int -77) (big/int (int/s64 -77))))

# forward and reverse logic
(assert (= (big/int 5) (band (big/int 7) 5)))
(assert (= (big/int 7) (bor (big/int 7) 5)))
(assert (= (big/int 2) (bxor (big/int 7) 5)))
(assert (= (big/int 5) (band 7 (big/int 5))))
(assert (= (big/int 7) (bor 7 (big/int 5))))
(assert (= (big/int 2) (bxor 7 (big/int 5))))

# test comparison of two big/ints
(assert (= -1 (cmp (big/int 7) (big/int 8))))
(assert (= 0 (cmp (big/int 9) (big/int 9))))
(assert (= 1 (cmp (big/int 9) (big/int 8))))

# test polymorphic comparison -- forward, reverse for janet numbers
(assert (= -1 (compare (big/int 7) 8)))
(assert (= 0 (compare (big/int 7) 7)))
(assert (= 1 (compare (big/int 7) 6)))
(assert (= -1 (compare 6 (big/int 7))))
(assert (= 0 (compare 7 (big/int 7))))
(assert (= 1 (compare 8 (big/int 7))))

# test polymorphic comparison -- forward, reverse for u64/s64
(assert (= -1 (compare (big/int 7) (int/u64 8))))
(assert (= 0 (compare (big/int 7) (int/s64 7))))
(assert (= 1 (compare (big/int 7) (int/s64 -3))))
(assert (= -1 (compare (int/s64 -6) (big/int 7))))
(assert (= 0 (compare (int/u64 7) (big/int 7))))
(assert (= 1 (compare (int/s64 8) (big/int 7))))

# Test sqrt
(assert (= (big/sqrt (big/int "12345678987654321234567890987654321")) (big/int "111111111000000001")))
(assert (= (big/sqrt 50) (big/int 7)))  # argument coercion
(assert-error "negative sqrt" (big/sqrt (big/int -50)))

# Test pow
(defn pow [x y] (product (seq [:repeat y] x))) # hack
(assert (= (pow (big/int 234) 567) (big/pow 234 567)))
(assert-error "negative exponent" (big/pow 234 -5))
(assert (= (big/int -125) (big/pow -5 3)) "negative base")

# confirm no automatic string promotion in math
# (https://github.com/andrewchambers/janet-big/issues/6)
(do
  (def [ok err] (protect (+ (big/int 5) "234")))
  (assert (not ok))
  )

# factorial
(defn fact [n]
  (var f (big/int 1))
  (for i 2 (inc n)
    (*= f i))
  f)

(assert (= (big/int 120) (fact 5)))
(assert (= (fact 100) (big/int "93326215443944152681699238856266700490715968264381621468592963895217599993229915608941463976156518286253697920827223758251185210916864000000000000000000000000")))

# Stringification of long integers -- never enter exponential mode

(assert (= (string (* (big/int 1) ;(range 1 73))) "61234458376886086861524070385274672740778091784697328983823014963978384987221689274204160000000000000000") "precision")

(assert
  (not
    (some
      |(string/find "e" $)
      (seq [y :in [1000 10000 100000]]
        (string (pow (big/int 2) y))))))
(assert (not (string/find "e" (string (* (big/int "1000000000000000000") (pow (big/int 2) 10000))))))
(assert (= (big/pow (big/int 2) 10000) (pow (big/int 2) 10000)))

# Issue10 from @leahneukerchen -- test initializing big/int from large float
(assert (= (big/int "100000000000000000000") (big/int 1e20)))
(assert (= (big/int "1267650600228229401496703205376") (big/int 1.2676506002282294e+30)))

(unless quiet
  (test/end-suite))
)

(all-tests)

# Optional simplistic memory testing:
# To test memory run with "janet test/big.janet -m"
# and watch "top" for any unbounded memory increases

(when (= "-m" (get (dyn :args) 1 ""))
  (repeat 1000000 (all-tests true)))

