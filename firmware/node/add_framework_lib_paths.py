# Workaround: this platform+core combination fails to put framework
# library headers on the include path when one framework library
# includes another (first Network.h, then WiFi.h from HTTPUpdate).
# Append every framework library's src dir explicitly — the same
# approach the Arduino IDE takes.
Import("env")
import os

framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
libs_dir = os.path.join(framework_dir, "libraries")
paths = []
if os.path.isdir(libs_dir):
    for name in os.listdir(libs_dir):
        src = os.path.join(libs_dir, name, "src")
        if os.path.isdir(src):
            paths.append(src)
env.Append(CPPPATH=paths)
