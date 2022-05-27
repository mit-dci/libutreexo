#ifndef UTREEXO_BENCH_UTIL_ARGS_H
#define UTREEXO_BENCH_UTIL_ARGS_H

#include <chrono>
#include <map>
#include <string>
#include <vector>


namespace benchmark {

class ArgsManager
{
protected:
    struct Arg {
        std::string m_help_param;
        std::string m_help_text;
    };
    std::map<std::string, Arg> m_available_args;
    //! Map of setting name to list of command line values.
    std::map<std::string, std::string> m_command_line_options;

public:
    void ParseParameters(int argc, const char* const argv[]);

    /**
     * Get the help string
     */
    std::string GetHelpMessage();

    /**
     * Format a string to be used as option description in help messages
     *
     * @param option Option message (e.g. "-rpcuser=<user>")
     * @param message Option description (e.g. "Username for JSON-RPC connections")
     * @return the formatted string
     */
    std::string HelpMessageOpt(const std::string& option, const std::string& message);

    /**
     * Add argument
     */
    void AddArg(const std::string& name, const std::string& help);

    /**
     * Return string argument or default value
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @param strDefault (e.g. "1")
     * @return command-line argument or default value
     */
    std::string GetArg(const std::string& strArg, const std::string& strDefault) const;

    /**
     * Return integer argument or default value
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @param nDefault (e.g. 1)
     * @return command-line argument (0 if invalid number) or default value
     */
    int64_t GetIntArg(const std::string& strArg, int64_t nDefault) const;

    /**
     * Return true if the given argument has been manually set
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @return true if the argument has been set
     */
    bool IsArgSet(const std::string& strArg) const;
};

} // namespace benchmark
#endif // UTREEXO_BENCH_UTIL_ARGS_H