#if false
#include <gtest/gtest.h>

#ifndef NDEBUG // 使用 RELEASE 模式
#define NDEBUG
#endif
#ifndef LOG_FATAL_THROW // FATAL 时 throw
#define LOG_FATAL_THROW 1
#endif
#define VERBOSE 2 // 关闭 debug、info
#include "../log.h"

TEST(BasicTest, T) {
	using std::cout;
	int v1 = 1, v2 = 2;

	// ---
	cout << "You shouldn't see any output from below:\n";
	// disable because of VERBOSE
	LOG(DEBUG) << "log debug";
	LOG(INFO) << "log info";

	// disable because of RELEASE
	DCHECK(v1 > v2);
	DCHECK_GT(v1, v2);
	DLOG(WARNING) << "dlog warning";
	DLOG_IF(ERROR, v1 < v2) << "dlog_if error";

	// disable because of if
	LOG_IF(ERROR, v1 > v2) << "log_if error";

	// ---
	cout << "You should see every output from below:\n";
	LOG(WARNING) << "log warning";
	LOG(ERROR) << "log error";

	CHECK(v1 < v2);
	CHECK_LT(v1, v2);
	LOG(WARNING) << "log warning";
	LOG_IF(ERROR, v1 < v2) << "log_if error";

	// won't fatal because of RELEASE
	LOG(DFATAL) << "log dfatal which is error";

	// ---
	cout << "Every exception should be caught below.\n";
	int counter = 0;

	#define ADD_CHECK(expr) \
		try { \
			++counter; \
			expr; \
		} catch (const ps_log::PSError& e) { \
			--counter; \
		}

	ADD_CHECK(CHECK(v1 >= v2));
	ADD_CHECK(CHECK_GE(v1, v2));
	ADD_CHECK(LOG(FATAL) << "fatal error!");

	EXPECT_EQ(counter, 0);
}
#endif
