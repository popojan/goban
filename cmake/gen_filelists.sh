#!/usr/bin/env bash

basedir="../.."
file=Build/cmake/FileList.cmake
src='set(GOBAN_SRC_FILES'
hdr='set(GOBAN_HDR_FILES'
srcdir='${PROJECT_SOURCE_DIR}'
srcpath=src
hdrpath=src

printfiles() {
    # Print headers
    echo ${hdr} >>$file
    find  $srcpath -maxdepth 1 -iname "*.h" -exec echo '    '$srcdir/{} \; 2>/dev/null | sort -f >>$file
    echo -e ')\n' >>$file
    # Print source files
    echo ${src} >>$file
    find  $srcpath -maxdepth 1 -iname "*.cpp" -exec echo '    '$srcdir/{} \; 2>/dev/null | sort -f >>$file
    echo -e ')\n' >>$file
}


pushd $basedir
echo -e "# This file was auto-generated with gen_filelists.sh\n" >$file
printfiles ""

