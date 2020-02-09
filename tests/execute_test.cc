/*
 * execute_test.cc
 *
 *  Created on: Feb 8, 2020
 *      Author: tomer
 */

#include "gtest/gtest.h"

#include "log.h"
#include "helpers.h"

using namespace std;
using namespace ael;

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    log::Sink::sink_ = new CoutSink();

    return RUN_ALL_TESTS();
}
