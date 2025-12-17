#!/usr/bin/env python3
# clangd_config_generator - 简化的clangd配置生成器

import json
import os
import glob
import re

# ====== 配置区域 ======

# 环境：1 = Linux服务器，2 = Windows本地
ENVIRONMENT = 2

# 路径映射配置（只在 ENVIRONMENT = 1 下生效）
PATH_MAPPINGS = {
    '/disk2/desen.li/': 'Z:/',
}

# ====== Function Definition ======

def find_source_files():
    """查找项目中的所有c/c++源文件"""
    print("⚙️  查找源文件中...")

    patterns = [
        '*.c', 'src/*.c', 'src/**/*.c', '**/*.c',
        '*.cpp', 'src/*.cpp', 'src/**/*.cpp', '**/*.cpp',
        '*.cc', 'src/*.cc', 'src/**/*.cc', '**/*.cc'
    ]

    source_files = set()
    for pattern in patterns:
        found_files = glob.glob(pattern, recursive=True)
        source_files.update(found_files)

    # 过滤排除目录
    exclude_dirs = [
        'output/', 'release/', '.git/', '.vscode/', 'build/',
        '__pycache__/', 'docs/', '.gitignore', '.gitattributes'
    ]

    filtered_files = []
    for source_file in source_files:
        should_exclude = any(exclude in source_file for exclude in exclude_dirs)
        if not should_exclude and os.path.exists(source_file):
            filtered_files.append(source_file)

    print(f"✓ 共找到 {len(filtered_files)} 个源文件")
    if filtered_files:
        for i, f in enumerate(sorted(filtered_files)[:5]):
            print(f"    示例 {i+1}: {f}")
        if len(filtered_files) > 5:
            print(f"    ...还有 {len(filtered_files) - 5} 个文件未显示")

    return sorted(filtered_files)


def find_include_directories():
    """查找项目中的所有include目录"""
    print(f"\n⚙️  查找Include目录...")

    include_dirs = set()
    project_root = os.getcwd()

    # 遍历项目目录查找.h文件
    for root, dirs, files in os.walk('.'):
        # 跳过不需要的目录
        dirs[:] = [d for d in dirs if not d.startswith('.') and
                  d not in ['build', 'output', '__pycache__', 'release', '.git']]

        # 如果目录包含.h文件，添加为include目录
        h_files = [f for f in files if f.endswith(('.h', '.hpp'))]
        if h_files:
            # 转换为相对于项目根目录的路径
            rel_path = os.path.relpath(root, project_root)
            # 规范化路径：将 . 转换为当前目录，并统一使用正斜杠
            if rel_path == '.':
                include_dirs.add('.')
            else:
                # 转换为正斜杠格式
                normalized_path = rel_path.replace('\\', '/')
                include_dirs.add(normalized_path)

    # 确保当前目录在列表中
    include_dirs.add('.')

    # 添加常见的include目录（如果存在）
    common_dirs = [
        'include',
        'inc',
        'src',
        'driver',
        'drivers',
        'bsp',
        'middleware',
        'lib',
        'libs',
        'components'
    ]

    for dir_name in common_dirs:
        if os.path.isdir(dir_name):
            # 使用正斜杠格式
            normalized_dir = dir_name.replace('\\', '/')
            include_dirs.add(normalized_dir)

    # 转换为排序后的列表
    include_list = sorted(list(include_dirs))

    print(f"✓ 找到 {len(include_list)} 个Include目录:")
    for i, inc_dir in enumerate(include_list):
        # 统计头文件数量
        try:
            search_pattern = os.path.join(inc_dir, '*.h')
            h_count = len(glob.glob(search_pattern))
            hpp_pattern = os.path.join(inc_dir, '*.hpp')
            hpp_count = len(glob.glob(hpp_pattern))
            total_headers = h_count + hpp_count
            print(f"    {i+1}. {inc_dir} ({total_headers} 个头文件)")
        except:
            print(f"    {i+1}. {inc_dir}")

    return include_list


def detect_defines():
    """检测常用的宏定义"""
    print(f"\n⚙️  检测宏定义...")

    common_defines = [
        'DEBUG=1',
        '_GNU_SOURCE',
    ]

    # 尝试从现有的Makefile中提取定义
    makefile_defines = []

    for makefile in ['Makefile', 'makefile', 'GNUmakefile']:
        if os.path.exists(makefile):
            try:
                with open(makefile, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()

                # 查找-D参数
                defines = re.findall(r'-D\s*([^\s]+)', content)
                makefile_defines.extend(defines)

                if defines:
                    print(f"✓ 从 {makefile} 中找到宏定义: {defines}")

            except Exception as e:
                print(f"✗ 读取 {makefile} 失败: {e}")

    config_defines = parse_config_define()
    if config_defines:
        print(f"✓ 从配置文件中找到宏定义: {config_defines}")

    # 合并宏定义
    all_defines = list(set(common_defines + makefile_defines + config_defines))
    print(f"✓ 使用的宏定义: {all_defines}")

    return all_defines


def normalize_windows_path(path):
    """
    规范化Windows路径格式
    将 /d/WorkSpace 转换为 D:/WorkSpace
    """
    # 先转换为绝对路径
    abs_path = os.path.abspath(path)

    # 在Windows下，处理盘符格式
    if os.name == 'nt':
        # os.path.abspath 在 Git Bash 下可能返回 /d/... 格式
        # 需要转换为标准的 D:/... 格式
        if abs_path.startswith('/') and len(abs_path) > 2 and abs_path[2] == '/':
            # /d/WorkSpace -> D:/WorkSpace
            drive_letter = abs_path[1].upper()
            rest_path = abs_path[2:]
            abs_path = f"{drive_letter}:{rest_path}"

    # 统一使用正斜杠
    normalized = abs_path.replace('\\', '/')

    return normalized


def convert_path(path):
    """根据环境转换路径 - 统一使用正斜杠"""
    # Linux服务器环境下进行路径映射
    if ENVIRONMENT == 1:
        converted_path = path
        for linux_prefix, windows_prefix in PATH_MAPPINGS.items():
            if path.startswith(linux_prefix):
                converted_path = path.replace(linux_prefix, windows_prefix)
                break
        # 统一使用正斜杠
        return converted_path.replace('\\', '/')

    # Windows本地环境下 - 使用绝对路径并转换为正斜杠
    elif ENVIRONMENT == 2:
        return normalize_windows_path(path)

    return path


def create_compile_commands():
    """生成compile_commands.json内容"""
    print(f"\n⚙️  生成 compile_commands.json 内容...")

    source_files = find_source_files()
    if not source_files:
        print("✗ 未找到任何源文件，无法生成 compile_commands.json")
        return False

    include_dirs = find_include_directories()
    defines = detect_defines()

    # 获取项目根目录的绝对路径
    project_dir = os.getcwd()

    # 构建编译标志（使用相对路径）
    include_flags = [f'-I{inc_dir}' for inc_dir in include_dirs]
    define_flags = [f'-D{define}' for define in defines]

    print(f"✓ Include Flags: {len(include_flags)} 个")
    print(f"✓ Define Flags: {len(define_flags)} 个")

    # 生成编译条目
    compile_commands = []

    for source_file in source_files:
        # 路径转换为正斜杠格式的绝对路径
        converted_source_path = convert_path(source_file)
        converted_project_dir = convert_path(project_dir)

        # 构建编译命令（使用相对路径）
        relative_source = os.path.relpath(source_file, project_dir).replace('\\', '/')
        cmd_parts = ['gcc'] + include_flags + define_flags + ['-c', relative_source]
        command = ' '.join(cmd_parts)

        entry = {
            "directory": converted_project_dir,
            "command": command,
            "file": converted_source_path
        }

        compile_commands.append(entry)

    print(f"✓ 生成了 {len(compile_commands)} 条编译命令")

    # 保存文件
    try:
        with open('compile_commands.json', 'w', encoding='utf-8') as f:
            json.dump(compile_commands, f, indent=2, ensure_ascii=False)

        print("✓ compile_commands.json 生成成功！")

        # 显示路径转换示例
        if compile_commands:
            print("\n📂 示例路径转换:")
            sample_entry = compile_commands[0]
            print(f"    原始文件路径: {source_files[0]}")
            print(f"    转换后文件路径: {sample_entry['file']}")
            print(f"    原始目录路径: {project_dir}")
            print(f"    转换后目录路径: {sample_entry['directory']}")
            print(f"\n📋 示例 JSON 条目:")
            print(json.dumps(sample_entry, indent=4, ensure_ascii=False))

        return True

    except Exception as e:
        print(f"✗ 保存 compile_commands.json 失败: {e}")
        return False


def create_clangd_config():
    """生成.clangd配置文件"""
    print(f"\n⚙️  生成 .clangd 配置...")

    include_dirs = find_include_directories()
    defines = detect_defines()

    try:
        with open('.clangd', 'w', encoding='utf-8') as f:
            f.write("# clangd 配置文件，由 clangd_config_generator 生成\n")
            f.write('CompileFlags:\n')
            f.write('  Add:\n')

            # 添加Include目录
            for inc_dir in include_dirs:
                f.write(f"    - -I{inc_dir}\n")
            # 添加宏定义
            for define in defines:
                f.write(f"    - -D{define}\n")

            f.write("  Remove:\n")
            f.write("    - -W*\n")  # 移除所有警告标志
            f.write("    - -std=*\n")  # 移除所有标准标志
            f.write("    - -O*\n")  # 移除所有优化标志
            f.write("\n")
            f.write("Diagnostics:\n")
            f.write("  UnusedIncludes: false\n")  # 关闭未使用的include警告
            f.write("  MissingIncludes: false\n")  # 关闭缺失include警告
            f.write("\n")
            f.write("InlayHints:\n")
            f.write("  Enabled: true\n")  # 启用内联提示
            f.write("  ParameterNames: true\n")  # 启用参数名称提示
            f.write("  VariableTypes: true\n")  # 启用变量类型提示
            f.write("  DeducedTypes: true\n")  # 启用推导类型提示
            f.write("\n")
            f.write("Index:\n")
            f.write("  Background: Build\n")  # 启用后台索引
            f.write("  StandardLibrary: true\n")  # 启用标准库索引

        print("✓ .clangd 配置文件生成成功！")
        return True

    except Exception as e:
        print(f"✗ 保存 .clangd 配置文件失败: {e}")
        return False


def parse_config_define():
    """从config_define.txt文件中解析宏定义"""
    config_defines = []

    # 查找所有配置文件
    config_patterns = ['**/*.conf', '**/*.config', '**/config_define.txt']

    for pattern in config_patterns:
        config_files = glob.glob(pattern, recursive=True)
        for config_file in config_files:
            try:
                with open(config_file, 'r', encoding='utf-8', errors='ignore') as f:
                    for line in f:
                        line = line.strip()

                        # 跳过注释和空行
                        if not line or line.startswith('#'):
                            continue

                        # 解析 KEY=VALUE 格式
                        if '=' in line:
                            key, value = line.split('=', 1)
                            key = key.strip()
                            value = value.strip()

                            if key and value:
                                if value == 'y' or value == '1':
                                    config_defines.append(f"{key}=1")
                                else:
                                    config_defines.append(f"{key}={value}")

            except Exception as e:
                print(f"✗ 读取配置文件 {config_file} 失败: {e}")

    return config_defines


def main():
    """主函数入口"""
    print("=" * 60)
    print("===       clangd_config_generator 开始运行            ===")
    print("=" * 60)

    # 显示当前配置
    env_desc = "Linux服务器" if ENVIRONMENT == 1 else "Windows本地"
    print(f"📌 当前环境: {env_desc}")
    print(f"📌 项目路径: {convert_path(os.getcwd())}")

    if ENVIRONMENT == 1:
        print("📌 路径映射配置:")
        for linux_path, windows_path in PATH_MAPPINGS.items():
            print(f"    {linux_path}  -->  {windows_path}")

    print()

    # 执行生成任务
    tasks = []

    if create_compile_commands():
        tasks.append("✓ compile_commands.json")
    else:
        tasks.append("✗ compile_commands.json")

    if create_clangd_config():
        tasks.append("✓ .clangd")
    else:
        tasks.append("✗ .clangd")

    # 显示结果摘要
    print(f"\n{'=' * 60}")
    print("■  生成结果:")
    for task in tasks:
        print(f"    {task}")

    success_count = sum(1 for task in tasks if "✓" in task)
    print(f"\n✦  完成: {success_count}/{len(tasks)}")

    if success_count >= 2:
        print("\n🎉 配置生成成功！")
        if ENVIRONMENT == 1:
            print("📋 请将以下文件复制到Windows本地项目根目录:")
            print("    - compile_commands.json")
            print("    - .clangd")
            print("\n📌 VSCode使用步骤:")
            print("    1. 安装clangd插件")
            print("    2. 禁用C/C++插件")
            print("    3. 重启VSCode")
            print("    4. 测试功能: F12跳转, Ctrl+Space补全")
        elif ENVIRONMENT == 2:
            print("\n📌 VSCode使用步骤:")
            print("    1. 确保已安装clangd插件")
            print("    2. 禁用C/C++插件（避免冲突）")
            print("    3. 重新加载窗口或重启VSCode")
            print("    4. 测试: F12跳转定义, Ctrl+Space代码补全")
    else:
        print("\n✗ 配置生成失败，请检查项目结构")

    print(f"{'=' * 60}")


if __name__ == "__main__":
    main()
