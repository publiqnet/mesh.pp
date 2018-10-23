# mesh.pp
## highlights
### p2p network basics
+ p2p socket library
+ kademlia implementation built in
### file system utilities
+ file_loader - everything needed to store persistent state of application
+ vector_loader, map_loader - store big amounts of data in file system
+ transactions - commit/rollback interface for storage related operations

## details on supported/unsupported features/technologies
+ *dependencies?* [boost](https://www.boost.org "boost"), [crypto++](https://www.cryptopp.com/ "crypto++"), [belt.pp](https://github.com/PubliqNetwork/belt.pp "belt.pp") and [a simple cmake utility](https://github.com/PUBLIQNetwork/cmake_utility "the simple title for the simple cmake utility")
+ *portable?* yes! it's a goal. clang, gcc, msvc working.
+ *build system?* cmake. refer to cmake project generator options for msvc, xcode, etc... project generation.
+ *static linking vs dynamic linking?* both supported, refer to BUILD_SHARED_LIBS cmake option
+ *32 bit build?* never tried to fix build errors/warnings
+ *header only?* no.
+ c++11
+ *unicode support?* boost::filesystem::path ensures that unicode paths are well supported and the rest of the code assumes that std::string is utf8 encoded.

## how to build mesh.pp?
```
cd to_somewhere_new
git clone https://github.com/PubliqNetwork/mesh.pp
cd mesh.pp
git submodule update --init --recursive
cd ..
mkdir mesh.pp.build
cd mesh.pp.build
cmake -DCMAKE_INSTALL_PREFIX=./install ../mesh.pp
cmake --build . --target install
```

### git submodules?
yes, we keep up with belt.pp. it's a part of the project, so, if you're a developer contributing to belt.pp too, then
```
cd mesh.pp
cd src/belt.pp
git checkout master
git pull
```

### more details?
refer to belt.pp readme
