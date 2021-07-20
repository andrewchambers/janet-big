(declare-project
  :name "big"
  :description "A simple big integer library for Janet"
  :author "Andrew Chambers"
  :license "MIT"
  :url "https://github.com/andrewchambers/janet-big"
  :repo "git+https://github.com/andrewchambers/janet-big.git"
  :dependencies ["https://github.com/janet-lang/spork.git"])

(declare-native
    :name "big"
    :cflags ["-Wall" "-O2" "-flto"]
    :source ["big.c" "libbf.c" "cutils.c"])
