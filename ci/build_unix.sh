#!/bin/bash
#
# Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#


# SHA256 functionality is only supported in MySQL 8+. From our
# CI systems, only OSX has this version.

cp ci/*.pem /tmp # Copy SSL certs/keys to a known location
if [ $TRAVIS_OS_NAME == "osx" ]; then
    brew update
    brew install $DATABASE lcov
    cp ci/unix-ci.cnf ~/.my.cnf  # This location is checked by both MySQL and MariaDB
    sudo mkdir -p /var/run/mysqld/
    sudo chmod 777 /var/run/mysqld/
    mysql.server start # Note that running this with sudo fails
    if [ $DATABASE == "mariadb" ]; then
        sudo mysql -u root < ci/root_user_setup.sql
    fi
else
    sudo cp ci/unix-ci.cnf /etc/mysql/conf.d/
    sudo service mysql restart
    wget https://github.com/Kitware/CMake/releases/download/v3.17.0/cmake-3.17.0-Linux-x86_64.sh -q -O cmake-latest.sh
    mkdir -p /tmp/cmake-latest
    bash cmake-latest.sh --prefix=/tmp/cmake-latest --skip-license
    export PATH=/tmp/cmake-latest/bin:$PATH
fi


mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
    $(if [ $USE_VALGRIND ]; then echo -DBOOST_MYSQL_VALGRIND_TESTS=ON; fi) \
    $(if [ $USE_COVERAGE ]; then echo -DBOOST_MYSQL_COVERAGE=ON; fi) \
    $(if [ $HAS_SHA256 ]; then echo -DBOOST_MYSQL_SHA256_TESTS=ON; fi) \
    $CMAKE_OPTIONS \
    .. 
make -j6 CTEST_OUTPUT_ON_FAILURE=1 all test

# Coverage collection
if [ $USE_COVERAGE ]; then
    # Select the gcov tool to use
    if [ "$TRAVIS_OS_NAME" == "osx" ]; then
        GCOV_TOOL="$TRAVIS_BUILD_DIR/ci/clang-gcov-osx.sh"
    elif [ "$TRAVIS_COMPILER" == "clang" ]; then
        GCOV_TOOL="$TRAVIS_BUILD_DIR/ci/clang-gcov-linux.sh"
    else
        GCOV_TOOL=gcov
    fi;
    
    # Generate an adequate coverage.info file to upload. Codecov's
    # default is to compute coverage for tests and examples, too, which
    # is not correct
    lcov --capture --directory . -o coverage.info --gcov-tool "$GCOV_TOOL"
    lcov -o coverage.info --extract coverage.info "**include/boost/mysql/**"
    
    # Upload the results
    curl -s https://codecov.io/bash -o codecov.sh
    bash +x codecov.sh -f coverage.info
fi
