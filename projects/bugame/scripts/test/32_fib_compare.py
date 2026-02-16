#!/usr/bin/env python3
"""Fibonacci benchmark baseline for BuLang comparison."""

import time


def fib(n: int) -> int:
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)


def bench_case(n: int, iterations: int, expected: int) -> None:
    start = time.perf_counter()
    last = 0

    for _ in range(iterations):
        last = fib(n)

    total_ms = (time.perf_counter() - start) * 1000.0
    per_iter_ms = total_ms / iterations

    print(
        f"[PYTHON] fib({n}) = {last} | iterations={iterations} "
        f"| total_ms={total_ms:.3f} | per_iter_ms={per_iter_ms:.3f}"
    )

    if last != expected:
        raise RuntimeError(f"invalid fibonacci result: got={last}, expected={expected}")


def main() -> None:
    print("========================================")
    print("Python Fibonacci Benchmark")
    print("========================================")

    bench_case(20, 50, 6765)
    bench_case(25, 20, 75025)
    bench_case(30, 5, 832040)

    print("========================================")
    print("Python Fibonacci benchmark complete")
    print("========================================")


if __name__ == "__main__":
    main()
