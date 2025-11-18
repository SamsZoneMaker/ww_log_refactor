import os
import re

DIR_ID_BITS = 4
FILE_ID_IN_DIR_BITS = 8

def generate_file_ids(root_dir):
    files = []
    for dirpath, dirnames, filenames in os.walk(root_dir):
        for filename in filenames:
            if filename.endswith('.c') or filename.endswith('.h'):
                files.append(os.path.relpath(os.path.join(dirpath, filename), root_dir).replace('\\', '/'))
    files.sort()

    # Group by dir
    dirs = {}
    for file in files:
        dir_name = os.path.dirname(file) or '.'
        if dir_name not in dirs:
            dirs[dir_name] = []
        dirs[dir_name].append(file)

    dir_list = sorted(dirs.keys())
    dir_ids = {dir: i for i, dir in enumerate(dir_list)}

    with open('include/file_ids.h', 'w') as f:
        f.write('#ifndef FILE_IDS_H\n#define FILE_IDS_H\n\n')
        f.write('typedef enum {\n')
        for dir_name, file_list in dirs.items():
            dir_id = dir_ids[dir_name]
            for i, file in enumerate(file_list):
                file_id = (dir_id << FILE_ID_IN_DIR_BITS) | i
                name = re.sub(r'[^\w]', '_', file.upper())
                f.write(f'    FILE_ID_{name} = {file_id},\n')
        f.write('    FILE_ID_MAX\n} file_id_t;\n\n')
        f.write('extern const char* file_names[FILE_ID_MAX];\n\n#endif\n')

    with open('src/file_ids.c', 'w') as f:
        f.write('#include "file_ids.h"\n\n')
        f.write('const char* file_names[FILE_ID_MAX] = {\n')
        for dir_name in dir_list:
            for file in dirs[dir_name]:
                f.write(f'    "{file}",\n')
        f.write('};\n')

    with open('tools/file_names.py', 'w') as f:
        f.write('file_names = [\n')
        for dir_name in dir_list:
            for file in dirs[dir_name]:
                f.write(f'    "{file}",\n')
        f.write(']\n')

if __name__ == '__main__':
    generate_file_ids('.')
    print("File IDs generated.")