// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include <string>
#include <vector>

#include "src/common.h"
#include "src/md5_generator.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

using std::string;

namespace backup2 {

TEST(Md5GeneratorTest, MD5Tests) {
  // Various tests to verify the MD5 checksumming algorithm is working
  // correctly.
  Md5Generator generator;

  Uint128 expected;
  expected.hi = 0xd41d8cd98f00b204;
  expected.lo = 0xe9800998ecf8427e;
  EXPECT_EQ(expected, generator.Checksum(""));

  expected.hi = 0x7215ee9c7d9dc229;
  expected.lo = 0xd2921a40e899ec5f;
  EXPECT_EQ(expected, generator.Checksum(" "));

  expected.hi = 0x41884e32dd651882;
  expected.lo = 0x32ce22cde06a153d;
  EXPECT_EQ(expected, generator.Checksum("Testing 123"));

  expected.hi = 0x2f947c90acede2e3;
  expected.lo = 0x610a36bd693728dd;
  EXPECT_EQ(expected, generator.Checksum(
      "skl;dfjoivj;wklefjoidsfl;kjweorijfjkwoiweopijfsoidfl;ksdjf[owierkjfpo"));
}

}  // namespace backup2
