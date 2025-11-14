import os
import sys

os.system("gcc *.h *.c -o main")
if sys.platform == "win32":
    os.system("main test.ys")
else:
    os.system("./main test.ys")
