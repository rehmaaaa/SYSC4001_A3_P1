
/**
 * @file interrupts_student1_student2_EP.cpp
 *
 */

#include "interrupts_101268686_101311227.hpp"
#include <cstdio>
// Pick next process (lower PID = higher priority)
void EP_pick_next(std::vector<PCB> &ready_queue,
                  PCB &running,
                  std::vector<PCB> &job_list,
                  unsigned int current_time,
                  std::string &log) {

    if (ready_queue.empty()) return;

    auto best_it = ready_queue.begin();
    for (auto it = ready_queue.begin(); it != ready_queue.end(); ++it) {
        if (it->PID < best_it->PID) {
            best_it = it;
        }
    }

    PCB p = *best_it;
    ready_queue.erase(best_it);

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

    auto all_done = [&](void) {
        if (job_list.empty()) return false;
        return all_process_terminated(job_list);
    };

    while (!all_done()) {

        // 1) new arrival
        for (std::size_t i = 0; i < list_processes.size(); ++i) {
            PCB &p = list_processes[i];
            if (!admitted[i] && p.arrival_time == current_time) {
                assign_memory(p);
                p.state = READY;

		log += print_exec_status(current_time, p.PID, NEW, READY);
		
		// (bonus)
        	FILE* memlog = fopen("memory_log.txt", "a");
        	fprintf(memlog, "TIME=%u USED=0 FREE=0 USABLE=0\n", current_time);
        	fclose(memlog);

        	ready_queue.push_back(p);
        	job_list.push_back(p);
        	admitted[i] = true;
    	     }
	}
                

        // 2) I/O completion
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

        // 3) if CPU idle then pick next with  priority
        if (running.PID == -1 && !ready_queue.empty()) {
            EP_pick_next(ready_queue, running, job_list, current_time, log);
        }

        // 4) Execute once on the running process
        if (running.PID != -1) {

            // take 1 cpu unit
            running.remaining_time--;

            // shows how much CPU the process executed in total
            unsigned int executed = running.processing_time - running.remaining_time;

            bool will_do_io = false;
            if (running.io_freq > 0 &&
                running.io_duration > 0 &&
                (executed % running.io_freq == 0) &&
                running.remaining_time > 0) {
                will_do_io = true;
            }

            if (will_do_io) {
                // RUNNING -> WAITING at end of time unit
                running.state = WAITING;
                log += print_exec_status(current_time + 1, running.PID, RUNNING, WAITING);
                wait_queue.push_back(running);
                wait_io_done.push_back(current_time + 1 + running.io_duration);
                sync_queue(job_list, running);
                idle_CPU(running);
            }
            else if (running.remaining_time == 0) {
                // RUNNING -> TERMINATED at end of time unit
                running.state = TERMINATED;
                log += print_exec_status(current_time + 1, running.PID, RUNNING, TERMINATED);
                free_memory(running);
                sync_queue(job_list, running);
                idle_CPU(running);
            }
            else {
                // Still RUNNING, now sync state
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
        std::cout << "Usage: ./interrupts_EP <input_file> <case_number>\n";
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

