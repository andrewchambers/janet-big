# generate some random pairs of numbers for division perf tests
from random import getrandbits

for nbits in [1000, 10000, 100000, 300000]:
    for dbits in [nbits//10, nbits//3, nbits//2, 3*nbits//4, nbits-1]:
        print(nbits, dbits, getrandbits(nbits), getrandbits(dbits))

