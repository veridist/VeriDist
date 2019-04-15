#pragma once

#include <string>
#include <iostream>

#include <utils.h>

struct NodeInfo {
    int dependency_count;
    std::vector<int> dependents;
};

using Graph = std::vector<NodeInfo>;

bool v_type(const std::string &token) {
    if (token[0] == 'V') {
        return true;
    }

    return false;
}

int get_tok_number(const std::string &token) {
    return stol(token.substr(1));
}

Graph generate_dag(std::vector<std::string> pws_commands) {
    using namespace std;
    cout << "DAG Generation : " << endl;
    
    unordered_map<int,int> tok_to_stmt;

    // TODO: Remove trailing element from the list
    auto dag = Graph(pws_commands.size());

    for (int i = 0; i < pws_commands.size(); ++i) {

        auto const &cmd = pws_commands[i];
        auto tokens = split(cmd, ' ');
        bool inLhs = true;
        for (auto const &token : tokens) {
            if (v_type(token)) {
                
                auto number = get_tok_number(token);
                if(inLhs) {

                    tok_to_stmt[number] = i;

                } else {
                    dag[i].dependency_count++;
                    dag[tok_to_stmt[number]].dependents.push_back(i);
                }
                
            } else if(token[0] == '=') {
                inLhs = false;
            }
        }
    }

    return dag;
}
