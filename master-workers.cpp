#include <simgrid/s4u.hpp>
#include <random>
#include <vector>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>

XBT_LOG_NEW_DEFAULT_CATEGORY(s4u_app_masterworker, "Messages specific for this s4u example");

std::string const program_name = "mm_pure_arith";

std::string RunCmd(std::string cmd) {
    std::string data;
    FILE *stream;
    const int max_buffer = 2048;
    char buffer[max_buffer];
    cmd.append(" 2>&1");

    stream = popen(cmd.c_str(), "r");
    if (stream) {
        while (!feof(stream))
        if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
        pclose(stream);
    }
    return data;
}

std::string RandomString(const std::string::size_type length) {
	using namespace std;
	static string chars = "0123456789"
	                      "abcdefghijklmnopqrstuvwxyz"
	                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	static mt19937 gen{random_device{}()};
	static uniform_int_distribution<string::size_type> dis(0, chars.length() - 1);

	string s(length, '*');

	for (auto &c : s)
		c = chars[dis(gen)];

	return s;
}


struct worker_request {
    std::vector<int> intermediates;
    std::string pws_commands;

    worker_request(std::vector<int> intermediates, std::string pws_commands) : intermediates(intermediates), pws_commands(pws_commands) {}
};

struct worker_response {
    std::vector<int> input_output;
    std::vector<int> intermediates;

    worker_response(std::vector<int> input_output, std::vector<int> intermediates) : input_output(input_output), intermediates(intermediates) {}
};

// Class containing worker code
class Worker {
    simgrid::s4u::MailboxPtr mailbox = nullptr;
    simgrid::s4u::MailboxPtr master_mailbox = nullptr;
    public:

    explicit Worker(std::vector<std::string> args)
    {
        xbt_assert(args.size() == 2, "The worker expects the name of the master host");

        mailbox = simgrid::s4u::Mailbox::by_name(simgrid::s4u::this_actor::get_host()->get_name());
        master_mailbox = simgrid::s4u::Mailbox::by_name(args[1]);
    }

    void operator()()
    {
        worker_request* req = static_cast<worker_request*>(mailbox->get());

        XBT_INFO("Got new task..\n");

        auto intermediates = req->intermediates;
        auto pws_commands = req->pws_commands;

        std::ofstream v_file;
        std::string v_filename = RandomString(32) + ".partial";
        v_file.open(v_filename);
        for (auto &intermediate : intermediates) {
            v_file << intermediate << " ";
        }
        v_file << std::endl;
        v_file.close();

        std::ofstream pws_file;
        std::string pws_filename = RandomString(32) + ".pws";
        pws_file.open(pws_filename);
        pws_file.write(pws_commands.c_str(), pws_commands.size());
        pws_file.close();
        
        std::string command = "./pepper_partial_prover_" + program_name + " " 
            + program_name + ".params "
            + pws_filename
            + program_name + ".inputs "
            + v_filename;

        auto result = RunCmd(command);

        worker_response* resp = new worker_response(std::vector<int>{}, std::vector<int>{}); 
        delete req;

        master_mailbox->put(resp, 1000000);

        XBT_INFO("Exiting now.");
    }
};

// Class containing master code
class Master {
    int matrix_dimension             = 0;
    double communicate_cost          = 0;
    std::vector<simgrid::s4u::MailboxPtr> workers;
    std::mt19937 gen;

    // Helper method to generate a matrix of integers
    std::vector<std::vector<int>> generateMatrix() {
        auto dist = std::uniform_int_distribution<int>(1,100);
        std::vector<std::vector<int>> matrix(matrix_dimension, std::vector<int>(matrix_dimension));
        for (int i = 0; i < matrix_dimension; i++) {
            std::generate(matrix[i].begin(), matrix[i].end(), [&dist, &gen = this->gen]() { return dist(gen); });
        }
        return matrix;
    }

    public:
    explicit Master(std::vector<std::string> args)
    {
        xbt_assert(args.size() > 3, "The master function expects 2 arguments plus the workers' names");

        matrix_dimension = std::stoi(args[1]);
        communicate_cost = std::stod(args[2]);
        gen              = std::mt19937(std::chrono::high_resolution_clock::now().time_since_epoch().count());

        for (unsigned int i = 3; i < args.size(); i++)
            workers.push_back(simgrid::s4u::Mailbox::by_name(args[i]));

        XBT_INFO("Got %zu workers and %ld tasks to process", workers.size(), matrix_dimension * matrix_dimension);
    }

    void operator()()
    {
        // Get master mailbox and set receiver to start data flow as soon as it's queued
        simgrid::s4u::MailboxPtr my_mailbox = simgrid::s4u::Mailbox::by_name(simgrid::s4u::this_actor::get_host()->get_name());
        my_mailbox->set_receiver(simgrid::s4u::Actor::self());

        /* - Select a worker in a round-robin way */
        // int task_num = i * matrix_dimension + j;
        simgrid::s4u::MailboxPtr mailbox = workers[0];

        /* - Send the computation amount to the worker */
        // XBT_INFO("Sending row %d, col %d to mailbox '%s'", i, j, mailbox->get_cname());

        // Open PWS File
        auto pws_filename = program_name + ".pws";
        std::ifstream pws_file;
        pws_file.open(pws_filename);

        std::stringstream pws_commands_buffer;
        std::string pws_commands;
        pws_commands_buffer << pws_file.rdbuf();
        pws_commands = pws_commands_buffer.str();

        // Create worker request
        std::vector<int> intermediates = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        worker_request* req = new worker_request(intermediates, pws_commands);

        mailbox->put(req, communicate_cost);

        XBT_INFO("Send request to worker..\n");

        std::vector<std::vector<int>> product_matrix(matrix_dimension, std::vector<int>(matrix_dimension)); 
        // Get responses from workers
        // for (int i = 0; i < tasks_count; i++) {
            if (my_mailbox->ready()) {
                worker_response* resp = static_cast<worker_response*>(my_mailbox->get());
                auto input_output = resp->input_output;
                auto intermediates = resp->intermediates;
                XBT_INFO("Received response..\n");
                delete resp;
            }
        // }
    }
};


int main(int argc, char* argv[])
{
    simgrid::s4u::Engine e(&argc, argv);
    xbt_assert(argc > 2, "Usage: %s platform_file deployment_file\n", argv[0]);

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
