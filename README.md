# mesh.pp
## highlights
### p2p network basics
+ p2p socket library
+ p2p contact list kademlia like algorithm built in
### file system utilities
+ file_loader - everything needed to store persistent state of application
+ vector_loader, map_loader - store big amounts of data in file system
+ transactions - commit/rollback interface for storage related operations

## details on supported/unsupported features/technologies
+ *dependencies?* [boost](https://www.boost.org "boost"), [crypto++](https://www.cryptopp.com/ "crypto++"), [belt.pp](https://github.com/publiqnet/belt.pp "belt.pp") and [a simple cmake utility](https://github.com/PUBLIQNetwork/cmake_utility "the simple title for the simple cmake utility")
+ *portable?* yes! it's a goal. clang, gcc, msvc working.
+ *build system?* cmake. refer to cmake project generator options for msvc, xcode, etc... project generation.
+ *static linking vs dynamic linking?* both supported, refer to BUILD_SHARED_LIBS cmake option
+ *32 bit build?* never tried to fix build errors/warnings
+ *header only?* no.
+ c++11
+ *unicode support?* boost::filesystem::path ensures that unicode paths are well supported and the rest of the code assumes that std::string is utf8 encoded.

## how to build mesh.pp?
```console
user@pc:~$ mkdir projects
user@pc:~$ cd projects
user@pc:~/projects$ git clone https://github.com/PubliqNetwork/mesh.pp
user@pc:~/projects$ cd mesh.pp
user@pc:~/projects/mesh.pp$ git submodule update --init --recursive
user@pc:~/projects/mesh.pp$ cd ..
user@pc:~/projects$ mkdir mesh.pp.build
user@pc:~/projects$ cd mesh.pp.build
user@pc:~/projects/mesh.pp.build$ cmake -DCMAKE_INSTALL_PREFIX=./install ../mesh.pp
user@pc:~/projects/mesh.pp.build$ cmake --build . --target install
```

### git submodules?
yes, we keep up with belt.pp. it's a part of the project, so, if you're a developer contributing to belt.pp too, then
```console
user@pc:~$ cd projects/mesh.pp
user@pc:~/projects/mesh.pp$ cd src/belt.pp
user@pc:~/projects/mesh.pp/src/belt.pp$ git checkout master
user@pc:~/projects/mesh.pp/src/belt.pp$ git pull
```

### more details?
refer to belt.pp readme
