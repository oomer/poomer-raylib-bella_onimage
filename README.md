# poomer-raylib-bella_onimage

c++ prototype GUI interface for [Bella Path Tracer](https://bellarender.com) using raylib/raygui with command line build instructions for MacOS/Win/Linux

<img src="resources/raylibmac.png" alt="Alt text" width="225" height="225">
<img src="resources/raylibubuntu.png" alt="Alt text" width="225" height="225">
<img src="resources/raylibwin.png" alt="Alt text" width="225" height="225">

# Build

```
workdir/
├── bella_engine_sdk/
├── raylib/
├── raygui/
└── poomer-raylib-bella_onimage/
```

Download SDK for your OS and drag bella_engine_sdk into your workdir

- [bella_engine_sdk MacOS](https://downloads.bellarender.com/bella_engine_sdk-24.6.0.dmg)
- [bella_engine_sdk Linux](https://downloads.bellarender.com/bella_engine_sdk-24.6.0.tar.gz)
- [bella_engine_sdk Win](https://downloads.bellarender.com/bella_engine_sdk-24.6.0.zip)


# MacOS/Linux

extra linux dependencies
```
apt install libx11-dev
apt install xorg-dev
```

```
cd workdir
git clone https://github.com/raysan5/raylib.git
cd raylib/src
make
cd ../examples
make
cd ../..
git clone https://github.com/raysan5/raygui.git
git clone https://github.com/oomer/poomer-raylib-bella_onimage.git
cd poomer-raylib-bella_onimage
make
```

# Windows
```
cd workdir
git clone https://github.com/raysan5/raylib.git
cd raylib
mkdir build
cd build
cmake ..
msbuild raylib.sln /p:Configuration=Release 
cd ../..
git clone https://github.com/raysan5/raygui.git
git clone https://github.com/oomer/poomer-raylib-bella_onimage.git
msbuild poomer-raylib-bella_onimage.vcxproj /p:Configuration=release /p:Platform=x64 /p:PlatformToolset=v143
```