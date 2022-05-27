#include "args.h"

#include <cassert>
#include <sstream>
#include <string>

// trimmed down version of https://github.com/bitcoin/bitcoin/blob/23.x/src/util/system.cpp#L306
void benchmark::ArgsManager::ParseParameters(int argc, const char* const argv[])
{
    m_command_line_options.clear();

    for (int i = 1; i < argc; i++) {
        std::string key(argv[i]);
        std::string value;
        size_t is_index = key.find('=');

        if (is_index != std::string::npos) {
            value = key.substr(is_index + 1);
            key.erase(is_index);
        }

        // key is -foo
        m_command_line_options[key] = value;
    }
}

// trimmed down version of https://github.com/bitcoin/bitcoin/blob/23.x/src/util/system.cpp#L677
std::string benchmark::ArgsManager::GetHelpMessage()
{
    std::string usage = "";
    usage += std::string("Options") + std::string("\n\n");
    for (const auto& arg : m_available_args) {
        std::string name;
        if (arg.second.m_help_param.empty()) {
            name = arg.first;
        } else {
            name = arg.first + arg.second.m_help_param;
        }
        usage += HelpMessageOpt(name, arg.second.m_help_text);
    }
    return usage;
}

static const int optIndent = 2;
static const int msgIndent = 7;

// trimmed down version of https://github.com/bitcoin/bitcoin/blob/23.x/src/util/system.cpp#L765
std::string benchmark::ArgsManager::HelpMessageOpt(const std::string& option, const std::string& message)
{
    return std::string(optIndent, ' ') + std::string(option) +
           std::string("\n") + std::string(msgIndent, ' ') + std::string(message) +
           std::string("\n\n");
}

// trimmed down version of https://github.com/bitcoin/bitcoin/blob/23.x/src/util/system.cpp#L649
void benchmark::ArgsManager::AddArg(const std::string& name, const std::string& help)
{
    // Split arg name from its help param
    size_t eq_index = name.find('=');
    if (eq_index == std::string::npos) {
        eq_index = name.size();
    }
    std::string arg_name = name.substr(0, eq_index);

    std::map<std::string, Arg>& arg_map = m_available_args;
    auto ret = arg_map.emplace(arg_name, Arg{name.substr(eq_index, name.size() - eq_index), help});
    assert(ret.second); // Make sure an insertion actually happened
}

std::string benchmark::ArgsManager::GetArg(const std::string& strArg, const std::string& strDefault) const
{
    try {
        const std::string& value = m_command_line_options.at(strArg);
        return value;
    } catch (const std::out_of_range&) {
        return strDefault;
    }
}

int64_t benchmark::ArgsManager::GetIntArg(const std::string& strArg, int64_t nDefault) const
{
    try {
        const std::string& value = m_command_line_options.at(strArg);
        return stoi(value);
    } catch (const std::out_of_range&) {
        return nDefault;
    };
}

bool benchmark::ArgsManager::IsArgSet(const std::string& strArg) const
{
    try {
        m_command_line_options.at(strArg);
        return true;
    } catch (const std::out_of_range&) {
        return false;
    }
}