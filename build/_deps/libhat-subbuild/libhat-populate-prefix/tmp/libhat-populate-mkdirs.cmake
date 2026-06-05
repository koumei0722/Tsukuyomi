# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/MCBEMOD/Tsukuyomi/build/_deps/libhat-src"
  "C:/MCBEMOD/Tsukuyomi/build/_deps/libhat-build"
  "C:/MCBEMOD/Tsukuyomi/build/_deps/libhat-subbuild/libhat-populate-prefix"
  "C:/MCBEMOD/Tsukuyomi/build/_deps/libhat-subbuild/libhat-populate-prefix/tmp"
  "C:/MCBEMOD/Tsukuyomi/build/_deps/libhat-subbuild/libhat-populate-prefix/src/libhat-populate-stamp"
  "C:/MCBEMOD/Tsukuyomi/build/_deps/libhat-subbuild/libhat-populate-prefix/src"
  "C:/MCBEMOD/Tsukuyomi/build/_deps/libhat-subbuild/libhat-populate-prefix/src/libhat-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/MCBEMOD/Tsukuyomi/build/_deps/libhat-subbuild/libhat-populate-prefix/src/libhat-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/MCBEMOD/Tsukuyomi/build/_deps/libhat-subbuild/libhat-populate-prefix/src/libhat-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
