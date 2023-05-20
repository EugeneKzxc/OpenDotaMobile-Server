# CMakeList.txt: проект CMake для ServerPi; включите исходный код и определения,
# укажите здесь логику для конкретного проекта.
#

include_directories("/usr/local/include/mongocxx/v_noabi")
include_directories("/usr/local/include/bsoncxx/v_noabi")

file(GLOB ServerPi_src	"*.h"  "*.cpp")
add_executable (ServerPi ${ServerPi_src})

find_package(jsoncons REQUIRED)

set_property(TARGET ServerPi PROPERTY CXX_STANDARD 20)

target_link_libraries(ServerPi PRIVATE bsoncxx)
target_link_libraries(ServerPi PRIVATE mongocxx)
target_link_libraries(ServerPi PRIVATE pthread)
target_link_libraries(ServerPi PRIVATE served)
