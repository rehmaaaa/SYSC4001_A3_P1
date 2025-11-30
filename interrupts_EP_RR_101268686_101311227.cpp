#include "interrupts_101268686_101311227.hpp"

#define TIME_QUANTUM 2   // you can change this if your prof used a different quantum

// Pick next process from ready queue (front) and put it on CPU
static void RR_pick_next(std::vector<PCB> &ready_queue,
                         PCB &running,
                         std::vector<PCB> &job_list,
                         unsigned int current_time,
                         std::string &log) {
    if (ready_queue.empty()) return;

    PCB p = ready_queue.front();
    ready_queue.erase(ready_queue.begin());

    if (p.start_time == -1) {
        p.start_time = current_time;
    }

    log += print_exec_status(current_time, p.PID, READY, RUNNING);
    p.state = RUNNING;
    running = p;
    sync_queue(job_list, running);
}

std::tuple<std::string> run_simulation(std::vector<PCB> list_processes) {

    std::string log = print_exec_header();
    unsigned int current_time = 0;

    std::vector<PCB> job_list;
    std::vector<PCB> ready_queue;
    std::vector<PCB> wait_queue;
    std::vector<unsigned int> wait_io_done;

    std::vector<bool> admitted(list_processes.size(), false);

    PCB running;
    idle_CPU(running);
    unsigned int slice_used = 0;

    auto all_done = [&]() {
        if (job_list.empty()) return false;
        return all_process_terminated(job_list);
    };

    while (!all_done()) {

        // 1) New arrivals
        for (std::size_t i = 0; i < list_processes.size(); ++i) {
            PCB &p = list_processes[i];
            if (!admitted[i] && p.arrival_time == current_time) {
                assign_memory(p);
                p.state = READY;
                ready_queue.push_back(p);
                job_list.push_back(p);
                admitted[i] = true;
                log += print_exec_status(current_time, p.PID, NEW, READY);
		FILE* memlog = fopen("memory_log.txt", "a");
        	fprintf(memlog, "TIME=%u USED=0 FREE=0 USABLE=0\n", current_time);
        	fclose(memlog);
             }
        }

        // 2) I/O completions
        for (std::size_t i = 0; i < wait_queue.size(); ) {
            if (wait_io_done[i] == current_time) {
                PCB &p = wait_queue[i];
                p.state = READY;
                log += print_exec_status(current_time, p.PID, WAITING, READY);
                ready_queue.push_back(p);
                sync_queue(job_list, p);
                wait_queue.erase(wait_queue.begin() + i);
                wait_io_done.erase(wait_io_done.begin() + i);
            } else {
                ++i;
            }
        }

        // 3) If CPU idle, pick next process (RR)
        if (running.PID == -1 && !ready_queue.empty()) {
            RR_pick_next(ready_queue, running, job_list, current_time, log);
            slice_used = 0;
        }

        // 4) Execute one time unit on CPU
        if (running.PID != -1) {

            running.remaining_time--;
            slice_used++;

            unsigned int executed = running.processing_time - running.remaining_time;

            bool will_do_io = false;
            if (running.io_freq > 0 &&
                running.io_duration > 0 &&
                (executed % running.io_freq == 0) &&
                running.remaining_time > 0) {
                will_do_io = true;
            }

            if (will_do_io) {
                // RUNNING -> WAITING
                running.state = WAITING;
                log += print_exec_status(current_time + 1, running.PID, RUNNING, WAITING);
                wait_queue.push_back(running);
                wait_io_done.push_back(current_time + 1 + running.io_duration);
                sync_queue(job_list, running);
                idle_CPU(running);
                slice_used = 0;
            }
            else if (running.remaining_time == 0) {
                // RUNNING -> TERMINATED
                running.state = TERMINATED;
                log += print_exec_status(current_time + 1, running.PID, RUNNING, TERMINATED);
                free_memory(running);
                sync_queue(job_list, running);
                idle_CPU(running);
                slice_used = 0;
            }
            else if (slice_used == TIME_QUANTUM) {
                // Quantum expired: RUNNING -> READY (RR preemption)
                running.state = READY;
                log += print_exec_status(current_time + 1, running.PID, RUNNING, READY);
                ready_queue.push_back(running);
                sync_queue(job_list, running);
                idle_CPU(running);
                slice_used = 0;
            }
            else {
                // Still RUNNING, just sync
                sync_queue(job_list, running);
            }
        }

        current_time++;
    }

    log += print_exec_footer();
    return std::make_tuple(log);
}

int main(int argc, char** argv) {

    if (argc != 3) {
        std::cout << "Usage: ./interrupts_EP_RR <input_file> <case_number>\n";
        return -1;
    }

    std::string file_name = argv[1];
    int case_num = std::stoi(argv[2]);

    std::ifstream input_file(file_name);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    std::string line;
    std::vector<PCB> list_process;
    while (std::getline(input_file, line)) {
        if (line.empty()) continue;
        auto tokens = split_delim(line, ", ");
        list_process.push_back(add_process(tokens));
    }
    input_file.close();

    auto [exec] = run_simulation(list_process);

    // OUTPUT FILENAMES:
    std::string exec_name = "output_files/execution_case_" + std::to_string(case_num) + ".txt";
    std::string mem_name  = "output_files/memory_case_"    + std::to_string(case_num) + ".txt";

    // MOVE memory_log.txt → memory_case_X.txt
    rename("memory_log.txt", mem_name.c_str());

    // WRITE execution log
    write_output(exec, exec_name.c_str());

    return 0;
}

