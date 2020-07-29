(import ../build/big :as big)

(defn time-div [n d nreps]
  (def start (os/clock))
  (var x 0)
  (repeat nreps
          (set x (/ n d)))
  (def end (os/clock))
  (/ (- end start) nreps))

(each line (string/split "\n" (slurp "rands.txt"))
  (def f (string/split " " line))
  (def nbits (scan-number (in f 0)))
  (def dbits (scan-number (in f 1)))
  (def n (big/int (in f 2)))
  (def d (big/int (in f 3)))
  (def t (time-div n d 100))
  (def tns (math/floor (* t 1000000000)))
  (printf "%q %q %q" nbits dbits tns))


