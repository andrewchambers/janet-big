import time

def time_div(n, d, nreps):
    start = time.time()
    for i in range(nreps):
        q = n // d
    end = time.time()
    return (end - start) / nreps

for line in open("rands.txt"):
    f = line.strip().split()
    nbits = int(f[0])
    dbits = int(f[1])
    n = int(f[2])
    d = int(f[3])
    t = time_div(n, d, 100)
    print(nbits, dbits, int(t * 1000000000))
