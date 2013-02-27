// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_COMMON_H_
#define BACKUP2_SRC_COMMON_H_

#include <string.h>

#include "boost/functional/hash.hpp"

#ifdef __GNUC__
  #define MUST_USE_RESULT __attribute__ ((warn_unused_result))
#else
  #define MUST_USE_RESULT
#endif

// Macro to disallow the invocation of copy constructor
// and assignment operator.
#ifdef __GNUC__
  #define DISALLOW_COPY_AND_ASSIGN(T) \
    T& operator=(const T&) = delete; \
    T(const T&) = delete;

  #define DISALLOW_CTOR(T) \
    T() = delete
#else
  // VS2012 does not support delete yet so we go with
  // the old version instead.
  #define DISALLOW_COPY_AND_ASSIGN(T) \
    T(const T&); \
    void operator=(const T&);

  #define DISALLOW_CTOR(T) \
    T()
#endif

// Storage class for 128-bit unsigned integers.
struct Uint128 {
  uint64_t hi;
  uint64_t lo;

  bool operator==(const Uint128& rhs) const {
    return hi == rhs.hi && lo == rhs.lo;
  }

  bool operator!=(const Uint128& rhs) const {
    return !(*this == rhs);
  }

  friend std::size_t hash_value(const Uint128& rhs) {
    std::size_t seed = 0;
    boost::hash_combine(seed, rhs.hi);
    boost::hash_combine(seed, rhs.lo);
    return seed;
  }
};

// Storage class for date/time data.
struct DateTime {
  DateTime() { memset(this, 0, sizeof(DateTime)); }

  uint8_t month;
  uint8_t day;
  uint32_t year;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

#endif  // BACKUP2_SRC_COMMON_H_
