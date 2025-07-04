#pragma once

// Cross-platform function name macro
#ifdef _MSC_VER
    // Microsoft Visual C++ Compiler
    #define FUNCTION_NAME __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
    // GCC or Clang
    #define FUNCTION_NAME __PRETTY_FUNCTION__
#else
    // Fallback for other compilers
    #define FUNCTION_NAME __func__
#endif
