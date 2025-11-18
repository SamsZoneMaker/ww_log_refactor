from file_names import file_names

FILE_ID_BITS = 12
LINE_BITS = 12
DATA_LEN_BITS = 8

FILE_ID_SHIFT = LINE_BITS + DATA_LEN_BITS
LINE_SHIFT = DATA_LEN_BITS
DATA_LEN_SHIFT = 0

def parse_log(log_stream):
    tokens = [t for t in log_stream.split() if t.startswith('0x')]
    i = 0
    while i < len(tokens):
        code = int(tokens[i], 16)
        file_id = (code >> FILE_ID_SHIFT) & ((1 << FILE_ID_BITS) - 1)
        line = (code >> LINE_SHIFT) & ((1 << LINE_BITS) - 1)
        data_len = code & ((1 << DATA_LEN_BITS) - 1)
        file_name = file_names[file_id] if file_id < len(file_names) and file_names[file_id] else "unknown"
        data = []
        for j in range(data_len):
            if i + 1 + j < len(tokens):
                data.append(int(tokens[i + 1 + j], 16))
        print(f"{file_name}:{line}: {data}")
        i += 1 + data_len

if __name__ == '__main__':
    import sys
    if len(sys.argv) > 1:
        with open(sys.argv[1], 'r') as f:
            log_stream = f.read()
    else:
        log_stream = sys.stdin.read()
    parse_log(log_stream)