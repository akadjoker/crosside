#!/usr/bin/env python3
"""Python baseline for scripts/test/benchmark.bu."""

from time import perf_counter


def run_benchmark(name, func, iterations):
    start = perf_counter()
    for _ in range(iterations):
        func()
    end = perf_counter()

    duration_us = (end - start) * 1_000_000.0
    per_iter_us = duration_us / iterations
    print(
        f"[BENCH] {name}: {duration_us:.2f} us "
        f"({iterations} iterations, {per_iter_us:.2f} us/iter)"
    )


def test_simple_increment():
    i = 0
    while i < 10000:
        i += 1


def test_multiple_vars():
    a = b = c = d = e = 0
    i = 0
    while i < 1000:
        a += 1
        b += 1
        c += 1
        d += 1
        e += 1
        i += 1


def test_local_function():
    total = 0
    i = 0
    while i < 1000:
        local = i
        total = total + local
        i += 1
    return total


def test_arithmetic():
    total = 0
    i = 0
    while i < 5000:
        total = total + i * 2 - 1
        i += 1


def test_many_vars():
    v0 = v1 = v2 = v3 = v4 = 0
    v5 = v6 = v7 = v8 = v9 = 0
    v10 = v11 = v12 = v13 = v14 = 0
    v15 = v16 = v17 = v18 = v19 = 0
    v20 = v21 = v22 = v23 = v24 = 0
    v25 = v26 = v27 = v28 = v29 = 0
    v30 = v31 = v32 = v33 = v34 = 0
    v35 = v36 = v37 = v38 = v39 = 0
    v40 = v41 = v42 = v43 = v44 = 0
    v45 = v46 = v47 = v48 = v49 = 0

    i = 0
    while i < 100:
        v0 += 1
        v10 += 1
        v20 += 1
        v30 += 1
        v40 += 1
        i += 1

    _ = (
        v0
        + v1
        + v2
        + v3
        + v4
        + v5
        + v6
        + v7
        + v8
        + v9
        + v10
        + v11
        + v12
        + v13
        + v14
        + v15
        + v16
        + v17
        + v18
        + v19
        + v20
        + v21
        + v22
        + v23
        + v24
        + v25
        + v26
        + v27
        + v28
        + v29
        + v30
        + v31
        + v32
        + v33
        + v34
        + v35
        + v36
        + v37
        + v38
        + v39
        + v40
        + v41
        + v42
        + v43
        + v44
        + v45
        + v46
        + v47
        + v48
        + v49
    )
    return _


def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)


def test_fibonacci():
    fib(20)


def fact(n):
    if n <= 1:
        return 1
    return n * fact(n - 1)


def test_factorial():
    fact(15)


def test_array():
    arr = []
    i = 0
    while i < 1000:
        arr.append(i)
        i += 1

    total = 0
    i = 0
    n = len(arr)
    while i < n:
        total = total + arr[i]
        i += 1
    return total


class Entity:
    def __init__(self):
        self.x = 0.0
        self.y = 0.0
        self.vx = 1.0
        self.vy = 1.0

    def update(self):
        self.x = self.x + self.vx
        self.y = self.y + self.vy


def test_objects():
    entities = []
    i = 0
    while i < 10:
        entities.append(Entity())
        i += 1

    i = 0
    while i < 100:
        j = 0
        while j < len(entities):
            entities[j].update()
            j += 1
        i += 1


class Particle:
    def __init__(self, px, py):
        self.x = float(px)
        self.y = float(py)
        self.vx = (float(px) - 50.0) / 10.0
        self.vy = (float(py) - 50.0) / 10.0

    def update(self):
        self.x = self.x + self.vx
        self.y = self.y + self.vy
        self.vx = self.vx * 0.99
        self.vy = self.vy * 0.99


def test_physics():
    particles = []
    i = 0
    while i < 100:
        particles.append(Particle(i, i))
        i += 1

    step = 0
    while step < 100:
        i = 0
        while i < len(particles):
            particles[i].update()
            i += 1
        step += 1


def test_nested_loops():
    matrix = []
    i = 0
    while i < 100:
        row = []
        j = 0
        while j < 100:
            row.append(i * j)
            j += 1
        matrix.append(row)
        i += 1
    return matrix


def main():
    print("========================================")
    print("Python Performance Benchmark")
    print("========================================")
    print("")

    iterations = 10
    run_benchmark("Simple increment loop (10000 iterations)", test_simple_increment, iterations)
    run_benchmark("Multiple variables", test_multiple_vars, iterations)
    run_benchmark("Local variables in function", test_local_function, iterations)
    run_benchmark("Arithmetic operations", test_arithmetic, iterations)
    run_benchmark("Many variables (50)", test_many_vars, iterations)
    run_benchmark("Recursive Fibonacci (fib(20))", test_fibonacci, iterations)
    run_benchmark("Recursive Factorial (fact(15))", test_factorial, iterations)
    run_benchmark("Array manipulation (1000 elements)", test_array, iterations)
    run_benchmark("Object property access", test_objects, iterations)
    run_benchmark("Physics simulation (100 particles)", test_physics, iterations)
    run_benchmark("Nested loops with arrays (100x100)", test_nested_loops, iterations)

    print("========================================")
    print("Benchmark complete")
    print("========================================")


if __name__ == "__main__":
    main()
