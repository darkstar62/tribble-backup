// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/callback.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace backup2 {

class CallbackTest : public testing::Test {
 public:
  int MyCallback1(int a) {
    return a * 3;
  }

  int MyCallback2(int a, int b) {
    return a + b;
  }
};

TEST_F(CallbackTest, TestCallbacks) {
  ResultCallback1<int, int>* cb1 =
      NewPermanentCallback(static_cast<CallbackTest*>(this),
                           &CallbackTest::MyCallback1);
  EXPECT_EQ(30, cb1->Run(10));
  delete cb1;

  ResultCallback2<int, int, int>* cb2 =
      NewPermanentCallback(static_cast<CallbackTest*>(this),
                           &CallbackTest::MyCallback2);
  EXPECT_EQ(15, cb2->Run(10, 5));
  delete cb2;
}

}  // namespace backup2
