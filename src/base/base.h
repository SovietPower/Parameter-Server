/**
 * @file base.h
 * @brief 系统无关的通用配置
 */
#pragma once

// --- log
/**
 * @brief 禁用的 log 等级。从低到高分别禁用：none、DEBUG、INFO、WARNING、ERROR (All)
 */
#ifndef VERBOSE
#define VERBOSE 0
#endif

/**
 * @brief FATAL 时是 throw ps::Error 还是直接 terminate。
 * Do not use FATAL and CHECK in destructors
 */
#ifndef LOG_FATAL_THROW
#define LOG_FATAL_THROW 1
#endif

/** @brief 是否使用 glog */
#ifndef USE_GLOG
#define USE_GLOG 0
#endif

/**
 * @brief FATAL 时是否打印栈信息。
 * 不太行先不用
 */
#if false // 没有 abi::xxx、backtrace、backtrace_symbols
#if (defined(__GNUC__) && !defined(__MINGW32__) && !defined(__sun) && !defined(__SVR4) && \
		!(defined __MINGW64__) && !(defined __ANDROID__))
#if (!defined(LOG_STACK_TRACE))
#define LOG_STACK_TRACE 1
#endif
#if (!defined(LOG_STACK_TRACE_SIZE))
#define LOG_STACK_TRACE_SIZE 10
#endif
#endif
#endif

// --- basic

#ifndef DISABLE_COPY_AND_ASSIGN
	#define DISABLE_COPY_AND_ASSIGN(T) \
		T(T const&) = delete; \
		T(T&&) = delete; \
		T& operator=(T const&) = delete; \
		T& operator=(T&&) = delete
#endif
