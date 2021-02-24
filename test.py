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


start = time.time()
for i in range(100000):
    main(3)
end = time.time()
func_time = (end - start) / 100000
print(f"Value: {main(3)} Time: {func_time * 1e9}")