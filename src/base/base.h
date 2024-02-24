/**
 * @file base.h
 * @brief defines configuration macros
 * @date 2024-02-21
 */
#pragma once

// --- log
/**
 * @brief 禁用的 log 等级。从低到高分别禁用：none、DEBUG、INFO、WARNING、ERROR (All)
 */
#ifndef VERBOSE
#define VERBOSE 1
#endif

/**
 * @brief whether to throw ps::Error instead of directly calling abort when FATAL error occurred.
 * Do not use FATAL and CHECK in destructors
 */
#ifndef LOG_FATAL_THROW
#define LOG_FATAL_THROW 1
#endif

/** @brief whether to use glog for logging */
#ifndef USE_GLOG
#define USE_GLOG 0
#endif

/**
 * @brief Whether to print stack trace for fatal error,
 * enabled on linux when using gcc.
 */
#if (defined(__GNUC__) && !defined(__MINGW32__) && !defined(__sun) && !defined(__SVR4) && \
		!(defined __MINGW64__) && !(defined __ANDROID__))
#if (!defined(LOG_STACK_TRACE))
#define LOG_STACK_TRACE 1
#endif
#if (!defined(LOG_STACK_TRACE_SIZE))
#define LOG_STACK_TRACE_SIZE 10
#endif
#endif

// --- basic

// Disable copy constructor and assignment operator.
#ifndef DISABLE_COPY_AND_ASSIGN
	#define DISABLE_COPY_AND_ASSIGN(T) \
		T(T const&) = delete; \
		T(T&&) = delete; \
		T& operator=(T const&) = delete; \
		T& operator=(T&&) = delete
#endif
