import os
import customtkinter as ctk
import platform
import ctypes
import ctkchart as ctkc
import time
import sys
import threading
import queue
# abandon all hope ye who enter here
captured_cout = queue.Queue()
# ???? we capture cout here
def capture_cpp_cout():
    read_fd, write_fd = os.pipe()
    original_stdout = os.dup(1)
    if platform.system() == "Windows":
        import msvcrt
        os.dup2(write_fd, 1)
        ctypes.pythonapi.PyRun_SimpleString(b"import sys; sys.stdout.flush()")
    else:
        os.dup2(write_fd, 1)
    def read_loop():
        caught_text = ""
        while True:
            try:
                block = os.read(read_fd, 4096)
                if not block:
                    break
                caught_text += block.decode("utf-8", errors="ignore")
                while "\n" in caught_text:
                    line, caught_text = caught_text.split("\n", 1)
                    captured_cout.put(line)
            except Exception:
                break
    monitoring_cout = threading.Thread(target=read_loop, daemon=True)
    monitoring_cout.start()

lib_dir = os.path.abspath("./lib")
# get the platform
if platform.system() == "Windows":
    os.add_dll_directory(lib_dir)
    dll = ctypes.CDLL(os.path.join(lib_dir, "windows_profiler.dll"))
    monitor = dll.WindowsVersion

elif platform.system() in ["Linux", "Darwin"]:
    so = ctypes.CDLL(os.path.join(lib_dir, "linux_profiler.so"))
    monitor = so.POSIXVersion
else:
    raise OSError(f"Unsupported: {platform.system()}")

class CheckBoxFrame(ctk.CTkFrame):
    def __init__(self, master, title, values):
        super().__init__(master)
        self.grid_columnconfigure(0, weight=1)
        self.title = title # hardcoded since i dont think i'll add additional checkbox frames
        self.checkboxes = [] # alright scrap that i'll add some flexibility if anyone'll use this
        self.values = values

        self.title = ctk.CTkLabel(self, text=self.title, fg_color="gray30", corner_radius=3)
        self.title.grid(row=0, column=0, padx=10, pady=(10,0), sticky="ew")

        for i, value in enumerate(self.values):
            checkbox = ctk.CTkCheckBox(self, text=value)
            checkbox.grid(row=i + 1, column = 0, padx = 10, pady = (10, 0), sticky="w") # yes, taken directly from docs
            self.checkboxes.append(checkbox)

    # check which checkboxes are active
    def find_active(self):
        checked_boxes = []
        for checkbox in self.checkboxes:
            if checkbox.get() == 1:
                checked_boxes.append(checkbox.cget("text"))
        return checked_boxes



class MainWindow(ctk.CTk):
    def __init__(self):
        super().__init__()

        # main window
        self.title("OOM Protector")
        self.geometry("800x400")
        
        # no lol
        self.resizable(False, False)
        # necessary variables to track
        self.file_path = None
        self.max_ram = None
        self.additional_args = None
        self.add_output = False

        self.b_string = None
        self.c_string = None
        self.argv_type = None
        # grid config
        self.grid_columnconfigure(0, weight=0)
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)
        
        self.grid_rowconfigure(8, weight=1)

        self.grid_columnconfigure(0, weight=0)
        self.grid_columnconfigure(1, weight=1)
        #for i in range(8):
        #    self.grid_rowconfigure(i, weight=1)
        # checkboxes
        self.checkbox_frame = CheckBoxFrame(self, "Options", values=["Show output stream"]) #maybe add cpu usage later?
        self.checkbox_frame.grid(row=0, column=0, padx=10, pady=(10,), sticky="nw")
        
        # i would add a class for making custom labels but i dont feel like it
        self.max_ram_text = ctk.CTkLabel(
            self,
            text="Max RAM usage\n (in percentage)",
            fg_color="gray25",
            corner_radius=5,
            width=165,
            wraplength=120
        )
        self.max_ram_text.grid(row=1, column=0, padx=10, pady=(0,0), sticky="nw")
        
        self.enter_max_ram = ctk.CTkTextbox(
            master=self,
            width=165,
            corner_radius=3,
            border_width=2,
            border_color="gray25",
            height=20
        )
        self.enter_max_ram.grid(row=2, column=0, padx=10, pady=(0,10), sticky="nw")

        self.add_ops = ctk.CTkLabel(
            self,
            text="Additional arguments\n (if necessary):",
            fg_color="gray25",
            corner_radius=5,
            width=165,
            wraplength=160
        )
        self.add_ops.grid(row=3, column=0, padx=10, pady=0, sticky="nw")
        self.additional_options = ctk.CTkTextbox(
            master=self,
            width=165,
            corner_radius=3,
            border_width=2,
            border_color="gray25",
            height=20,
            wrap="word"
        )
        self.additional_options.grid(row=4, column=0, padx=10, pady=0, sticky="nw")
        self.select_button = ctk.CTkButton(
            self,
            text="Click to select file",
            fg_color="gray25",
            hover_color="gray30",
            height=10,
            command=self.open_file,
            anchor="center",
            width=165
        )
        self.status_label = ctk.CTkLabel(self, text="Nothing here (yet)", fg_color="gray40", corner_radius = 5, width=165, wraplength=160)
        self.status_label.grid(row=5, column=0, padx=10, pady=(10, 10), sticky="nw") # just a bunch of bullshit (trial and error)

        self.select_button.grid(row=6, column=0, padx=10, pady=(0, 10), sticky="nw")
        
        self.execute_button = ctk.CTkButton(self, text="Execute!", fg_color="gray25", hover_color="gray25", width=165, command=self.correct_c_input)
        self.execute_button.grid(row=7, column=0, padx=10, pady=(0, 10), sticky="nw")
        
        # basically classic programming but in classes...lol
        self.chart = ctkc.CTkLineChart(
            master=self,
            x_axis_values=(tuple(i for i in range(0, 30))),
            y_axis_values=(0, 100),
            fg_color="white",
            bg_color="gray25",
            y_axis_font_color="red",
            x_axis_font_color="red",
            width=600,
            x_axis_section_color="grey20",
            y_axis_section_color="grey20" ,
            y_axis_section_count=10,          # Draws 5 horizontal grid lines matching your labels
            x_axis_section_count=30,
            height=300,
            x_axis_data_position="top", 
            y_axis_data_position="top",
            y_axis_label_count=10,
        )
        
        self.line = ctkc.CTkLine(
            master=self.chart,
            size=2,
            color="green"
        )
        self.line_limit = ctkc.CTkLine(
            master=self.chart,
            size=2,
            color="red"
        )

        self.chart.grid(row=0, column=1, rowspan=7, padx=(10,10), pady=(10, 20), sticky="nsew")
        self.output_stream = ctk.CTkTextbox(
            master=self,
            width=300,
            corner_radius=3,
            border_width=2,
            border_color="gray25",
            height=60,
            wrap="word",
            state="disabled"
        )
        self.output_stream.grid(row=7, column=1, padx=(10, 10), pady=(0, 10), sticky="nsew")
        self.kill_button = ctk.CTkButton(
            self,
            text="Kill the process",
            fg_color="grey25",
            hover_color="grey25",
            state="disabled",
            command=self.kill_child
        )
        self.kill_button.grid(row=7, column=0, padx=(10,10), pady=(0,10), sticky="sew")



    def open_file(self):
        self.file_path = ctk.filedialog.askopenfilename(
            title="Select a file",
            filetypes=[("All Files", "*.*")]
        )
        if self.file_path:
            file_name = os.path.basename(self.file_path)
            self.status_label.configure(
                text=f"Selected: {file_name}",
                text_color="#1dda00"
            )
            self.execute_button.configure(
                fg_color = "#1dda00",
                hover_color = "green",
                state="normal"
            )
        else:
            self.status_label.configure(
                text="No file selected :(",
                text_color="red"
            )
            self.execute_button.configure(
                fg_color = "red", 
                hover_color = "#de2828"
            )
            self.file_path = None

    def correct_c_input(self): # self-explanatory
        additional_args = self.additional_options.get("1.0", "end-1c").strip()
        raw_ram = self.enter_max_ram.get("1.0", "end-1c").strip().replace("%", "") # in case someone does this
        real_ram = None # perhaps there is a better way to do this
        try:
            real_ram = float(raw_ram)
        except:
            self.max_ram_text.configure(
                fg_color = "red",
            )
            self.execute_button.configure(
                fg_color = "red",
                hover_color = "#de2828"
            ) 
        if self.file_path is None:
            self.execute_button.configure(
                fg_color = "red",
                hover_color = "#de2828"
            )
        elif real_ram is None or not raw_ram or real_ram > 100.0:
            self.max_ram_text.configure(
                fg_color = "red"
            )
            self.execute_button.configure(
                fg_color = "red",
                hover_color = "#de2828"
            )
        else:
            self.max_ram_text.configure(
                fg_color = "gray25"
            )

            masked_path = self.file_path.replace(" ", "\u200B")
            c_args = self.string_to_cstring(masked_path + " " + additional_args)
            for idx in range(len(self.b_strings)):
                self.b_strings[idx] = self.b_strings[idx].replace(b"\xe2\x80\x8b", b" ")
            boxes = self.checkbox_frame.find_active()
            if "Show output stream" in boxes:
               self.add_output = True 
            ##monitor(c_args, real_ram)
            self.to_cpp(c_args, real_ram)

    
    def string_to_cstring(self, executable): # harder than expected :/
        split_string = executable.split()
        self.b_strings = [s.encode('utf-8') for s in split_string]
        self.c_strings = [ctypes.c_char_p(b) for b in self.b_strings] + [None] # get the c strings and null term the array
        self.argv_type = ctypes.c_char_p * len(self.c_strings) # get their size
        c_argv = self.argv_type(*self.c_strings) # return the made c strings
        return c_argv 
     
    def add_line_to_stream(self, text): # make this a function for easy access
        self.output_stream.configure(state="normal")
        self.output_stream.insert("end", text + "\n")
        self.output_stream.configure(state="disabled")
        self.output_stream.see("end")
    
    #finally get it to cpp
    def to_cpp(self, argv, real_ram):
        self.status_label.configure(text="Executing!", fg_color="green")
        self.execute_button.configure(state="disabled")
        data_limit = [real_ram for x in range(0,30)]
        
        monitor.argtypes = [self.argv_type, ctypes.c_double]
        def worker():
            self.add_line_to_stream("[DEBUG]: Launching library...")
            status = monitor(argv, real_ram)
            self.add_line_to_stream("[DEBUG]: Monitoring finished.")
            if (status == 0):
                self.status_label.configure(fg_color="gray40", text="No memory leak detected. :)")
            elif (status == 2):
                self.status_label.configure(fg_color="gray40", text="Memory leak detected. :(", text_color="red")
        self.kill_button.configure(state="normal", fg_color="red", hover_color="#de2828")
        self.cpp_thread = threading.Thread(target=worker, daemon=True)
        self.data = [0,0,]
        def listen():
            while True:
                try:
                    caught = captured_cout.get(timeout=1.0)
                    if "[MEM_USAGE]" in caught:
                        self.data.append(float(caught[caught.find("(") + 1:caught.find('%')])) # i dont feel like commenting this
                        if len(self.data) > 29:
                            self.data.pop(0)
                    if (self.add_output): 
                        self.add_line_to_stream(caught)
                    captured_cout.task_done()
                except queue.Empty:
                    continue
        listener = threading.Thread(target=listen, daemon=True)
        listener.start()
        self.cpp_thread.start()
        def update_chart():
            self.chart.show_data(line=self.line_limit, data=data_limit)
            self.chart.show_data(line=self.line, data=self.data)
            if not self.cpp_thread.is_alive():
                self.execute_button.configure(state="normal", fg_color="#1dda00")
                self.kill_button.configure(state="disabled", fg_color="gray25", hover_color="gray25")
                return
            self.after(100, update_chart)
            

        self.after(100, update_chart)
        self.execute_button.configure(fg_color="gray25", hover_color="gray25")

    
    def kill_child(self): # YOU SHOULD KILL YOURSELF **NOW**!
        self.kill_button.configure(state="disabled", fg_color="gray25", hover_color="gray25")
        self.status_label.configure(text="Stopping...", fg_color="orange") 
        child = os.path.basename(self.file_path)
        self.add_line_to_stream(f"[DEBUG]: Terminating all instances of {child}...")
        try:
            if platform.system() == "Windows":
                os.system(f'taskkill /F /IM "{child}"')
            else:
                os.system(f'killall -9 "{child}"')
            self.status_label.configure(text="Done!", fg_color="gray25")
            self.execute_button.configure(state="normal", fg_color="#1dda00")
        except Exception as FUCK:
            self.status_label.configure(text="oh no", fg_color="red")
            self.add_line_to_stream(f"[ERROR]: Failed to close the processes. Code: {FUCK}")

if __name__ == "__main__":
    capture_cpp_cout()
    window = MainWindow()
    window.mainloop()
else:
    print("Unable to be used as a library. (just dont)")
    exit(1)
