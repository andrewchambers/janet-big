(import ../build/big :as big)

(assert (= (unmarshal (marshal (big/int 4))) (big/int 4)))

(assert (= (string (+ (big/int 1) (big/int "2") 3)) "6"))
(assert (= (string (+ 1 (big/int 1))) "2"))

(assert (= (string (/ 4 (big/int 2))) "2"))
(assert (= (string (/ (big/int 4) 2)) "2"))
