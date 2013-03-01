// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/callback.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace backup2 {

class CallbackTest : public testing::Test {
 public:
  int MyCallback(int a, int b) {
    return a + b;
  }
};

TEST_F(CallbackTest, TestCallbacks) {
  ResultCallback2<int, int, int>* cb =
      NewPermanentCallback(static_cast<CallbackTest*>(this),
                           &CallbackTest::MyCallback);
  EXPECT_EQ(15, cb->Run(10, 5));
  delete cb;
}

}  // namespace backup2
