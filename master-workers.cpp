#include <simgrid/s4u.hpp>
#include <random>
#include <vector>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <limits>

#include <iostream>
#include <string>

#include <unordered_set>

#include <dag-generation.h>
#include <utils.h>

XBT_LOG_NEW_DEFAULT_CATEGORY(s4u_app_masterworker, "Messages specific for this s4u example");

std::string program_name;
std::string const partial_files_dir = "partials/";

struct worker_request {
    bool from_master;
    int num_vars;
    int num_depends_on;
    std::string pws_commands;
    std::vector<std::string> dependents;
    std::vector<int> intermediates;

    worker_request(int num_vars, int num_depends_on, std::string pws_commands, std::vector<std::string> dependents)
    : num_vars(num_vars),
      num_depends_on(num_depends_on),
      pws_commands(pws_commands),
      dependents(dependents) {
          from_master = true;
      }

    worker_request(std::vector<int> intermediates)
    : intermediates(intermediates) {
          from_master = false;
      }
};

struct worker_response {
    std::vector<int> input_output;

    worker_response(std::vector<int> input_output) : input_output(input_output) {}
};

struct comp_params {
    int n_constraints;
    int n_inputs;
    int n_outputs;
    int n_vars;
};

// Class containing worker code
class Worker {
    std::string pws_commands;
    std::vector<std::string> dependents;
    std::vector<int> intermediates;
    simgrid::s4u::MailboxPtr mailbox = nullptr;
    simgrid::s4u::MailboxPtr master_mailbox = nullptr;

    void BuildIntermediates() {
        int num_depends_on = 1000;
        for (int i = 0; i < num_depends_on + 1; i++) {
            worker_request* req = static_cast<worker_request*>(mailbox->get());
            
            if (req->from_master == true) {
                // XBT_INFO("Got master message\n");
                if (intermediates.empty()) intermediates.assign(req->num_vars, 0);
                num_depends_on = req->num_depends_on;
                pws_commands = req->pws_commands;
                dependents = req->dependents;
            }
            else {
                // XBT_INFO("Got peer message\n");
                if (intermediates.empty()) intermediates.assign(req->intermediates.size(), 0);
                for (int j = 0; j < req->intermediates.size(); j++) {
                    if (req->intermediates[j] != 0)
                        intermediates[j] = req->intermediates[j];
                }
            }
            delete req;
        }
    }

    public:

    explicit Worker(std::vector<std::string> args)
    {
        xbt_assert(args.size() == 2, "The worker expects the name of the master host");

        mailbox = simgrid::s4u::Mailbox::by_name(simgrid::s4u::this_actor::get_host()->get_name());
        master_mailbox = simgrid::s4u::Mailbox::by_name(args[1]);

        mailbox->set_receiver(simgrid::s4u::Actor::self());
    }

    void operator()() {
        BuildIntermediates();

        std::ofstream v_file;
        std::string v_filename = partial_files_dir +  RandomString(32) + ".partial";
        v_file.open(v_filename);
        for (auto &intermediate : intermediates) {
            v_file << intermediate << " ";
        }
        v_file << std::endl;
        v_file.close();

        std::ofstream pws_file;
        std::string pws_filename = partial_files_dir + RandomString(32) + ".pws";
        pws_file.open(pws_filename);
        pws_file.write(pws_commands.c_str(), pws_commands.size());
        pws_file.close();

        std::string command = "./pepper_partial_prover_" + program_name + " " 
            + program_name + ".params "
            + pws_filename + " "
            + program_name + ".inputs "
            + v_filename;

        std::cout << "Command : " << command << std::endl;

        // Run the command and get the stdout into result
        auto result = RunCmd(command);

        std::cout << "Result : " << result << "\n";

        // Parse the two lines
        auto io_and_v = split(result, '\n');
        auto io_str = io_and_v[0];
        auto v_str = io_and_v[1];

        // Parse each line
        auto io_list = split(io_str, ' ');
        auto v_list = split(v_str, ' ');

        // Convert to integer
        auto io_vec = convert_to_int(io_list);
        auto v_vec = convert_to_int(v_list);

        // send result to master
        worker_response* resp = new worker_response(io_vec);
        master_mailbox->put(resp, 1000000);

        if (!dependents.empty()) {
            // send intermediates to dependents
            for (const auto& dep_name : dependents) {
                simgrid::s4u::MailboxPtr receiver_mailbox = simgrid::s4u::Mailbox::by_name(dep_name);
                worker_request* msg = new worker_request(v_vec);
                receiver_mailbox->put(msg, 1000000);
            }
        }
        XBT_INFO("Exiting now.");
    }
};

// Class containing master code
class Master {
    public:
    explicit Master(std::vector<std::string> args) : worker_count(int(args.size()) - 2)
    {
        xbt_assert(args.size() > 2, "The master function expects 2 arguments plus the workers' names");

        communicate_cost = std::stod(args[1]);
        gen              = std::mt19937(std::chrono::high_resolution_clock::now().time_since_epoch().count());

        for (unsigned int i = 2; i < args.size(); i++) {
            workers.push_back(simgrid::s4u::Mailbox::by_name(args[i]));
            worker_names.push_back(args[i]);
        }

        // Get master mailbox and set receiver to start data flow as soon as it's queued
        my_mailbox = simgrid::s4u::Mailbox::by_name(simgrid::s4u::this_actor::get_host()->get_name());
        my_mailbox->set_receiver(simgrid::s4u::Actor::self());
    }

    void operator()() {
        // Get number of variables
        auto params_filename = program_name + ".params";
        auto params = GetParams(params_filename);
        
        DispatchTasks(params.n_vars);

        // Get responses from workers
        CollectResponses(params);

        XBT_INFO("FINAL RESULT :");
        for (const auto& output : outputs) {
            std::cout << output << " " << std::endl;
        }
    }

    private:

    std::mt19937 gen;
    double communicate_cost;
    const int worker_count;
    std::vector<int> outputs;
    std::vector<std::string> worker_names;
    simgrid::s4u::MailboxPtr my_mailbox;
    std::vector<simgrid::s4u::MailboxPtr> workers;

    void CollectResponses(const comp_params& params) {
        outputs.assign(params.n_outputs, 0);

        for (int i = 0; i < worker_count; i++) {     
            worker_response* resp = static_cast<worker_response*>(my_mailbox->get());
            auto input_output = resp->input_output;

            for (int j = 0; j < params.n_outputs; j++) {
                if (input_output[params.n_inputs + j] != 0) {
                    outputs[j] = input_output[params.n_inputs + j];
                }
            }
            XBT_INFO("Received response..\n");
            delete resp;
        }
    }

    comp_params GetParams(const std::string param_filename) {
        std::ifstream paramFile(param_filename);
        if (!paramFile.is_open()) {
            std::cerr << "ERROR: " << param_filename << " not found." << std::endl;
            exit(1);
        }
        int num_constraints, num_inputs, num_outputs, num_vars;
        std::string comment;
        paramFile >> num_constraints >> comment >> num_inputs >> comment >> num_outputs >> comment >> num_vars;
        paramFile.close();

        return comp_params{num_constraints, num_inputs, num_outputs, num_vars};
    }

    std::vector<std::string> GetPWSCommands() {
        std::vector<std::string> pws_commands;
        std::string command;

        // Open PWS File
        auto pws_filename = program_name + ".pws";
        std::ifstream pws_file;
        pws_file.open(pws_filename);

        while (std::getline(pws_file, command)) {
            pws_commands.push_back(command);
        }

        pws_file.close();

        return pws_commands;
    }

    std::string GetChunkCommands(const std::vector<std::string>& pws_commands, int chunk_size, int worker_num) {
        std::string chunk_commands;
        int start_index = worker_num * chunk_size;

        for (int i = 0; i < chunk_size; i++) {
            int index =  start_index + i; 
            if (index >= pws_commands.size()) {
                return chunk_commands;
            }
            chunk_commands += pws_commands[index];
            chunk_commands += "\n";
        }
        return chunk_commands;
    }

    std::vector<std::string> GetDependents(const Graph& dependency_graph, int chunk_size, int worker_num, int num_lines) {
        int start_line = chunk_size * worker_num;
        int end_line = std::min(chunk_size * (worker_num + 1), num_lines) - 1;

        std::unordered_set<std::string> dependents;
        
        for (int i = start_line; i <= end_line; i++) {
            for (const auto& child_line : dependency_graph[i].dependents) {
                if (child_line / chunk_size != worker_num)
                    dependents.insert(worker_names[child_line / chunk_size]);
            }
        }

        return std::vector<std::string>(dependents.begin(), dependents.end());
    }

    int GetDependencyCount(const Graph& dependency_graph, int chunk_size, int worker_num, int num_lines) {
        int start_line = chunk_size * worker_num;
        int end_line = std::min(chunk_size * (worker_num + 1), num_lines) - 1;

        std::unordered_set<std::string> depends_on;
        
        for (int i = start_line; i <= end_line; i++) {
            for (const auto& child_line : dependency_graph[i].depends_on) {
                if (child_line / chunk_size != worker_num)
                    depends_on.insert(worker_names[child_line / chunk_size]);
            }
        }

        return depends_on.size();
    }

    void DispatchSubtask(const int& worker_num, const std::string& pws_commands, const std::vector<std::string>& dependents, int num_depends_on, const int& num_vars) {
        worker_request* req = new worker_request(num_vars, num_depends_on, pws_commands, dependents);

        simgrid::s4u::MailboxPtr mailbox = workers[worker_num];
        mailbox->put(req, communicate_cost);

        std::string msg = "Sent request to " + worker_names[worker_num] + "\n";
        XBT_INFO(msg.c_str());
    }

    bool DispatchTasks(const int& num_vars) {
        auto pws_commands = GetPWSCommands();
        Graph dependency_graph = generate_dag(pws_commands);
        int num_lines = pws_commands.size();
        // set chunk size to ceil(num_lines / worker_count)
        int chunk_size = (num_lines + worker_count - 1) / worker_count;

        std::cout << " nl : " << num_lines << " csz : " << chunk_size << std::endl;

        for (int worker_num = 0; worker_num < worker_count; worker_num++) {
            auto commands = GetChunkCommands(pws_commands, chunk_size, worker_num);
            auto dependents = GetDependents(dependency_graph, chunk_size, worker_num, pws_commands.size());
            auto dependency_count = GetDependencyCount(dependency_graph, chunk_size, worker_num, pws_commands.size());
            // std::string info = worker_names[worker_num] + " : " + std::to_string(dependency_count);
            // XBT_INFO(info.c_str());
            DispatchSubtask(worker_num, commands, dependents, dependency_count, num_vars);
        }
    }
};


int main(int argc, char* argv[])
{
    simgrid::s4u::Engine e(&argc, argv);
    xbt_assert(argc > 3, "Usage: %s platform_file deployment_file user_program\n", argv[0]);

    program_name = std::string(argv[3]);

    /* Register the classes representing the actors */
    e.register_actor<Master>("master");
    e.register_actor<Worker>("worker");

    /* Load the platform description and then deploy the application */
    e.load_platform(argv[1]);
    e.load_deployment(argv[2]);

    /* Run the simulation */
    e.run();

    XBT_INFO("Simulation is over");

    return 0;
}
