#ifndef CLI_H
#define CLI_H
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "souffle/SouffleInterface.h"

class IncrementalCLI {
private:
    /**
     * Parse a tuple from a string
     * @param str The string to parse (format: "relation_name(val1, val2, ...)")
     * @return Pair of relation name and vector of values as strings
     */
    std::pair<std::string, std::vector<std::string>> parseTuple(const std::string& str) {
        std::string relName;
        std::vector<std::string> values;

        // Modified regex to require relation names to start with a letter
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
    /**
     * Constructor
     */
    IncrementalCLI() {}

    /**
     * Process a single command
     * @param command The command to process
     * @return True if processing should continue, false to exit
     */
    bool processCommand(const std::string& command) {
        // Split command into parts
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;

        if (cmd == "insert") {
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
            std::cout << "Available commands: insert, delete/remove, commit, exit/quit/q" << std::endl;
        }

        return true;
    }

    /**
     * Start the CLI loop
     */
    void run() {
        bool running = true;

        std::cout << "Incremental Souffle CLI (Parser Only)" << std::endl;
        std::cout << "Available commands: insert, delete/remove, commit, exit/quit/q" << std::endl;

        while (running) {
            std::cout << "> ";
            std::string line;
            std::getline(std::cin, line);

            running = processCommand(line);
        }
    }
};
#endif //CLI_H
