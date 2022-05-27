// Copyright (c) 2015-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bench.h"

#include <chrono>
#include <iostream>
#include <map>
#include <regex>
#include <string>

using namespace std::chrono_literals;

benchmark::BenchRunner::BenchmarkMap& benchmark::BenchRunner::benchmarks()
{
    static std::map<std::string, BenchFunction> benchmarks_map;
    return benchmarks_map;
}

benchmark::BenchRunner::BenchRunner(std::string name, benchmark::BenchFunction func)
{
    benchmarks().insert(std::make_pair(name, func));
}

void benchmark::BenchRunner::RunAll(const Args& args)
{
    std::regex reFilter(args.regex_filter);
    std::smatch baseMatch;

    std::vector<ankerl::nanobench::Result> benchmarkResults;
    for (const auto& p : benchmarks()) {
        if (!std::regex_match(p.first, baseMatch, reFilter)) {
            continue;
        }

        Bench bench;
        bench.name(p.first);
        if (args.min_time > 0ms) {
            // convert to nanos before dividing to reduce rounding errors
            std::chrono::nanoseconds min_time_ns = args.min_time;
            bench.minEpochTime(min_time_ns / bench.epochs());
        }

        if (args.asymptote.empty()) {
            p.second(bench);
        } else {
            for (auto n : args.asymptote) {
                bench.complexityN(n);
                p.second(bench);
            }
            std::cout << bench.complexityBigO() << std::endl;
        }

        if (!bench.results().empty()) {
            benchmarkResults.push_back(bench.results().back());
        }
    }
}