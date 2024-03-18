/**
 * @file log.h
 */
/**
可用的宏：
CHECK、CHECK_LT 等
DCHECK、DCHECK_LT 等

LOG(DEBUG)、LOG(INFO)、LOG(WARNING)、LOG(ERROR)、LOG(FATAL)、LOG(DFATAL) (only fatal in debug)
DLOG(DEBUG) 等

LOG_IF(DEBUG, cond) 等
DLOG_IF(DEBUG, cond) 等

可用异常：ps_log::PSError
CHECK 失败、调用 LOG(FATAL) 会抛出异常或终止程序
*/
/*!
	TODO 实现 log 时，添加一个条件输出到特定文件的宏避免多进/线程的输出混乱
	在 base/log 里面加个全局变量来控制输出到的文件名，要在 main 中去初始化。
 */
#pragma once

#include <string>
#include <stdexcept>

#include "./Base.h"

#if USE_GLOG
#include <glog/logging.h>

namespace ps_log {
inline void InitLogging(const char* argv0) {
	google::InitGoogleLogging(argv0 ? argv0 : "my-ps");
}
}	// namespace ps_log

#else
// use a light version of glog
#include <ctime>
#include <chrono>
#include <iomanip> // put_time
#include <fstream>
#include <sstream>
#include <iostream>

namespace ps_log { // 不放在 namespace ps 下。注意宏不受 namespace 影响

// 注意该设置需要在各引用 Log.h 的模块间共享，不要用 static
inline bool use_log_ofstream = false;
inline std::ofstream log_ofstream {};

inline void InitLogging(const char* log_filename) {
	if (log_filename) {
		use_log_ofstream = true;
		log_ofstream = std::ofstream(log_filename);
	}
}

enum E_LOG_SEVERITY {
	E_DEBUG,
	E_INFO,
	E_WARNING,
	E_ERROR,
	E_FATAL,
};

// --- Always-on checking
#define CHECK_FAILED(x, y, cmp) \
	ps_log::LogMessageFatal(__FILE__, __LINE__).stream() << "Check failed: " \
		#x << ' ' << cmp << ' ' << #y << "\n\twhere x is: " << (x) << "\n\tand   y is: " << (y) << '\n'

#define CHECK(x) \
	if (!(x)) \
		ps_log::LogMessageFatal(__FILE__, __LINE__).stream() << "Check true failed: " \
			#x << "\n\twhere x is: " << (x) << '\n'
#define CHECK_LT(x, y) \
	if (!((x) < (y))) CHECK_FAILED(x, y, "<")
#define CHECK_GT(x, y) \
	if (!((x) > (y))) CHECK_FAILED(x, y, ">")
#define CHECK_LE(x, y) \
	if (!((x) <= (y))) CHECK_FAILED(x, y, "<=")
#define CHECK_GE(x, y) \
	if (!((x) >= (y))) CHECK_FAILED(x, y, ">=")
#define CHECK_EQ(x, y) \
	if (!((x) == (y))) CHECK_FAILED(x, y, "==")
#define CHECK_NE(x, y) \
	if (!((x) != (y))) CHECK_FAILED(x, y, "!=")
#define CHECK_NOTNULL(x) \
	((x) == nullptr? ps_log::LogMessageFatal(__FILE__, __LINE__).stream() << "Check notnull: " #x << ' ', (x) : (x))

// --- Debug-only checking
#ifdef NDEBUG
#define DCHECK(x) \
	while (false) CHECK(x)
#define DCHECK_LT(x, y) \
	while (false) CHECK((x) < (y))
#define DCHECK_GT(x, y) \
	while (false) CHECK((x) > (y))
#define DCHECK_LE(x, y) \
	while (false) CHECK((x) <= (y))
#define DCHECK_GE(x, y) \
	while (false) CHECK((x) >= (y))
#define DCHECK_EQ(x, y) \
	while (false) CHECK((x) == (y))
#define DCHECK_NE(x, y) \
	while (false) CHECK((x) != (y))
#define DCHECK_NOTNULL(x) \
	while(false) CHECK_NOTNULL(x)
#else
#define DCHECK(x) CHECK(x)
#define DCHECK_LT(x, y) CHECK((x) < (y))
#define DCHECK_GT(x, y) CHECK((x) > (y))
#define DCHECK_LE(x, y) CHECK((x) <= (y))
#define DCHECK_GE(x, y) CHECK((x) >= (y))
#define DCHECK_EQ(x, y) CHECK((x) == (y))
#define DCHECK_NE(x, y) CHECK((x) != (y))
#define DCHECK_NOTNULL(x) CHECK_NOTNULL(x)
#endif	// NDEBUG

// --- Always-on log (but depends on macro VERBOSE)
#if VERBOSE > 0
#define _CK_DEBUG while (false)
#else
#define _CK_DEBUG
#endif
#if VERBOSE > 1
#define _CK_INFO while (false)
#else
#define _CK_INFO
#endif
#if VERBOSE > 2
#define _CK_WARNING while (false)
#else
#define _CK_WARNING
#endif
#if VERBOSE > 3
#define _CK_ERROR while (false)
#else
#define _CK_ERROR
#endif

#define LOG_DEBUG _CK_DEBUG ps_log::LogMessage<ps_log::E_DEBUG>(__FILE__, __LINE__)
#define LOG_INFO _CK_INFO ps_log::LogMessage<ps_log::E_INFO>(__FILE__, __LINE__)
#define LOG_WARNING _CK_WARNING ps_log::LogMessage<ps_log::E_WARNING>(__FILE__, __LINE__)
#define LOG_ERROR _CK_ERROR ps_log::LogMessage<ps_log::E_ERROR>(__FILE__, __LINE__)
#define LOG_FATAL ps_log::LogMessageFatal(__FILE__, __LINE__)

#define LGI LOG_INFO.stream()
#define LOG(severity) LOG_##severity.stream()
#define LOG_IF(severity, condition) \
	!(condition) ? (void)0 : ps_log::LogMessageVoidify() & LOG(severity)

#ifdef NDEBUG
#define LOG_DFATAL LOG_ERROR
#define DFATAL ERROR
#else
#define LOG_DFATAL LOG_FATAL // LOG_FATAL if in DEBUG
#define DFATAL FATAL // FATAL if in DEBUG
#endif

// --- Debug-only log
#ifdef NDEBUG
#define DLOG(severity) true ? (void)0 : ps_log::LogMessageVoidify() & LOG(severity)
#define DLOG_IF(severity, condition) \
	(true || !(condition)) ? (void)0 : ps_log::LogMessageVoidify() & LOG(severity)
#else
#define DLOG(severity) LOG(severity)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#endif

// #define VLOG(n) LOG(INFO).stream() // same as current LOG
// #define LOG_EVERY_N(severity, n) LOG(severity)

// --- log implementation
// 关闭 localtime 的警告
#if defined(_MSC_VER)
#pragma warning(disable: 4996) // _CRT_SECURE_NO_WARNINGS
#endif

// 想取地址必须有左值，只能单独定义一下
#define set_time() \
	std::time_t tm_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
// 日期格式设置
#define get_time() \
	std::put_time(std::localtime(&tm_now), "%m-%d %T")

template <E_LOG_SEVERITY severity>
class LogMessage {
 public:
	LogMessage(const char* file, int line): log_stream_(use_log_ofstream ? log_ofstream : std::cerr)
	{
		if constexpr (severity == E_DEBUG) {
			log_stream_ << "[DEBUG] ";
		} else if constexpr (severity == E_INFO) {
			log_stream_ << "[INFO] ";
		} else if constexpr (severity == E_WARNING) {
			log_stream_ << "[WARNING] ";
		} else if constexpr (severity == E_ERROR) {
			log_stream_ << "[ERROR] ";
		}
		set_time();
		log_stream_ << "[" << get_time() << "] "
					<< file << ":" << line << ": ";
	}
	~LogMessage() {
		log_stream_ << "\n";
	}
	std::ostream& stream() { return log_stream_; }

 protected:
	std::ostream& log_stream_;

	DISABLE_COPY_AND_ASSIGN(LogMessage);
};

#if LOG_STACK_TRACE
inline std::string Demangle(char const *msg_str) {
	using std::string;
	string msg(msg_str);
	size_t symbol_start = string::npos;
	size_t symbol_end = string::npos;
	if (((symbol_start = msg.find("_Z")) != string::npos) &&
			(symbol_end = msg.find_first_of(" +", symbol_start))) {
		string left_of_symbol(msg, 0, symbol_start);
		string symbol(msg, symbol_start, symbol_end - symbol_start);
		string right_of_symbol(msg, symbol_end);

		int status = 0;
		size_t length = string::npos;
		std::unique_ptr<char, decltype(&std::free)> demangled_symbol = {
				abi::__cxa_demangle(symbol.c_str(), 0, &length, &status), &std::free};
		if (demangled_symbol && status == 0 && length > 0) {
			string symbol_str(demangled_symbol.get());
			std::ostringstream os;
			os << left_of_symbol << symbol_str << right_of_symbol;
			return os.str();
		}
	}
	return string(msg_str);
}

inline std::string StackTrace() {
	using std::string;
	std::ostringstream stacktrace_os;
	const int MAX_STACK_SIZE = LOG_STACK_TRACE_SIZE;
	void *stack[MAX_STACK_SIZE];
	int nframes = backtrace(stack, MAX_STACK_SIZE);
	stacktrace_os << "Stack trace returned " << nframes << " entries:" << std::endl;
	char **msgs = backtrace_symbols(stack, nframes);
	if (msgs != nullptr) {
		for (int frameno = 0; frameno < nframes; ++frameno) {
			string msg = ps_log::Demangle(msgs[frameno]);
			stacktrace_os << "[bt] (" << frameno << ") " << msg << "\n";
		}
	}
	free(msgs);
	string stack_trace = stacktrace_os.str();
	return stack_trace;
}
#endif	// LOG_STACK_TRACE

/**
 * @brief exception class that will be thrown by default logger if LOG_FATAL_THROW == 1
 */
struct PSError: public std::runtime_error {
	explicit PSError(const std::string &s) : std::runtime_error(s) {}
};

#if LOG_FATAL_THROW == 0
class LogMessageFatal: public LogMessage {
 public:
	LogMessageFatal(const char* file, int line): LogMessage(file, line) {}
	~LogMessageFatal() {
		log_stream_ << "\n";
		abort();
	}

 private:
	DISABLE_COPY_AND_ASSIGN(LogMessageFatal);
};
#else
class LogMessageFatal {
 public:
	LogMessageFatal(const char* file, int line) {
		set_time();
		log_stream_ << "\n[FATAL] "
					<< "[" << get_time() << "] "
					<< file << ":" << line << ": ";
	}
	~LogMessageFatal() noexcept(false) {
#if LOG_STACK_TRACE
		log_stream_ << "\n\n" << StackTrace() << "\n";
#endif
		LOG(ERROR) << log_stream_.str();
		throw PSError(log_stream_.str());
	}
	std::ostringstream &stream() { return log_stream_; } // 需要将日志保存，因此不能直接先输出

 private:
	std::ostringstream log_stream_; // 需要将输出保存作为 PSError 信息

	DISABLE_COPY_AND_ASSIGN(LogMessageFatal);
};
#endif

/**
 * @brief 将 ostream 类型的表达式的类型转为 void，避免三目运算符中两个值类型不匹配的问题
 * 比如：true ? (void)0 : ps_log::LogMessageVoidify() & LOG(severity)
 */
struct LogMessageVoidify {
	LogMessageVoidify() {}
	/**
	 * @brief 转换 ostream 类型表达式（包含流运算符）在三目运算符中的类型
	 * 因此需要一个比 << 优先级低、比 ?: 优先级高的运算符
	 */
	void operator &(std::ostream&) {}
};

} // namespace ps_log

#endif // USE_GLOG
