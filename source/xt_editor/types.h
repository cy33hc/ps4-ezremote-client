#ifndef TYPES_H
#define TYPES_H

#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#ifdef BUILD_WIN32 // MSVC
#define API_EXPORT extern "C" __declspec(dllexport)
#define API_IMPORT extern "C" __declspec(dllimport)
#elif BUILD_LINUX | BUILD_MACOS // GCC or Clang
#define API_EXPORT extern "C" __attribute__((visibility("default")))
#define API_IMPORT
#endif

#define global static

typedef unsigned int uint;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

typedef int8 s8;
typedef int16 s16;
typedef int32 s32;
typedef int64 s64;
typedef bool32 b32;

typedef uint8 u8;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef real32 f32;
typedef real64 f64;

typedef uintptr_t umm;
typedef intptr_t smm;

#define S32Min ((s32)0x80000000)
#define S32Max ((s32)0x7fffffff)
#define U16Max 65535
#define U32Max ((u32)-1)
#define U64Max ((u64)-1)
#define F32Max FLT_MAX
#define F32Min -FLT_MAX

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

#define For(Value) For_e((Value), ArrayCount(Value))
#define For_e(Value, End) For_se((Value), 0, (End))
#define For_se(Value, Start, End) for (int (Value) = (Start); (Value) < (End); ++(Value))

// todo(jax): Should these always be 64-bit?
#define Kilobytes(Value) (((uint64)Value) * 1024LL)
#define Megabytes(Value) (Kilobytes((uint64)Value) * 1024LL)
#define Gigabytes(Value) (Megabytes((uint64)Value) * 1024LL)
#define Terabytes(Value) (Gigabytes((uint64)Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
// todo(jax): swap, min, max ... macros???

#define AlignPow2(Value, Alignment) ((Value + ((Alignment) - 1)) & ~((Alignment) - 1))
#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)

#define Stringize(x) PrimitiveStringize(x)
#define PrimitiveStringize(x) #x

inline b32 IsPow2(u32 Value) {
    return ((Value & ~(Value - 1)) == Value);
}

// ANSI Color Codes
#define BLACK "\33[0;30m"
#define RED "\33[0;31m"
#define GREEN "\33[0;32m"
#define YELLOW "\33[0;33m"
#define BLUE "\33[0;34m"
#define MAGENTA "\33[0;35m"
#define CYAN "\33[0;36m"
#define WHITE "\33[0;37m"
#define LIGHT_GRAY "\33[0;37m"
#define DARK_GRAY "\33[1;30m"
#define BOLD_BLACK "\33[1;30m"
#define BOLD_RED "\33[1;31m"
#define BOLD_GREEN "\33[1;32m"
#define BOLD_YELLOW "\33[1;33m"
#define BOLD_BLUE "\33[1;34m"
#define BOLD_MAGENTA "\33[1;35m"
#define BOLD_CYAN "\33[1;36m"
#define BOLD_WHITE "\33[1;37m"
#define RESET "\33[0m"

// note(jax): Platform-independent way to perform an assertion.
// Flat out writes to zero memory to crash the program.
// todo(jax): Create some sort of assert function that creates a message box 
// like in previous engines I've worked on!
#if ENGINE_SLOW
#define Assert(Expression) if (!(Expression)) { *(int*)0=0; }
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath")

// A structure that encapsulates a non-terminated buffer
struct string {
    u8* Data;
    umm Count;
};

inline u32 SafeTruncateU32(u64 Value) {
	// todo(jax): Defines for min/max values 
	Assert(Value <= 0xFFFFFFFF);
	u32 Result = (u32)Value;
	return Result;
}

inline u16 SafeTruncateU16(u32 Value) {
	// todo(jax): Defines for min/max values 
    Assert(Value <= 0xFFFF);
    u16 Result = (u16)Value;
    return Result;
}

inline u8 SafeTruncateU8(u64 Value) {
    Assert(Value <= 0xFF);
    u8 Result = (u8)Value;
    return Result;
}

inline s32 SafeTruncateS32(s64 Value) {
    if (Value >> 63) {
        b32 IsSafeOperation = !(!(Value >> 32) && 0xffffffff);
        if (!IsSafeOperation) {
            printf("SafeTruncateS32: Performing unsafe truncation on '%lld'\n", Value);
        }
        return (s32)Value;
    } else {
        b32 IsSafeOperation = !((Value >> 32) && 0xffffffff);
        if (!IsSafeOperation) {
            printf("SafeTruncateS32: Performing unsafe truncation on '%lld'\n", Value);
        }
        return (s32)Value;
    }
}

inline s16 SafeTruncateS16(s32 Value) {
    if (Value >> 31) {
        b32 IsSafeOperation = !(!(Value >> 16) && 0xffff);
        if (!IsSafeOperation) {
            printf("SafeTruncateS16: Performing unsafe truncation on '%d'\n", Value);
        }
        return (s16)Value;
    } else {
        b32 IsSafeOperation = !((Value >> 16) && 0xffff);
        if (!IsSafeOperation) {
            printf("SafeTruncateS16: Performing unsafe truncation on '%d'\n", Value);
        }
        return (s16)Value;
    }
}

inline s8 SafeTruncateS8(s16 Value) {
    if (Value >> 15) {
        b32 IsSafeOperation = !(!(Value >> 8) && 0xff);
        if (!IsSafeOperation) {
            printf("SafeTruncateS8: Performing unsafe truncation on '%d'\n", Value);
        }
        return (s8)Value;
    } else {
        b32 IsSafeOperation = !((Value >> 8) && 0xff);
        if (!IsSafeOperation) {
            printf("SafeTruncateS8: Performing unsafe truncation on '%d'\n", Value);
        }
        return (s8)Value;
    }
}

//
// note: Scalar operations
//

// todo(jax): These will eventually go into mathlib

inline real32 Square(real32 A) {
    real32 Result = A*A;

    return Result;
}

inline real32 Lerp(real32 A, real32 t, real32 B){
    real32 Result = (1.0f - t)*A + t*B;

    return Result;
}

#endif