// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/status.h"

using testing::StrEq;

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

TEST(StatusTest, StatusOr) {
  StatusOr<int> value = 15;
  EXPECT_TRUE(value.ok());
  EXPECT_EQ(15, value.value());

  StatusOr<int> copied_value = value;
  EXPECT_TRUE(copied_value.ok());
  EXPECT_EQ(15, copied_value.value());

  StatusOr<const char*> string_value("abcdefg");
  StatusOr<const char*> copied_string_value = string_value;
  EXPECT_THAT(copied_string_value.value(), StrEq("abcdefg"));

  StatusOr<int> bad_value = Status::UNKNOWN;
  EXPECT_FALSE(bad_value.ok());
  EXPECT_EQ(kStatusUnknown, bad_value.status().code());
  ASSERT_DEATH(bad_value.value(), ".*StatusOr has error status.*");
}

}  // namespace backup2
