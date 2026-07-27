# mir2x

<a href="https://github.com/etorth/mir2x/actions/workflows/build.yml">
  <img alt="GitHub Actions Build Status"
       src="https://github.com/etorth/mir2x/actions/workflows/build.yml/badge.svg"/>
</a>
<a href="https://scan.coverity.com/projects/etorth-mir2x">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/9270/badge.svg"/>
</a>
<a href="https://gitter.im/mir2x/community?utm_source=share-link&utm_medium=link&utm_campaign=share-link">
  <img alt="Gitter chat"
       src="https://badges.gitter.im/org.png"/>
</a>

mir2x is an experimental project that verifies actor-model based parallelism for MMORPG, it's c/s based with various platforms supported and contains all need components for game players and developers:

  - client
  - server
  - pkgviewer
  - animaker
  - mapeditor

### Prebuilt binaries

Each push to the repository publishes a rolling `latest` GitHub release containing Linux and Windows MinGW UCRT64 install trees:

- [mir2x-linux-latest-build.zip](https://github.com/etorth/mir2x/releases/download/latest/mir2x-linux-latest-build.zip)
- [mir2x-windows-latest-build.zip](https://github.com/etorth/mir2x/releases/download/latest/mir2x-windows-latest-build.zip)

The full release page is at <https://github.com/etorth/mir2x/releases/tag/latest>.

### Notes
- This repo uses C++ coroutine to implement actor model, requires compiler to support c++23.
- This repo uses classic v1.45 mir2 as a reference implementation, you can try the original game:
  - Install [win-xp](https://github.com/etorth/winxp-zh) to host and run the game server/client, tested on real machine or virtualbox machine.
  - Install server/client from [mir2-v1.45](https://github.com/etorth/CBWCQ3).
  - Change screen resolution to 16bit mode to run the game.

### Public Server
- Check the tutorial [here](https://github.com/etorth/mir2x/wiki/Host-your-monoserver-on-Oracle-Cloud) for how to run the ```server``` with Oracle Cloud as a public server.
- You can try the public test server ```192.9.241.118``` by
  ```shell
  client --server-ip=192.9.241.118 # not maintained recently
  ```

YouTube links: [1](https://youtu.be/Yz-bGOkDyEQ) [2](https://youtu.be/jl1LPxe2EAA) [3](https://youtu.be/TtGONA83Mb8)

<https://user-images.githubusercontent.com/1754214/162589720-7dd9453b-55e4-4119-a1ee-c879093cf017.mp4>


An IME for SDL fullscreen mode:

<https://user-images.githubusercontent.com/1754214/213572554-785e826c-226d-43fa-a196-ee4f92112db2.mp4>


### Building from source

mir2x uses Conan 2 for third-party dependencies and CMake 4.2.3 or newer
for the project build. A compiler with C++23 support is required.

Detect a Conan profile once:

```sh
conan profile detect
```

Install the Conan dependency graph:

```sh
conan install . --output-folder=build_conan \
    -s:h build_type=Release -s:h compiler.cppstd=23 \
    -c "tools.cmake:configure_args=['-DCMAKE_POLICY_VERSION_MINIMUM=3.5']" \
    --build=missing
```

The policy setting lets CMake 4 build older upstream packages such as g3log;
it does not change mir2x policies. Configure and build in `build_conan`:

```sh
cmake -S . -B build_conan \
    -DCMAKE_TOOLCHAIN_FILE=build_conan/build/generators/conan_toolchain.cmake
cmake --build build_conan --config Release
```

For Debug builds, replace `Release` with `Debug` in the Conan command and CMake
build command.

The ImGui core, miniaudio and sol2 come from ConanCenter. The ImGui
GLFW/OpenGL3 backends are compiled from the sources exported by the Conan
package; `GLTexture.hpp` and `ImGuiFileDialog.hpp` are project extension
headers in `3rdparty`.

On Linux, install the OpenGL, X11, audio, D-Bus and input development packages
required by GLFW and miniaudio before running Conan. Set
`MIR2X_RES_REPO_PATH` at CMake configure time when packaging client/server
resources from an existing `mir2x_res` checkout.

### First time run
To start the monoserver, find a linux machine to host the server, I tried to host it on ```Oracle Cloud Infrastructure```, it works perfectly with the ```always-free``` plan. Click menu server/launch to start the service before start client:

```sh
cd b_mir2x/install/server
./server --auto-launch
```

Start client, currently you can use default account (id = test, pwd = 123456) to try it:

```sh
cd b_mir2x/install/client
./client --server-ip=localhost --auto-login=test:123456
```

### Packages

mir2x uses a number of open source projects to work properly, and of course itself is open source with a public repository on github, please remind me if I missed anything.

* [SDL3](https://www.libsdl.org/) - A cross-platform development library designed to provide a hardware abstraction layer.
* [FLTK](http://www.fltk.org) - A cross-platform C++ GUI toolkit for UNIX®/Linux® (X11), Microsoft® Windows®, and MacOS® X.
* [asio](http://www.think-async.com/) - A cross-platform C++ library for network and low-level I/O programming.
* [g3log](https://github.com/KjellKod/g3log) - An asynchronous, "crash safe", logger that is easy to use.
* [lua](https://www.lua.org/) - A powerful, efficient, lightweight, embeddable scripting language.
* [sol2](https://github.com/ThePhD/sol2) - A fast, simple C++ and Lua binding.
* [tinyxml2](http://www.grinninglizard.com/tinyxml2/) - A simple, small, efficient, C++ XML parser.
* [utf8-cpp](http://utfcpp.sourceforge.net/) - A simple, portable and lightweigt C++ library for UTF-8 string handling.
* [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) - SQLiteC++ (SQLiteCpp) is a smart and easy to use C++ SQLite3 wrapper.
