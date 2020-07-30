(import ../build/big :as big)

(defn all-tests [] 

# stringification
(assert (= "77" (string (big/int 77))))

# marshall and unmarshall
(assert (= (unmarshal (marshal (big/int "3435174324234893242542544"))) (big/int "3435174324234893242542544")))

# forward and reverse addition and multiplication
(assert (= (string (+ (big/int 1) (big/int "2") 3)) "6"))
(assert (= (string (+ 1 (big/int 1))) "2"))
(assert (= (big/int 8) (* (big/int 4) 2)))
(assert (= (big/int 77) (* 11 (big/int 7))))

# forward and reverse subtraction and division
(assert (= (big/int 5) (- (big/int 8) 3)))
(assert (= (big/int 5) (- 8 (big/int 3))))
(assert (= (string (/ 4 (big/int 2))) "2"))
(assert (= (string (/ (big/int 4) 2)) "2"))

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

)

(all-tests)

# Optional simplistic memory testing:
# To test memory run with "janet test/big.janet -m"
# and watch "top" for any unbounded memory increases

(when (= "-m" (get (dyn :args) 1 ""))
  (repeat 1000000 (all-tests)))

