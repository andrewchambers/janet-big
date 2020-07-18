(declare-project
  :name "big"
  :author "Andrew Chambers"
  :license "MIT"
  :url "https://github.com/andrewchambers/janet-big"
  :repo "git+https://github.com/andrewchambers/janet-big.git")

(declare-native
    :name "big"
    :cflags [;default-cflags "-O2" "-flto"]
    :source ["big.c" "libbf.c" "cutils.c"])
