#pragma once

#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include <stdexcept>

/*
TODO:
- Required flag for arguments
- Check number of default values matches [min_nvalues, max_nvalues]
- Help message generation (program_name --help)
- Usage message generation (program_name help <command>)
- Custom help message format
- Type conversion cache for ParsedArgument (use std::type_index as key)
*/

namespace ArgCLITool {

class Args {
    struct ParsedArgument {
        std::string name;
        std::vector<std::string> values;
        bool parsed; // true if appeared in command line or has default values
    };

    class ArgGetter {
    public:
        ArgGetter(std::weak_ptr<ParsedArgument> arg) : arg_(arg) {}

        inline operator bool() const {
            return get()->parsed;
        }

        template <typename T>
        inline T as(int index = 0) const {
            auto arg = get();
            if (index < 0 || index >= static_cast<int>(arg->values.size())) {
                throw std::out_of_range("Index " + std::to_string(index) + " out of range for argument: " + arg->name);
            }
            if constexpr (std::is_same_v<T, std::string>) {
                return arg->values[index];
            } else {
                T value;
                std::istringstream iss(arg->values[index]);
                iss >> value;
                if (iss.fail() || !iss.eof()) {
                    throw std::invalid_argument("Invalid value '" + arg->values[index] + "' for argument: " + arg->name);
                }
                return value;
            }
        }

        template <typename T>
        inline T asList() const {
            auto arg = get();
            if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                return arg->values;
            } else {
                std::vector<T> values;
                for (const auto& value : arg->values) {
                    T v;
                    std::istringstream iss(value);
                    iss >> v;
                    if (iss.fail() || !iss.eof()) {
                        throw std::invalid_argument("Invalid value '" + value + "' for argument: " + arg->name);
                    }
                    values.push_back(v);
                }
                return values;
            }
        }

    private:
        inline std::shared_ptr<ParsedArgument> get() const {
            auto arg = arg_.lock();
            if (!arg) {
                throw std::invalid_argument("Argument has been deleted");
            }
            return arg;
        }

    private:
        std::weak_ptr<ParsedArgument> arg_;
    };

public:
    inline ArgGetter operator[](const std::string& name) {
        auto it = arguments_.find(name);
        if (it == arguments_.end()) {
            throw std::invalid_argument("Argument not found: " + name);
        }
        return ArgGetter(it->second);
    }

    inline bool has(const std::string& name) const {
        return arguments_.find(name) != arguments_.end();
    }

    // mappping from argument name to given values
    void set(const std::string& name, const std::vector<std::string>& values = {}, bool parsed = true) {
        std::shared_ptr<ParsedArgument> arg;
        auto it = arguments_.find(name);
        if (it == arguments_.end()) { // not found, create new argument
            arg = std::make_shared<ParsedArgument>();
            argument_list_.push_back(arg);
            arguments_[name] = argument_list_.back();
        } else { // already exists, use it
            arg = it->second;
        }
        // update argument values
        arg->name = name;
        arg->values = values;
        arg->parsed = parsed;
    }

    // mapping both short name and long name to given values
    void set(const std::string& short_name, const std::string& long_name, const std::vector<std::string>& values = {}, bool parsed = true) {
        std::shared_ptr<ParsedArgument> arg;
        auto short_name_it = arguments_.find(short_name);
        auto long_name_it = arguments_.find(long_name);
        if (short_name_it == arguments_.end() && long_name_it == arguments_.end()) { // both not found, create new argument and map both names to it
            arg = std::make_shared<ParsedArgument>();
            argument_list_.push_back(arg);
            // map both names to it
            arguments_[short_name] = argument_list_.back();
            arguments_[long_name] = argument_list_.back();
        } else if (short_name_it != arguments_.end() && long_name_it == arguments_.end()) { // only short name found, map long name to it
            arg = short_name_it->second;
            // map long name to it
            arguments_[long_name] = arg;
        } else if (short_name_it == arguments_.end() /* && long_name_it != arguments_.end() */) { // only long name found, map short name to it
            arg = long_name_it->second;
            // map short name to it
            arguments_[short_name] = arg;
        } else { // both found, check if they are the same argument
            if (short_name_it->second != long_name_it->second) {
                throw std::invalid_argument("Short name and long name are mapped to different arguments: " + short_name + ", " + long_name);
            }
            // same argument, use it
            arg = short_name_it->second;
        }
        // update argument values
        arg->name = short_name;
        arg->values = values;
        arg->parsed = parsed;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<ParsedArgument>> arguments_;
    std::vector<std::shared_ptr<ParsedArgument>> argument_list_;
};

class ArgParser {
    struct Argument {
        std::string position_name; // name in position argument
        std::string short_name;    // short option name
        std::string long_name;     // long option name
        std::string description;
        std::string usage;
        int min_nvalues;           // minimum number of values,
        int max_nvalues;           // maximum number of values, should be greater than or equal to min_nvalues
        // TODO: required flag
        std::vector<std::string> default_values;
    };

    class ArgumentSetter {
    public:
        ArgumentSetter(std::weak_ptr<Argument> arg) : arg_(arg) {}

        ArgumentSetter& description(const std::string& description) {
            get()->description = description;
            return *this;
        }

        ArgumentSetter& usage(const std::string& usage) {
            get()->usage = usage;
            return *this;
        }

        /**
         * @brief Set the number of values for the argument.
         *
         * @param min Minimum number of values. Use `-1` for variadic arguments or `>= 0` for a fixed number of values.
         * @param max Maximum number of values. Must be greater than or equal to `min`. If not specified, `max` is set to `min`.
         *
         * @note Special behavior when `min = 0` for positional arguments:
         * @note  - If `max = -1`, `max` is automatically set to `1`, allowing the argument to have `0` or `1` value.
         * @note  - If `max = 0`, an exception is thrown because a positional argument cannot have exactly `0` values.
         * @note  - If `max > 0`, the argument can have between `0` and `max` values.
         *
         * @return ArgumentSetter& Reference to this object.
         */
        ArgumentSetter& nvalues(int min, int max = -1) {
            // check min and max
            if (min < -1 || max < -1 || (max != -1 && max < min)) {
                throw std::invalid_argument("Invalid number of values: " + std::to_string(min) + ", " + std::to_string(max));
            }
            auto arg = get();
            // check special behavior for positional arguments
            if (!arg->position_name.empty() && min == 0) {
                if (max == 0) {
                    throw std::invalid_argument("Positional argument cannot have exactly 0 values");
                }
                if (max == -1) {
                    max = 1;
                }
            }
            // check variadic argument (TODO: Add this to the note)
            if (min == -1 && max != -1) {
                throw std::invalid_argument("Variadic argument cannot have a maximum number of values");
            }
            // set number of values
            arg->min_nvalues = min;
            arg->max_nvalues = max == -1 ? min : max;
            return *this;
        }

        ArgumentSetter& defaultValues(const std::vector<std::string>& default_values) {
            get()->default_values = default_values;
            return *this;
        }

    private:
        std::shared_ptr<Argument> get() const {
            auto arg = arg_.lock();
            if (!arg) {
                throw std::invalid_argument("Argument has been deleted");
            }
            return arg;
        }

    private:
        std::weak_ptr<Argument> arg_;
    };

private:
    static inline bool isPositional(const std::string& name) { return name.size() >= 1 && name[0] != '-'; }
    static inline bool isShortName(const std::string& name) { return name.size() >= 2 && name[0] == '-' && name[1] != '-' && std::isalpha(name[1]); }
    static inline bool isLongName(const std::string& name) { return name.size() >= 3 && name[0] == '-' && name[1] == '-' && std::isalpha(name[2]); }

public:
    ArgParser& prog(const std::string& program_name) {
        program_name_ = program_name;
        return *this;
    }

    ArgParser& usage(const std::string& usage) {
        usage_ = usage;
        return *this;
    }

    ArgParser& description(const std::string& description) {
        description_ = description;
        return *this;
    }

    ArgParser& epilog(const std::string& epilog) {
        epilog_ = epilog;
        return *this;
    }

    ArgumentSetter add(const std::string& name) {
        // check empty
        if (name.empty()) {
            throw std::invalid_argument("Empty argument name");
        }
        auto arg = std::make_shared<Argument>(Argument{
            .position_name = isPositional(name) ? name : "",
            .short_name = isShortName(name) ? name : "",
            .long_name = isLongName(name) ? name : "",
            .min_nvalues = 0,
            .max_nvalues = 0,
        });
        // check valid name
        if (arg->position_name.empty() && arg->short_name.empty() && arg->long_name.empty()) {
            throw std::invalid_argument("Invalid argument name: " + name);
        }
        // check duplicate
        if (arguments_.find(name) != arguments_.end()) {
            throw std::invalid_argument("Duplicate argument name: " + name);
        }
        // add
        if (!arg->position_name.empty()) {
            // the default number of values for positional argument is 1
            arg->min_nvalues = 1;
            arg->max_nvalues = 1;
            positional_list_.push_back(arg);
            arguments_[arg->position_name] = positional_list_.back();
        } else {
            option_list_.push_back(arg);
            arguments_[arg->short_name.empty() ? arg->long_name : arg->short_name] = option_list_.back();
        }
        return ArgumentSetter(arg);
    }

    ArgumentSetter add(const std::string& short_name, const std::string& long_name) {
        // check empty
        if (short_name.empty() || long_name.empty()) {
            throw std::invalid_argument("Empty argument name");
        }
        // cannot be positional
        if (isPositional(short_name) || isPositional(long_name)) {
            throw std::invalid_argument("Positional argument cannot have multiple names");
        }
        // check name1 is short name and name2 is long name
        if (!isShortName(short_name) || !isLongName(long_name)) {
            throw std::invalid_argument("Invalid argument name: " + short_name + ", " + long_name);
        }
        // check duplicate
        if (arguments_.find(short_name) != arguments_.end() || arguments_.find(long_name) != arguments_.end()) {
            throw std::invalid_argument("Duplicate argument name: " + short_name + ", " + long_name);
        }
        // add
        auto arg = std::make_shared<Argument>(Argument{
            .short_name = short_name,
            .long_name = long_name,
            .min_nvalues = 0,
            .max_nvalues = 0,
        });
        option_list_.push_back(std::move(arg));
        arguments_[short_name] = option_list_.back();
        arguments_[long_name] = option_list_.back();
        return ArgumentSetter(option_list_.back());
    }

    Args parse(int argc, char* argv[]) {
        // parse program info
        if (program_name_.empty()) {
            program_name_ = argv[0];
        }
        // parse arguments
        Args args; // data structure to store parsed arguments
        int positional_count = 0;
        for (int i = 1; i < argc; ++i) {
            std::string input_arg = argv[i];
            bool is_short_name = isShortName(input_arg);
            bool is_long_name = isLongName(input_arg);
            std::shared_ptr<ArgCLITool::ArgParser::Argument> arg; // argument corresponding to input_arg
            if (is_short_name || is_long_name) { // case option argument
                // check argument exists
                auto it = arguments_.find(input_arg);
                if (it == arguments_.end()) {
                    throw std::invalid_argument("Unknown argument: " + input_arg);
                }
                arg = it->second;
                ++i; // skip argument name
            } else { // case positional argument
                // check number of positional arguments is valid
                if (positional_count >= static_cast<int>(positional_list_.size())) {
                    throw std::invalid_argument("Too many positional arguments");
                }
                arg = positional_list_[positional_count++];
            }
            // parse argument values
            std::vector<std::string> values;
            if (arg->min_nvalues == -1) { // case variadic number of values
                for (int j = i; j < argc; ++j) { // greedy consume all values until next option argument
                    std::string value = argv[j];
                    // check value is an option argument
                    if (isShortName(value) || isLongName(value)) {
                        break;
                    }
                    // add value
                    values.push_back(value);
                }
            } else { // case ranged number of values
                for (int j = 0; j < arg->max_nvalues; ++j) {
                    int index = i + j;
                    // check index is valid
                    if (index >= argc) {
                        break;
                    }
                    std::string value = argv[index];
                    // check value is an option argument
                    if (isShortName(value) || isLongName(value)) {
                        break;
                    }
                    // add value
                    values.push_back(value);
                }
                // check number of values is valid
                if (static_cast<int>(values.size()) < arg->min_nvalues) {
                    const std::string& arg_name = (is_short_name || is_long_name) ? input_arg : arg->position_name;
                    throw std::invalid_argument("Not enough values for argument: " + arg_name);
                }
            }
            // set argument values
            if (is_short_name || is_long_name) { // option argument
                // option argument can have both short name and long name
                const std::string& another_name = is_short_name ? arg->long_name : arg->short_name;
                if (another_name.empty()) { // only short name or long name is set
                    args.set(input_arg, values);
                } else { // both short name and long name are set, map both names to the same argument
                    args.set(arg->short_name, arg->long_name, values);
                }
            } else { // positional argument
                args.set(arg->position_name, values);
            }
            // skip parsed values
            i += values.size() - 1; // -1 because i will be incremented in the next loop
        }
        // check the remaining positional arguments have enough values
        for (int i = positional_count; i < static_cast<int>(positional_list_.size()); ++i) {
            const auto& arg = positional_list_[i];
            if (arg->min_nvalues > 0) { // 0 for optional, -1 for variadic
                throw std::invalid_argument("Not enough values for argument: " + arg->position_name);
            }
        }
        // add default values for positional arguments
        for (const auto& arg : positional_list_) {
            // check if the argument has been parsed
            if (args.has(arg->position_name)) {
                continue;
            }
            // add default values
            bool parsed = arg->default_values.empty() ? false : true; // if default values are set, the argument is considered parsed
            args.set(arg->position_name, arg->default_values, parsed);
        }
        // add default values for option arguments
        for (const auto& arg : option_list_) {
            // check if the argument has been parsed
            if (args.has(arg->short_name) || args.has(arg->long_name)) {
                continue;
            }
            // add default values
            bool has_short_name = !arg->short_name.empty();
            bool has_long_name = !arg->long_name.empty();
            bool parsed = arg->default_values.empty() ? false : true; // if default values are set, the argument is considered parsed
            if (has_short_name && has_long_name) { // both short name and long name are set
                args.set(arg->short_name, arg->long_name, arg->default_values, parsed);
            } else { // only short name or long name is set
                auto& name = has_short_name ? arg->short_name : arg->long_name;
                args.set(name, arg->default_values, parsed);
            }
        }
        return args;
    }

private:
    std::string program_name_; // argv[0] if empty
    std::string usage_; // auto generated if empty
    std::string description_;
    std::string epilog_;
    std::unordered_map<std::string, std::shared_ptr<Argument>> arguments_;
    std::vector<std::shared_ptr<Argument>> positional_list_;
    std::vector<std::shared_ptr<Argument>> option_list_;
};

} // namespace ArgCLITool
