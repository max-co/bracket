#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#include <cinttypes>
#include <cstdint>

#include "boost/dynamic_bitset.hpp"

typedef uint_fast32_t state_t;
#define PRI_STATE PRIuFAST32
#define STATE_MAX UINT32_MAX

typedef boost::dynamic_bitset<> bitset_t;
typedef uint_least64_t runid_t;
#define RUNID_MAX UINT_LEAST64_MAX

#endif
