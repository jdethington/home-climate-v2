# Workaround: the Arduino core's Network library (folder "Network",
# properties name "Networking") is found by LDF but its headers never
# land on the include path, so WiFi/RPAsyncTCP fail on Network.h.
# Append the library's src dir to the include path explicitly.
Import("env")
import os

framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
env.Append(CPPPATH=[os.path.join(framework_dir, "libraries", "Network", "src")])
