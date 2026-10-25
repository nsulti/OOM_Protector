# 🛡️Out-of-memory Protector🛡️

A small project made in a day, utilizing system calls + piping C++ std::cout straight into Python. And a bit of CustomTKinter. Launches your executable under C++ monitoring it's RAM usage. When goes overboard - instant kill.

## ⭐Features⭐

- Selectable RAM usage before your executable is killed.
- A graph to show the change of RAM usage.
- An option to show what the program pipes back.
- Add additional arguments to the program.
## 🛠️Installation🛠️

Required:
- Python

That's pretty much it. 🤷

.dll and .so come pre-compiled so you can use it (almost) out-of-box, only the Python libraries are required.

```bash
  $ git clone ...
  $ cd OOM_Protector
  $ python -m venv .venv
  $ source .venv/bin/activate
  $ pip install -r requirements.txt
  $ python OOM_Protector.py
```


