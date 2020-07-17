(import ../build/big :as big)

(var acc (big/int 0))
(var den (big/int 1))
(var num (big/int 1))

(defn extract-digit [n]
  (/ (+ acc (* num n)) den))

(defn eliminate-digit [d]
  (set acc (- acc (* den d)))
  (*= acc 10)
  (*= num 10))

(defn next-term [k]
  (def k2 (+ 1 (* k 2)))
  (+= acc (* num 2))
  (*= acc k2)
  (*= den k2)
  (*= num k))

(def n 10)
(var i 0)
(var k 0)
(while (< i n)
  (++ k)
  (next-term k)
  (when (<= num acc)
    (def d (extract-digit 3))
    (when (= d (extract-digit 4))
      (prin d)
      (++ i)
      (when (= (% i 10) 0)
        (printf "\t: %d" i))
      (eliminate-digit d))))



