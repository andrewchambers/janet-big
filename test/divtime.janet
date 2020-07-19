(import ../build/big :as big)

(var num 0)
(var den 0)
(var acc 0)

(each line (string/split "\n" (slurp "num10000"))
  (let [[k v] (string/split " " line)]
    (case k
      "NUM" (set num (big/int v))
      "DEN" (set den (big/int v))
      "ACC" (set acc (big/int v)))))

(var x 0)
(repeat 1
        #(set x (/ (+ acc (* num 3)) den)))
        (set x (* num den)))

(print x)


