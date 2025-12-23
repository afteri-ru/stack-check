import lit.formats
import os

# Название проекта
config.name = "Stack Overflow Check"

# Каталог с тестами
config.test_source_root = os.path.dirname(__file__)

# Каталог для вывода результатов тестов
config.test_exec_root = os.path.join(config.test_source_root, "temp")

# Формат тестов
config.test_format = lit.formats.ShTest(not lit_config.useValgrind)

# Суффиксы тестовых файлов
config.suffixes = ['.c', '.cpp']

# Исключаем файлы с маской "unit-test*"
config.excludes = ['unit_test.cpp']

# Путь к инструментам LLVM
config.llvm_tools_dir = "/usr/lib/llvm-21/bin"

# Путь к библиотеке плагина
config.plugin_path = "/home/rsashka/SOURCE/afteri/stack-overflow-check/trusted-cpp_clang.so"

# Добавляем пути к инструментам
config.environment['PATH'] = os.path.pathsep.join((
    config.llvm_tools_dir,
    config.environment['PATH']))

# Добавляем определения для препроцессора
config.available_features.add('clang')
