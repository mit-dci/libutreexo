#include "bench.h"
#include "util/args.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

static const char* DEFAULT_BENCH_FILTER = ".*";
static constexpr int64_t DEFAULT_MIN_TIME_MS{10};

using namespace benchmark;

static void SetupBenchArgs(ArgsManager& argsman)
{
    argsman.AddArg("-asymptote=<n1,n2,n3,...>", "Test asymptotic growth of the runtime of an algorithm, if supported by the benchmark");
    argsman.AddArg("-filter=<regex>", "Regular expression filter to select benchmark by name (default: " + std::string(DEFAULT_BENCH_FILTER) + ")");
    argsman.AddArg("-min_time=<milliseconds>", "Minimum runtime per benchmark, in milliseconds (default: " + std::to_string(DEFAULT_MIN_TIME_MS) + ")");
    // help options
    argsman.AddArg("-?", "Print this help message and exit");
}

// parses a comma separated list like "10,20,30,50"
static std::vector<double> parseAsymptote(const std::string& str)
{
    std::stringstream ss(str);
    std::vector<double> numbers;
    double d;
    char c;
    while (ss >> d) {
        numbers.push_back(d);
        ss >> c;
    }
    return numbers;
}

int main(int argc, char** argv)
{
    ArgsManager argsman;
    SetupBenchArgs(argsman);
    argsman.ParseParameters(argc, argv);

    // Help text
    if (argsman.IsArgSet("-?") || argsman.IsArgSet("-h")) {
        std::cout << "Usage:  bench_utreexo [options]\n"
                     "\n"
                  << argsman.GetHelpMessage()
                  << "Description:\n"
                     "\n"
                     "  bench_utreexo executes microbenchmarks. The quality of the benchmark results\n"
                     "  highly depend on the stability of the machine. It can sometimes be difficult\n"
                     "  to get stable, repeatable results, so here are a few tips:\n"
                     "\n"
                     "  * Use pyperf [1] to disable frequency scaling, turbo boost etc. For best\n"
                     "    results, use CPU pinning and CPU isolation (see [2]).\n"
                     "\n"
                     "  * Each call of run() should do exactly the same work. E.g. inserting into\n"
                     "    a std::vector doesn't do that as it will reallocate on certain calls. Make\n"
                     "    sure each run has exactly the same preconditions.\n"
                     "\n"
                     "  * If results are still not reliable, increase runtime with e.g.\n"
                     "    -min_time=5000 to let a benchmark run for at least 5 seconds.\n"
                     "\n"
                     "  * bench_utreexo uses nanobench [3] for which there is extensive\n"
                     "    documentation available online.\n"
                     "\n"
                     "Environment Variables:\n"
                     "\n"
                     "  To attach a profiler you can run a benchmark in endless mode. This can be\n"
                     "  done with the environment variable NANOBENCH_ENDLESS. E.g. like so:\n"
                     "\n"
                     "    NANOBENCH_ENDLESS=MuHash ./bench_bitcoin -filter=MuHash\n"
                     "\n"
                     "  In rare cases it can be useful to suppress stability warnings. This can be\n"
                     "  done with the environment variable NANOBENCH_SUPPRESS_WARNINGS, e.g:\n"
                     "\n"
                     "    NANOBENCH_SUPPRESS_WARNINGS=1 ./bench_bitcoin\n"
                     "\n"
                     "Notes:\n"
                     "\n"
                     "  1. pyperf\n"
                     "     https://github.com/psf/pyperf\n"
                     "\n"
                     "  2. CPU pinning & isolation\n"
                     "     https://pyperf.readthedocs.io/en/latest/system.html\n"
                     "\n"
                     "  3. nanobench\n"
                     "     https://github.com/martinus/nanobench\n"
                     "\n";

        return EXIT_SUCCESS;
    }

    // initialize arguments
    Args args;
    args.asymptote = parseAsymptote(argsman.GetArg("-asymptote", ""));
    args.min_time = std::chrono::milliseconds(argsman.GetIntArg("-min_time", DEFAULT_MIN_TIME_MS));
    args.regex_filter = argsman.GetArg("-filter", DEFAULT_BENCH_FILTER);

    BenchRunner::RunAll(args);

    return EXIT_SUCCESS;
}