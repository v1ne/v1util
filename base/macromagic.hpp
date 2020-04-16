#pragma once

#define V1_PP_CAT2(res) res
#define V1_PP_CAT1(a, b) V1_PP_CAT2(a##b)
//! Concatenate two elements
#define V1_PP_CAT(a, b) V1_PP_CAT1(a, b)

#define V1_PP_STR1(x) #x
//! Turn an expression into a string
#define V1_PP_STR(x) V1_PP_STR1(x)

//! Generate a unique name from a base name
#define V1_PP_UNQIUE_NAME(base) V1_PP_CAT(base, __COUNTER__)

//! logical implication A => B
#define V1_IMPLIES(A, B) (!(A) || (B))

#ifndef V1_UNUSED
//! Suppress warnings, often used in conjunction with V1_ASSERT
#  define V1_UNUSED(x) (void)x
#endif
