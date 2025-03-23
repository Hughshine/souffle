#ifndef CLI_H
#define CLI_H
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <regex>
#include <readline/readline.h>
#include <readline/history.h>
#include "souffle/SouffleInterface.h"

class IncrementalCLI {
private:
    std::vector<std::string> commandHistory;

    std::pair<std::string, std::vector<std::string>> parseTuple(const std::string& str) {
        std::string relName;
        std::vector<std::string> values;

        // Look for a pattern with proper word boundaries
        std::regex tupleRegex("\\b([a-zA-Z][a-zA-Z0-9_]*)\\s*\\(([^)]*)\\)");
        std::smatch matches;

        if (std::regex_search(str, matches, tupleRegex) && matches.size() > 2) {
            relName = matches[1].str();
            std::string valuesStr = matches[2].str();

            // Split values by comma
            std::regex valueRegex("\\s*([^,]+)\\s*,?");
            std::string::const_iterator searchStart(valuesStr.cbegin());
            std::smatch valueMatch;

            while (std::regex_search(searchStart, valuesStr.cend(), valueMatch, valueRegex)) {
                std::string value = valueMatch[1].str();
                // Trim whitespace
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                values.push_back(value);
                searchStart = valueMatch.suffix().first;
            }
        }

        return std::make_pair(relName, values);
    }

public:
    IncrementalCLI() {
        // Initialize readline
        using_history();

        // Set up completion if desired (optional)
        // rl_attempted_completion_function = completionFunction;
    }

    ~IncrementalCLI() {
        // Clean up readline history
        clear_history();
    }

    bool processCommand(const std::string& command) {
        // Skip empty commands
        if (command.empty()) {
            return true;
        }

        // Split command into parts
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;

        if (cmd == "help" || cmd == "h") {
            std::cout << "Incremental Souffle CLI Commands:\n"
                      << "--------------------------------\n"
                      << "insert relation_name(val1, val2, ...)   : Queue a tuple for insertion\n"
                      << "delete/remove relation_name(val1, val2...): Queue a tuple for deletion\n"
                      << "commit                                  : Apply queued changes and run incremental computation\n"
                      << "help, h                                 : Display this help message\n"
                      << "exit, quit, q                           : Exit the CLI\n"
                      << std::endl;
            return true;
        }
        else if (cmd == "insert") {
            // Get the rest of the line
            std::string tupleSpec;
            std::getline(iss >> std::ws, tupleSpec);

            // Parse tuple
            auto parsed = parseTuple(tupleSpec);
            if (parsed.first.empty()) {
                std::cout << "Error: Invalid tuple format. Use: relation_name(val1, val2, ...)" << std::endl;
                return true;
            }

            // Output parsed information
            std::cout << "PARSED INSERT: Relation = " << parsed.first << ", Values = [";
            for (size_t i = 0; i < parsed.second.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << parsed.second[i];
            }
            std::cout << "]" << std::endl;

        } else if (cmd == "delete" || cmd == "remove") {
            // Get the rest of the line
            std::string tupleSpec;
            std::getline(iss >> std::ws, tupleSpec);

            // Parse tuple
            auto parsed = parseTuple(tupleSpec);
            if (parsed.first.empty()) {
                std::cout << "Error: Invalid tuple format. Use: relation_name(val1, val2, ...)" << std::endl;
                return true;
            }

            // Output parsed information
            std::cout << "PARSED DELETE: Relation = " << parsed.first << ", Values = [";
            for (size_t i = 0; i < parsed.second.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << parsed.second[i];
            }
            std::cout << "]" << std::endl;

        } else if (cmd == "commit") {
            std::cout << "PARSED COMMIT: Would apply pending changes and run incremental computation" << std::endl;

        } else if (cmd == "exit" || cmd == "quit" || cmd == "q") {
            std::cout << "PARSED EXIT: Exiting CLI" << std::endl;
            return false;

        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
            std::cout << "Use 'help' to see available commands" << std::endl;
        }

        return true;
    }

    void run() {
        bool running = true;

        std::cout << "Incremental Souffle CLI (with command history)" << std::endl;
        std::cout << "Type 'help' for a list of available commands" << std::endl;

        while (running) {
            // Use readline to get input with history support
            char* line = readline("> ");

            // Check for EOF
            if (!line) {
                std::cout << std::endl;
                break;
            }

            // Skip empty lines
            if (line[0] != '\0') {
                // Add to readline history
                add_history(line);

                // Process the command
                std::string command(line);
                running = processCommand(command);
            }

            // Free the memory allocated by readline
            free(line);
        }

        std::cout << "Exiting CLI" << std::endl;
    }
};
#endif //CLI_H
