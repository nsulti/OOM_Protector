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
#include <filesystem>

#ifndef EXPORT_API
    #define EXPORT_API __declspec(dllexport)
#endif
namespace {
    std::wstring Utf8ToWide(const std::string& value) {
        if (value.empty()) return {};
        int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()),
            nullptr, 0
        );
        if (size == 0) return {};
        std::vector<wchar_t> buffer(size);
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), buffer.data(), size) == 0) return {};
        return std::wstring(buffer.data(), size);
    }

    std::string QuoteWindowsArgument(const std::string& arg) {
        if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos) return arg;
        std::string quoted = "\"";
        size_t backslashes = 0;
        for (char c : arg) {
            if (c == '\\') {
                ++backslashes;
            }
            else if (c == '\"') {
                quoted.append(backslashes * 2 + 1, '\\');
                quoted += '\"';
                backslashes = 0;
            }
            else {
                quoted.append(backslashes, '\\');
                quoted += c;
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, '\\'); // honestly, i am crying right now
        quoted += '\"';
        return quoted;
    }
    void DrainChildOutput(HANDLE pipe_read) {
        char buffer[4096];
        while (true) {
            DWORD available = 0;
            if (!PeekNamedPipe(pipe_read, nullptr, 0, nullptr, &available, nullptr) ||
                available == 0) {
                return;
            }
            DWORD bytes_read = 0;
            DWORD amount = std::min<DWORD>(available, sizeof(buffer));
            if (!ReadFile(pipe_read, buffer, amount, &bytes_read, nullptr) ||
                bytes_read == 0) {
                return;
            }
            std::cout.write(buffer, bytes_read);
            std::cout.flush();
        }
    }
}
extern "C" EXPORT_API int WindowsVersion(char* arguments[], double allowed_percentage) {

  MEMORYSTATUSEX mem{};
  mem.dwLength = sizeof(mem);
  if (!GlobalMemoryStatusEx(&mem)) {
      std::cerr << "Failed to get sysmem info. Reason: " << GetLastError() << "." << std::endl;
      return 1;
  }

  std::string combined_args = "";
  for (int i = 0; arguments[i] != nullptr; ++i) {
      if (i != 0) combined_args += ' ';
      combined_args += QuoteWindowsArgument(arguments[i]);
  }

  std::wstring app_name = Utf8ToWide(arguments[0]);
  std::wstring command_line = Utf8ToWide(combined_args);
  if (app_name.empty() || command_line.empty()) {
      std::cerr << "Process not started; invalid UTF-8." << std::endl;
      return 1;
  }
  std::string exec = arguments[0];
  //std::wstring work_dir;
  // separator = exec.find_last_of("/\\");
  //if (separator != std::string::npos) {
  //    size_t length = (separator == 2 && exec.size() > 1 && exec[1] == ':') ? separator + 1 : separator;
  //    work_dir = Utf8ToWide(exec.substr(0, length));
  //}

  HANDLE pipe_read = nullptr;
  HANDLE pipe_write = nullptr;
  SECURITY_ATTRIBUTES security{};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  if (!CreatePipe(&pipe_read, &pipe_write, &security, 0) || !SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0)) {
      std::cerr << "Failed to create child pipe. Reason: " << GetLastError() << "." << std::endl;
      if (pipe_read) CloseHandle(pipe_read);
      if (pipe_write) CloseHandle(pipe_write);
      return 1;
  }
  HANDLE null_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
  if (null_input == INVALID_HANDLE_VALUE) {
      std::cerr << "Failed to open null input. Reason: " << GetLastError() << "." << std::endl;
      CloseHandle(pipe_read);
      CloseHandle(pipe_write);
      return 1;
  }

  STARTUPINFOW start_info{};
  start_info.cb = sizeof(start_info);
  start_info.dwFlags = SsTARTF_USESTDHANDLES;
  start_info.hStdInput = null_input;
  start_info.hStdOutput = pipe_write;
  start_info.hStdError = pipe_write;
  PROCESS_INFORMATION proc_info;

  std::vector<wchar_t> cmd_buffer(command_line.begin(), command_line.end());
  cmd_buffer.push_back(L'\0');
  BOOL started = CreateProcessW(
      app_name.c_str(), cmd_buffer.data(), nullptr, nullptr, TRUE, 0, nullptr,
      nullptr, &start_info, &proc_info);// work_dir.empty() ? nullptr : work_dir.c_str()

  DWORD launch_error = started ? ERROR_SUCCESS : GetLastError();
  CloseHandle(null_input);
  CloseHandle(pipe_write); // child owns this

  if (!started) {
      std::cerr << "Process didn't start! Reason: " << launch_error << "." << std::endl;
      CloseHandle(pipe_read);
      return 1;
  }
  CloseHandle(proc_info.hThread);

  int ret_code = 0;
  double total_ram_kb = static_cast<double>(mem.ullTotalPhys) / 1024.0;
  while (true) {
      DrainChildOutput(pipe_read);

      DWORD waiting = WaitForSingleObject(proc_info.hProcess, 50);
      if (waiting == WAIT_OBJECT_0) {
          DrainChildOutput(pipe_read);
          DWORD exit_code = 0;
          if (!GetExitCodeProcess(proc_info.hProcess, &exit_code)) {
              std::cerr << "Could not read the child exit code. Reason: " << GetLastError() << "." << std::endl;
              ret_code = 1;
          }
          else {
              std::cout << "[CHILD_EXIT]: " << exit_code << std::endl;
              if (exit_code != 0) {
                  std::cerr << "The child process exited with an error; this is not a successful leak check." << std::endl;
                  ret_code = 3;
              }
          }
          break;
      }
      if (waiting == WAIT_FAILED) {
          std::cerr << "Failed while waiting for the child. Reason: " << GetLastError() << "." << std::endl;
          ret_code = 1;
          break;
      }

      PROCESS_MEMORY_COUNTERS pmc{};
      if (GetProcessMemoryInfo(proc_info.hProcess, &pmc, sizeof(pmc))) {
          double mem_used = (static_cast<double>(pmc.WorkingSetSize) / 1024.0) /
              total_ram_kb * 100.0;
          if (mem_used > allowed_percentage) {
              TerminateProcess(proc_info.hProcess, 1);
              WaitForSingleObject(proc_info.hProcess, 5000);
              std::cerr << "Memory leak detected. :(" << std::endl;
              ret_code = 2;
              break;
          }
          std::cout << "[MEM_USAGE]: " << pmc.WorkingSetSize / 1024.0
              << " kB out of " << total_ram_kb << " kB (" << mem_used
              << "% / " << allowed_percentage << "%)" << std::endl;
          std::cout.flush();
      }
  }

  if (ret_code == 0) {
      std::cout << "No memory leak detected. :)" << std::endl;
  }
  DrainChildOutput(pipe_read);
  CloseHandle(pipe_read);
  CloseHandle(proc_info.hProcess);

  return ret_code;
}


#endif

