Benchmarking
============

Utreexo has an internal benchmarking framework that uses the [nanobench library](https://github.com/martinus/nanobench) and follows the Bitcoin Core benchmarking implementation.

Benchmarks are compiled by default and can be disabled with

```
./configure --disable-bench
```


Running
---------------------
After compiling, the benchmarks can be run with

```
./bench_utreexo
```

#### Arguments

The `-min_time=<milliseconds>` argument allows to run the benchmark for much longer than the default. When results are unreliable, choosing a large timeframe here should usually get repeatable results.
For more details see https://github.com/bitcoin/bitcoin/pull/23025

The `-filter=<regex>` argument is a regular expression filter to select benchmark by name. For example, to run

- only forest related benchmarks: `./bench_utreexo -filter=.*Forest`
- only a single benchmark: `./bench_utreexo -filter=<benchmark-name>`

The `-asymptote=<n1,n2,n3,...>` argument allows for dynamic parameters and then calculates asymptotic complexity (Big O) from multiple runs of the benchmark with different complexity N. [Read more about nanobench's asymptotic complexity](https://nanobench.ankerl.com/tutorial.html#asymptotic-complexity).


