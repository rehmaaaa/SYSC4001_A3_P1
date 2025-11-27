#include <interrupts_student1_student2.hpp>

std::tuple<std::string> run_simulation(std::vector<PCB> list_processes)
{
    std::vector<PCB> ready_queue;
    std::vector<PCB> wait_queue;
    std::vector<PCB> job_list;

    PCB running;
    idle_CPU(running);

    unsigned int current_time = 0;
    unsigned int quantum = 2; // RR quantum

    std::string out = print_exec_header();

    while (true)
    {
        bool any_active = false;
        for (auto &p : job_list)
            if (p.state != TERMINATED) any_active = true;

        if (!any_active && job_list.size() > 0)
            break;

        for (auto &p : list_processes)
        {
            if (p.arrival_time == current_time)
            {
                assign_memory(p);
                p.state = READY;
                ready_queue.push_back(p);
                job_list.push_back(p);
                out += print_exec_status(current_time, p.PID, NEW, READY);
            }
        }

        // I/O queue
        for (auto &p : wait_queue)
        {
            if (p.remaining_time == p.io_duration)
            {
                p.state = READY;
                ready_queue.push_back(p);
                out += print_exec_status(current_time, p.PID, WAITING, READY);
                p.remaining_time = p.processing_time;
            }
            else
                p.remaining_time++;
        }

        // EP — priority sort
        std::sort(ready_queue.begin(), ready_queue.end(),
                  [](const PCB &a, const PCB &b){ return a.PID < b.PID; });

        // RR — rotate for fairness
        if (!ready_queue.empty())
        {
            PCB cur = ready_queue.front();
            ready_queue.erase(ready_queue.begin());

            if (cur.state != RUNNING)
                out += print_exec_status(current_time, cur.PID, READY, RUNNING);

            cur.state = RUNNING;

            unsigned int work = std::min(cur.remaining_time, quantum);
            cur.remaining_time -= work;
            current_time += work;

            if (cur.remaining_time == 0)
            {
                out += print_exec_status(current_time, cur.PID, RUNNING, TERMINATED);
                cur.state = TERMINATED;
                free_memory(cur);
            }
            else
            {
                out += print_exec_status(current_time, cur.PID, RUNNING, READY);
                cur.state = READY;
                ready_queue.push_back(cur);
            }

            continue;
        }

        current_time++;
    }

    out += print_exec_footer();
    return {out};
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: ./interrupts_EP_RR <input_file>\n";
        return 1;
    }

    std::ifstream f(argv[1]);
    std::string line;
    std::vector<PCB> list;

    while (std::getline(f, line))
    {
        auto parts = split_delim(line, ", ");
        list.push_back(add_process(parts));
    }

    auto [exec] = run_simulation(list);
    write_output(exec, "execution.txt");
    return 0;
}

