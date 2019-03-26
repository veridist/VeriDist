#include <simgrid/s4u.hpp>
#include <random>
#include <vector>
#include <chrono>
#include <algorithm>

XBT_LOG_NEW_DEFAULT_CATEGORY(s4u_app_masterworker, "Messages specific for this s4u example");

struct worker_request {
    int row_num, col_num;
    std::vector<int> row, col;
    bool has_more;

    worker_request(int row_num, int col_num, std::vector<int> row, std::vector<int> col, bool has_more) :
        row_num(row_num), col_num(col_num), row(row), col(col), has_more(has_more) {}
};

struct worker_response {
    int row_num, col_num, value;

    worker_response(int row_num, int col_num, int value) : row_num(row_num), col_num(col_num), value(value) {}
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
        int sum;
        bool has_more;
        do {
            worker_request* req = static_cast<worker_request*>(mailbox->get());
            sum = 0;
            for (int i = 0; i < req->row.size(); i++) {
                sum += req->row[i] * req->col[i];
            }
            has_more = req->has_more;
            worker_response* resp = new worker_response(req->row_num, req->col_num, sum); 
            delete req;

            master_mailbox->put(resp, 1000000);

        } while (has_more == true);

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
        // Generate two matrices
        auto matrix_A = generateMatrix(), matrix_B = generateMatrix();
        int tasks_count = matrix_dimension * matrix_dimension;

        // Get master mailbox and set receiver to start data flow as soon as it's queued
        simgrid::s4u::MailboxPtr my_mailbox = simgrid::s4u::Mailbox::by_name(simgrid::s4u::this_actor::get_host()->get_name());
        my_mailbox->set_receiver(simgrid::s4u::Actor::self());

        for (int i = 0; i < matrix_dimension; i++) {
            for (int j = 0; j < matrix_dimension; j++) {
                /* - Select a worker in a round-robin way */
                int task_num = i * matrix_dimension + j;
                simgrid::s4u::MailboxPtr mailbox = workers[task_num % workers.size()];

                /* - Send the computation amount to the worker */
                XBT_INFO("Sending row %d, col %d to mailbox '%s'", i, j, mailbox->get_cname());

                // Create worker request
                worker_request* req = new worker_request(i, j, matrix_A[i], matrix_B[j], task_num + workers.size() < tasks_count);

                mailbox->put(req, communicate_cost);
            }
        }

        std::vector<std::vector<int>> product_matrix(matrix_dimension, std::vector<int>(matrix_dimension)); 
        // Get responses from workers
        for (int i = 0; i < tasks_count; i++) {
            if (my_mailbox->ready()) {
                worker_response* resp = static_cast<worker_response*>(my_mailbox->get());
                product_matrix[resp->row_num][resp->col_num] = resp->value;
                delete resp;
            }
        }
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
