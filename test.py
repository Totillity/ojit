import time


def main(a1):
    a2 = 0
    while a2 < a1:
        a2 = a2 + 1
    a3  = a2  +  3
    a4  = a3  +  4
    a5  = a4  +  5
    a6  = a5  +  6
    a7  = a6  +  7
    a8  = a7  +  8
    a9  = a8  +  9
    a10 = a9  + 10
    a11 = a10 + 11
    a12 = a11 + 12
    return a12 + 13


print(f"{main(4)}\n")

i = 0
start = time.time()
while True:
    main(3)
    i += 1
    if i % 1000 == 0:
        elasped = time.time() - start
        print(f"\rIteration {i}, Time: {elasped / i * 1e9}", end="", flush=True)
