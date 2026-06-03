#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <signal.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <cstring>
#include <vector>
#include "CodeExecutor.h"

string random_string(int length) {
    string chars = "abcdefghijklmnopqrstuvwxyz";
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(0, chars.size() - 1);
    string s = "";
    for (int i = 0; i < length; ++i) s += chars[dist(gen)];
    return s;
}

ExecutionResult CodeExecutor::execute(string code, string language) {
    ExecutionResult result;
    filesystem::create_directories("/home/mihir/codepair/temp_run");
    
    char path_template[] = "/home/mihir/codepair/temp_run/run_XXXXXX";
    char* temp_dir_path = mkdtemp(path_template);
    if (!temp_dir_path) {
        result.stderr_output = "Failed to create temp directory";
        result.exit_code = -1;
        return result;
    }
    
    string temp_dir = string(temp_dir_path);
    string file_name;
    string container_name = "codepair_" + random_string(8);
    vector<string> cmd;
    
    cmd.push_back("docker");
    cmd.push_back("run");
    cmd.push_back("--name");
    cmd.push_back(container_name);
    cmd.push_back("--rm");
    cmd.push_back("--network");
    cmd.push_back("none");
    cmd.push_back("--memory");
    cmd.push_back("128m");
    cmd.push_back("--cpus");
    cmd.push_back("0.5");
    cmd.push_back("-v");
    cmd.push_back(temp_dir + ":/workspace");
    cmd.push_back("-w");
    cmd.push_back("/workspace");
    cmd.push_back("-u");
    cmd.push_back("1000:1000");
    
    if (language == "python") {
        file_name = "main.py";
        cmd.push_back("python:3.10-alpine");
        cmd.push_back("python3");
        cmd.push_back("main.py");
    } else if (language == "javascript") {
        file_name = "main.js";
        cmd.push_back("node:18-alpine");
        cmd.push_back("node");
        cmd.push_back("main.js");
    } else if (language == "cpp") {
        file_name = "main.cpp";
        cmd.clear();
        cmd.push_back("docker");
        cmd.push_back("run");
        cmd.push_back("--name");
        cmd.push_back(container_name);
        cmd.push_back("--rm");
        cmd.push_back("--network");
        cmd.push_back("none");
        cmd.push_back("--memory");
        cmd.push_back("256m");
        cmd.push_back("--cpus");
        cmd.push_back("1.0");
        cmd.push_back("-v");
        cmd.push_back(temp_dir + ":/workspace");
        cmd.push_back("-w");
        cmd.push_back("/workspace");
        cmd.push_back("gcc:latest");
        cmd.push_back("sh");
        cmd.push_back("-c");
        cmd.push_back("g++ -O3 main.cpp -o main && ./main");
    } else {
        result.stderr_output = "Unsupported language: " + language;
        result.exit_code = -1;
        filesystem::remove_all(temp_dir);
        return result;
    }
    
    ofstream file(temp_dir + "/" + file_name);
    file << code;
    file.close();
    
    int out_pipe[2];
    int err_pipe[2];
    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        result.stderr_output = "Failed to create communication pipes";
        result.exit_code = -1;
        filesystem::remove_all(temp_dir);
        return result;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        result.stderr_output = "Failed to fork process";
        result.exit_code = -1;
        filesystem::remove_all(temp_dir);
        return result;
    }
    
    if (pid == 0) {
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[0]);
        close(err_pipe[1]);
        
        vector<char*> args;
        for (auto& s : cmd) {
            args.push_back(const_cast<char*>(s.c_str()));
        }
        args.push_back(nullptr);
        
        execvp(args[0], args.data());
        exit(1);
    } else {
        close(out_pipe[1]);
        close(err_pipe[1]);
        
        fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);
        
        struct pollfd fds[2];
        fds[0].fd = out_pipe[0];
        fds[0].events = POLLIN;
        fds[1].fd = err_pipe[0];
        fds[1].events = POLLIN;
        
        int timeout_ms = 5000;
        int time_elapsed = 0;
        bool child_exited = false;
        
        while (time_elapsed < timeout_ms) {
            int ret = poll(fds, 2, 100);
            if (ret > 0) {
                char buffer[1024];
                if (fds[0].revents & POLLIN) {
                    int bytes = read(out_pipe[0], buffer, sizeof(buffer) - 1);
                    if (bytes > 0) {
                        buffer[bytes] = '\0';
                        result.stdout_output += buffer;
                    }
                }
                if (fds[1].revents & POLLIN) {
                    int bytes = read(err_pipe[0], buffer, sizeof(buffer) - 1);
                    if (bytes > 0) {
                        buffer[bytes] = '\0';
                        result.stderr_output += buffer;
                    }
                }
            }
            
            int status;
            pid_t wait_ret = waitpid(pid, &status, WNOHANG);
            if (wait_ret == pid) {
                child_exited = true;
                if (WIFEXITED(status)) {
                    result.exit_code = WEXITSTATUS(status);
                } else {
                    result.exit_code = -1;
                }
                break;
            }
            
            time_elapsed += 100;
        }
        
        if (!child_exited) {
            result.timed_out = true;
            result.exit_code = -1;
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            
            string kill_cmd = "docker kill " + container_name + " > /dev/null 2>&1";
            system(kill_cmd.c_str());
        } else {
            char buffer[1024];
            int bytes;
            while ((bytes = read(out_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[bytes] = '\0';
                result.stdout_output += buffer;
            }
            while ((bytes = read(err_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[bytes] = '\0';
                result.stderr_output += buffer;
            }
        }
        
        close(out_pipe[0]);
        close(err_pipe[0]);
        filesystem::remove_all(temp_dir);
    }
    
    return result;
}
