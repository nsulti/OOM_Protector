#include <iostream>
#include <chrono>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
// reference: vmrss = actual ram

#if defined(_WIN32) || defined(__CYGWIN__)
    #define EXPORT_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define EXPORT_API __attribute__((visibility("default")))
#else
    #define EXPORT_API
#endif
// export api is basically how we compile to libs n stuff

// posix version here
#if defined(__linux__) || defined(__APPLE__)
  
#include <unistd.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>
#include <fcntl.h>

extern "C" EXPORT_API int POSIXVersion(char* arguments[], double allowed_percentage) { // get necessary argv's
  struct sysinfo info;
  
  if (sysinfo(&info) != 0) {
    std::cerr << "Failed to get system info!" << std::endl;
    return 1;
  }
  unsigned long long total_ram = ((unsigned long long) info.totalram * info.mem_unit) / 1024; // total ram is in blocks

  int pipe_fds[2];
  if (pipe(pipe_fds) < 0) {
    std::cerr << "Pipeline failed to create!" << std::endl;
    return 1;
  } // create pipelines

  pid_t pid = fork(); // fork the process into two instances
  if (pid < 0) {
    std::cerr << "Fork failed! (does the file exist?)\n" << std::endl;
    return 1;
  }
  
  if (pid == 0) {
    // pipe the pipe (lol) to stdout
    dup2(pipe_fds[1], STDOUT_FILENO);
    
    // closing unnecessary pipes (write)
    close(pipe_fds[0]);
    close(pipe_fds[1]);

    execvp(arguments[0], &arguments[0]); // in child process execute the target file

    std::cerr << "Failed to execute command. (does the file exist?)\n";
  }
  else {
    close(pipe_fds[1]);
    char buffer[512];
    size_t bytes_read;
    fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK); // documentation....basically remove blocking
    while (true) { // we read stream of the child here
      bytes_read = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
      
      if (bytes_read > 0) { // pipe returned something!
        buffer[bytes_read] = '\0';
        std::cout << "[STREAM]: " << buffer << std::endl;
      } else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // pipe is sleeping
      } else { // pipe is closed
        break;
      }

      std::string path = "/proc/" + std::to_string(pid) + "/status"; // reading the state of the process
      std::ifstream status_file(path);
      if (!status_file) {
        std::cerr << "No process spawned!\n" << std::endl;
        return 1;
      }
      
      std::string VmRSS;
      bool foundVmRSS = false;
      while (std::getline(status_file, VmRSS)) {
        if (VmRSS.rfind("VmRSS:", 0) == 0) {
          std::stringstream ss(VmRSS);
          std::string label;
          long value; // looks dumb at first, but we get the VmRSS line which has ram usage in it
          std::string unit;
          ss >> label >> value >> unit;
          double used_percentage = ((double)value / total_ram) * 100.0f;
          if (used_percentage > allowed_percentage) {
            kill(pid, SIGKILL);
            close(pipe_fds[0]);
            waitpid(pid, nullptr, 0);
            return 2;
          }
          std::cout << "\n[MEM_USAGE]: " << value << " kB out of " << total_ram << " kB (" << used_percentage << "% / " << allowed_percentage << "%)" << std::endl;
          foundVmRSS = true;
          break;
        }
      }
      if (!foundVmRSS) {
        std::cerr << "[MONITOR]: Memory tracking ended." << std::endl;
      }

      std::cout.flush();
      usleep(50000); // to not peak one core at 100%
    }
    close(pipe_fds[0]);
    waitpid(pid, nullptr, 0);
    std::cout << "\nDone!" << std::endl;
  }
  std::cout << "\nChild finished!" << std::endl;
  return 0;
}

#elif defined(_WIN32)

#include <windows.h>
#include <psapi.h> 

extern "C" EXPORT_API int WindowsVersion(char* arguments[], double allowed_percentage) {
  STARTUPINFOW start_info;
  PROCESS_INFORMATION proc_info;
  int ret_code = -1;
  ZeroMemory(&start_info, sizeof(start_info));
  start_info.cb = sizeof(start_info);
  ZeroMemory(&proc_info, sizeof(proc_info));
  std::string combined_args = "";
  int i = 0;
  while (arguments[i] != nullptr) {
    std::string arg = arguments[i];

    if (arg.find(' ') != std::string::npos) {
      combined_args += "\"" + arg + "\"";
    } else {
      combined_args += arg;
    }
    i++;
    if (arguments[i] != nullptr) {
      combined_args += " ";
    }
  }
  int required_size = MultiByteToWideChar(CP_ACP, 0, combined_args.c_str(), -1, NULL, 0);
  std::vector<wchar_t> cmd_buffer(required_size);
  MultiByteToWideChar(CP_ACP, 0, combined_args.c_str(), -1, cmd_buffer.data(), required_size);

  if (!CreateProcessW(
        NULL,
        cmd_buffer.data(),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &start_info,
        &proc_info
      )) {
    std::cerr << "Process didn't start! Reason: " << GetLastError() << "." << std::endl;
    return 1;
  }
  while (true) {
    DWORD waiting = WaitForSingleObject(proc_info.hProcess, 100);
    if (waiting == WAIT_OBJECT_0) {
      std::cout << "No memory leak caught. :)" << std::endl;
      ret_code = 0;
      break;
    }
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(proc_info.hProcess, &pmc, sizeof(pmc))) {
      if (pmc.WorkingSetSize / 1024 > allowed_percentage) {
        TerminateProcess(proc_info.hProcess, 1);
        ret_code = 2;
        break;
      }
    }
  }
  if (ret_code == -1) { ret_code = 0; }
  CloseHandle(proc_info.hProcess);
  CloseHandle(proc_info.hThread);
  return ret_code;
}


#endif

