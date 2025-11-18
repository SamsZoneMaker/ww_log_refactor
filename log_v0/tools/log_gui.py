import tkinter as tk
from tkinter import filedialog, scrolledtext
import datetime
from file_names import file_names

FILE_ID_BITS = 12
LINE_BITS = 12
DATA_LEN_BITS = 8
FILE_ID_SHIFT = LINE_BITS + DATA_LEN_BITS
LINE_SHIFT = DATA_LEN_BITS

def parse_log_to_list(log_stream):
    logs = []
    tokens = log_stream.split()
    i = 0
    while i < len(tokens):
        code = int(tokens[i], 16)
        file_id = (code >> FILE_ID_SHIFT) & ((1 << FILE_ID_BITS) - 1)
        line = (code >> LINE_SHIFT) & ((1 << LINE_BITS) - 1)
        data_len = code & ((1 << DATA_LEN_BITS) - 1)
        file_name = file_names[file_id] if file_id < len(file_names) else "unknown"
        data = []
        for j in range(data_len):
            if i + 1 + j < len(tokens):
                data.append(int(tokens[i + 1 + j], 16))
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        logs.append(f"[{timestamp}] {file_name}:{line}: {data}")
        i += 1 + data_len
    return logs

class LogGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Log Parser GUI")
        self.text_area = scrolledtext.ScrolledText(root, width=80, height=20)
        self.text_area.pack()
        self.load_button = tk.Button(root, text="Load Log File", command=self.load_file)
        self.load_button.pack()

    def load_file(self):
        file_path = filedialog.askopenfilename(filetypes=[("Text files", "*.txt"), ("All files", "*.*")])
        if file_path:
            with open(file_path, 'r') as f:
                log_stream = f.read()
            logs = parse_log_to_list(log_stream)
            self.text_area.delete(1.0, tk.END)
            for log in logs:
                self.text_area.insert(tk.END, log + '\n')

if __name__ == '__main__':
    root = tk.Tk()
    gui = LogGUI(root)
    root.mainloop()