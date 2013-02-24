// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "gtest/gtest.h"
#include "src/status.h"

namespace backup2 {

TEST(StatusTest, StatusCopy) {
  // Test copying status objects.
  Status mystatus(kStatusGenericError, "Foobar Blah");
  Status copied_status = mystatus;
  EXPECT_EQ(mystatus.code(), copied_status.code());
  EXPECT_EQ(mystatus.description(), copied_status.description());
  EXPECT_EQ(mystatus.ToString(), copied_status.ToString());
  EXPECT_FALSE(mystatus.ok());
  EXPECT_FALSE(copied_status.ok());

  Status another_copy(mystatus);
  EXPECT_EQ(mystatus.code(), another_copy.code());
  EXPECT_EQ(mystatus.description(), another_copy.description());
  EXPECT_EQ(mystatus.ToString(), another_copy.ToString());
  EXPECT_FALSE(mystatus.ok());
  EXPECT_FALSE(another_copy.ok());
}

TEST(StatusTest, AssignedCode) {
  Status mystatus(kStatusUnknown, "Binky");
  EXPECT_EQ(kStatusUnknown, mystatus.code());
  EXPECT_EQ("Binky", mystatus.description());
  EXPECT_EQ("Unknown: Binky", mystatus.ToString());
  EXPECT_FALSE(mystatus.ok());
}

TEST(StatusTest, StatusOK) {
  Status mystatus = Status::OK;
  EXPECT_EQ(kStatusOk, mystatus.code());
  EXPECT_EQ("OK", mystatus.description());
  EXPECT_EQ("OK: OK", mystatus.ToString());
  EXPECT_TRUE(mystatus.ok());
}

}  // namespace backup2
