import time


def main(a1):
    a2 =   a1  + 3
    a3 =   a2  + 3
    a4 =   a3  + 3
    a5 =   a4  + 3
    a6 =   a5  + 3
    a7 =   a6  + 3
    a8 =   a7  + 3
    a9 =   a8  + 3
    a10 =  a9  + 3
    a11 =  a10 + 3
    a12 =  a11 + 3
    return a12 + 3


i = 0
start = time.time()
while True:
    main(3)
    i += 1
    if i % 1000 == 0:
        elasped = time.time() - start
        print(f"\rTime: {elasped / i * 1e9}", end="", flush=True)
