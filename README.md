# 🛡️Out-of-memory Protector🛡️

A cross-platform GUI utility designed to monitor the RAM usage of an executable in real-time and instantly terminate it if it goes overboard. Built utilizing system calls and piping C++ std::cout straight into Python. 

This project demonstrates low-level inter-process communication (IPC), cross-language binding via `ctypes`, and OS-specific memory management.

## ⭐Features⭐

- Selectable RAM usage before your executable is killed.
- A graph to show the change of RAM usage.
- An option to show what the program pipes back.
- Add additional arguments to the program.
- Cross-platform GUI and syscalls (WindowsAPI / POSIX).

## 🛠️Installation🛠️

Required:
- Python

That's pretty much it. 🤷

.dll and .so come pre-compiled so you can use it (almost) out-of-box, only the Python libraries are required.

# Linux
```bash
  $ git clone ...
  $ cd OOM_Protector
  $ python -m venv .venv
  $ source .venv/bin/activate
  $ pip install -r requirements.txt
  $ python OOM_Protector.py
```
# Windows !!(currently WIP, does not work as intended)!!
```cmd
  $ git clone ...
  $ cd OOM_Protector
  $ python -m venv .venv
  $ .venv\Scripts\activate
  $ pip install -r requirements.txt
  $ python OOM_Protector.py
```


## 🛠️ Architecture & Tech Stack

- Backend (Core): C++17, POSIX API (unistd.h, sys/sysinfo.h), Windows API (windows.h, psapi.h).
- Frontend (GUI): Python 3, customtkinter, ctkchart.
- Glue Layer: ctypes for loading native dynamic libraries and structural arguments passing.


## 🚧 Known Issues & Future Roadmap (Windows Target)

The Linux implementation is fully stable, while the Windows core is currently in an experimental phase with several planned improvements.

## 🌋 License

This project is licensed under the GNU GPLv3 — feel free to analyze, modify, and distribute the code!
