(declare-project
  :name "bigcalc"
  :author "Andrew Chambers"
  :license "MIT"
  :url "https://github.com/andrewchambers/janet-bigcalc"
  :repo "git+https://github.com/andrewchambers/janet-bigcalc.git")

(defn pkg-config [what]
  (def f (file/popen (string "pkg-config " what)))
  (def v (->>
           (file/read f :all)
           (string/trim)
           (string/split " ")))
  (unless (zero? (file/close f))
    (error "pkg-config failed!"))
  v)

(declare-native
    :name "bigcalc"
    :cflags (pkg-config "libtommath --cflags")
    :lflags (pkg-config "libtommath --libs")
    :source ["bigcalc.c"])
