"""
DownloadFastNoiseLite.py
Letölti a FastNoiseLite.h fájlt a ThirdParty/FastNoise mappába.

Futtatás:
  python "C:/Users/darva/Documents/Unreal Projects/Priordium/Source/ThirdParty/FastNoise/DownloadFastNoiseLite.py"
"""
import urllib.request, os

url = "https://raw.githubusercontent.com/Auburn/FastNoiseLite/master/Cpp/FastNoiseLite.h"
dest = os.path.join(os.path.dirname(__file__), "FastNoiseLite.h")

print(f"Downloading FastNoiseLite.h to: {dest}")
urllib.request.urlretrieve(url, dest)
print(f"Done! File size: {os.path.getsize(dest)} bytes")
