include("C:/MCBEMOD/Tsukuyomi/cmake/CPM.cmake")
CPMAddPackage("GITHUB_REPOSITORY;nlohmann/json;VERSION;3.11.3;EXCLUDE_FROM_ALL;YES;SYSTEM;YES;")
set(json_FOUND TRUE)