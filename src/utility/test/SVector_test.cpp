#include <gtest/gtest.h>

#include "../SVector.h"

using namespace ps;
using std::vector;

constexpr int V = 2024;

struct A {
	A(): x(V) {}
	A(int x): x(x) {}
	A(int x, int* desCounter): x(x), desCounter(desCounter) { upd(); }
	~A() {
		x = 0;
		if (!alive) {
			ADD_FAILURE();
		}
		alive = false;
		if (desCounter) {
			(*desCounter)--;
		}
	}

	A(const A& o): x(o.x), desCounter(o.desCounter) { upd(); }
	A(A&& o) noexcept: x(o.x), desCounter(o.desCounter) {o.x = 0; upd(); }
	A& operator =(const A& o) = default;
	A& operator =(A&& o) noexcept = default;

	bool operator == (const A& o) const {
		return x == o.x;
	}
	bool operator == (int value) const {
		return x == value;
	}
	void upd() { if (desCounter) (*desCounter)++; }
	int x;

private:
	bool alive{true}; // 检查是否重析构
	int* desCounter{nullptr}; // 检查析构次数是否正确
};

std::ostream& operator <<(std::ostream& os, const A& arg) {
	os << arg.x;
	return os;
}

// 如果要验证析构次数是否正确，则定义 counter，并用 AC 构造；最后判 counter 是否为0。
#define AC(value) A(value, &counter)

namespace ps {
AddRelocatable(A);
}

TEST(ConstructorTest, T1) {
	{
		SVector<A> s;
		EXPECT_EQ(s.size(), 0);
		EXPECT_EQ(s.capacity(), 0);
	}
	{
		constexpr size_t size = 5;
		SVector<int> s(size, 5);
		EXPECT_EQ(s.size(), size);
		EXPECT_EQ(s.capacity(), size);
		for (size_t i = 0; i < size; ++i) {
			EXPECT_EQ(s[i], 5);
		}
	}
	{
		constexpr size_t size = 7;
		SVector<A> s(size);
		EXPECT_EQ(s.size(), size);
		EXPECT_EQ(s.capacity(), size);
		for (size_t i = 0; i < size; ++i) {
			EXPECT_EQ(s[i], V);
		}
	}
	{
		constexpr size_t size = 3;
		SVector<std::string> s(size, "abc");
		EXPECT_EQ(s.size(), size);
		EXPECT_EQ(s.capacity(), size);
		for (size_t i = 0; i < size; ++i) {
			EXPECT_EQ(s[i], "abc");
		}
	}
	{
		constexpr size_t size = 3;
		SVector<std::vector<A>> s(size);
		s[1].push_back(A());
		EXPECT_EQ(s[1].data()[0], V);
	}
}

TEST(ConstructorTest, T2) {
	// SVector(const SVector&)
	{
		SVector<A> s1(4);
		#define Ck(s) \
			EXPECT_EQ(s[0], V); \
			EXPECT_EQ(s[1], 1); \
			EXPECT_EQ(s[2], 2); \
			EXPECT_EQ(s[3], V)
		s1[1] = A(1); // s1: V 1 V V
		{
			SVector<A> s2(s1);
			s2[2] = A(2);
			EXPECT_EQ(s2.size(), s1.size());
			Ck(s2);
		}
		Ck(s1);
	}
	{
		int counter = 0;
		{
			SVector<A> s1(3, AC(0)); // 0 0 0
			EXPECT_EQ(s1[1], 0);
			s1.reserve(6);
			EXPECT_EQ(s1.size(), 3);
			EXPECT_EQ(s1.capacity(), 6);
			s1[0] = AC(-1);
			{
				SVector<A> s2(s1);
				EXPECT_EQ(s2[0], -1);
				s2.emplace_back(99, &counter);
				EXPECT_EQ(s2.size(), 4);
				EXPECT_EQ(s2.capacity(), 6);
				EXPECT_EQ(s2[0], -1);
				EXPECT_EQ(s2[3], 99);
				s2[1] = AC(-2);
			}
			EXPECT_EQ(s1[1], -2);
			EXPECT_EQ(s1.size(), 3);
			EXPECT_EQ(s1.capacity(), 6);
		}
		EXPECT_EQ(counter, 0);
	}
	// (unsupported) SVector(const SVector<U>&)
	// // {
	// // 	uint16_t high = 1, low = 3;
	// // 	uint32_t v = (high << 16) | low;
	// // 	SVector<uint32_t> s1(3, v);
	// // 	uint16_t* p = reinterpret_cast<uint16_t*>(&s1[0]);
	// // 	bool isSmallEndian = (*p == 1);
	// // 	EXPECT_TRUE(isSmallEndian); // 为了方便，假设机器是小端存储
	// // 	{
	// // 		SVector<uint16_t> s2(s1); // 1 3...
	// // 		EXPECT_EQ(s2.size(), s1.size() * 2);
	// // 		EXPECT_EQ(s2[0], low);
	// // 		EXPECT_EQ(s2[1], high);
	// // 		s2[2] = low = 7;
	// // 		s2[3] = high = 10;
	// // 	}
	// // 	EXPECT_EQ(s1[0], v);
	// // 	EXPECT_EQ(s1[1], (high << 16) | low);
	// // }
}

TEST(ConstructorTest, T3) {
	// SVector(T* data, size_t size, bool deletable)
	{
		constexpr size_t size = 5;
		A a[size] = {1, 2, 3, 4, V};
		{
			SVector<A> s (a, size);
			EXPECT_EQ(s[1], 2);
			EXPECT_EQ(s[4], V);
			s[1] = -2;
			s[3] = -3;
			a[4].x = -V;
			EXPECT_EQ(s[4], -V);
		}
		EXPECT_EQ(a[1], -2);
		EXPECT_EQ(a[3], -3);
	}
	{
		constexpr size_t size = 5;
		std::allocator<A> alloc{};
		A* a = alloc.allocate(size);
		for (int i = 0; i < 4; ++i) {
			std::construct_at(a + i, i + 1);
		}
		std::construct_at(a + 4, V);

		SVector<A> s1 (a, size, true);
		s1.SetAllocator(std::move(alloc));
		s1[1] = -2;
		EXPECT_EQ(a[1], -2);
		{
			// share data between them
			SVector<A> s2 (a, size, false);
			a[2] = -5;
			EXPECT_EQ(s2[2], -5);
			s1[3] = -7;
			EXPECT_EQ(s2[3], -7);
			s2[1] = -3;
		}
		EXPECT_EQ(a[1], -3);
		EXPECT_EQ(s1[2], -5);
	}
	{
		// 与 vector 共享底层存储
		// 注意，修改 vector size 之外的元素将无法被 vector 所用
		vector<A> v {1, 2, 3, 4, V};
		v.reserve(10);
		SVector<A> s1 (v.data(), v.capacity());
		s1[1] = -2;
		EXPECT_EQ(v[1], -2);
		s1[5] = 5;
		EXPECT_EQ(s1[5], 5);
		{
			SVector<A> s2 (s1);
			EXPECT_EQ(s2[5], 5);
			s2[2] = -3;
		}
		EXPECT_EQ(s1[2], -3);
		EXPECT_EQ(v[2], -3);
	}
}

TEST(ConstructorTest, T4) {
	// SVector(const std::vector<T>&)
	// SVector(const std::shared_ptr<std::vector<T>>&)
	// SVector(const std::initializer_list<T>&)
	{
		vector<A> v {1, 2, 3, 4, V};
		{
			// copy, not sharing with vec
			SVector<A> s1(v);
			EXPECT_EQ(s1.size(), 5);
			EXPECT_EQ(s1.capacity(), 5);
			s1[0] = -1;
			{
				// share
				SVector<A> s2(s1);
				EXPECT_EQ(s2[0], -1);
				s2[1] = -2;
			}
			EXPECT_EQ(s1[1], -2);

			s1.emplace_back(6);
			EXPECT_EQ(s1.size(), 6);
			EXPECT_GE(s1.capacity(), 6);
			EXPECT_EQ(s1[5], 6);
		}
		EXPECT_EQ(v[0], 1);
		EXPECT_EQ(v[1], 2);
		EXPECT_EQ(v.capacity(), 5);
	}
	{
		using std::shared_ptr;
		auto sp = std::make_shared<vector<A>>(5, A(V));
		SVector<A> s1 (sp);
		EXPECT_EQ(s1.size(), 5);
		EXPECT_EQ(s1[1], V);
		s1[1] = 1;
		EXPECT_EQ(s1[1], 1);
		EXPECT_EQ((*sp)[1], 1);
		{
			SVector<A> s2 (sp);
			EXPECT_EQ(s2[1], 1);
			s2[1] = 2;
			EXPECT_EQ(s2[1], 2);
		}
		EXPECT_EQ(s1[1], 2);
		EXPECT_EQ((*sp)[1], 2);

		sp.reset();
		EXPECT_EQ(s1[1], 2);
		EXPECT_EQ(s1[4], V);
	}
	{
		SVector<A> s1 {1, 2, 3, 4, 5, V};
		EXPECT_EQ(s1.size(), 6);
		EXPECT_EQ(s1.capacity(), 6);
		EXPECT_EQ(s1[1], 2);
		s1.emplace_back(7);
		EXPECT_EQ(s1.size(), 7);
		EXPECT_GE(s1.capacity(), 7);
		EXPECT_EQ(s1[1], 2);
		EXPECT_EQ(s1[6], 7);
	}
	puts("---");
	{
		SVector<std::string> s1 {"1", "2", "qqq", "\n\n\t", "55555", "******"};
		EXPECT_EQ(s1.size(), 6);
		EXPECT_EQ(s1.capacity(), 6);
		EXPECT_EQ(s1[1], "2");
		std::cout << s1 << std::endl;

		s1.emplace_back("\t");
		EXPECT_EQ(s1.size(), 7);
		EXPECT_GE(s1.capacity(), 7);
		EXPECT_EQ(s1[1], "2");
		EXPECT_EQ(s1[6], "\t");
		std::cout << s1 << std::endl;
	}
}

TEST(CopyFromTest, T) {
	// SVector
	{
		SVector<A> s1 {1, 2, 3, 4, V};
		s1.reserve(10);

		SVector<A> s2;
		s2.CopyFrom(s1);
		EXPECT_EQ(s2.size(), 5);
		EXPECT_EQ(s2.capacity(), 10);
		EXPECT_TRUE(s1 == s2);
	}
	// data
	{
		int* p = new int[4]{1, 2, 3, V};
		SVector<int> s1;
		s1.CopyFrom(p, 4);
		EXPECT_EQ(s1[0], 1);
		EXPECT_EQ(s1[3], V);

		EXPECT_EQ(p[3], V);
		delete[] p;
	}
	{
		using std::string;
		string* p = new string[5] {"1", "ab", "cd", "wwwwwwwww", "\n\n\t"};
		SVector<string> s1;
		s1.CopyFrom(p, 5);
		EXPECT_EQ(s1[1], "ab");
		EXPECT_EQ(s1[3], "wwwwwwwww");
		EXPECT_EQ(s1[4], "\n\n\t");

		EXPECT_EQ(p[4], "\n\n\t");
		delete[] p;
	}
	// iterator
	{
		using std::string;
		std::map<int, string> m = {
			{1, "1"},
			{233, "qvq"},
			{999, "wwwwwww"},
			{-1, "\n\n\t"},
		};
		// map 的 value_type 是 pair<const Key, T>
		using T = std::map<int, string>::value_type;
		SVector<T> s1;
		s1.CopyFrom(m.begin(), m.end());
		EXPECT_EQ(s1.size(), 4);
		EXPECT_EQ(s1.capacity(), 4);
		// 注意 map 是有序的
		T v0 = {-1, "\n\n\t"}; EXPECT_EQ(s1[0], v0);
		T v1 = {1, "1"}; EXPECT_EQ(s1[1], v1);
		T v2 = {233, "qvq"}; EXPECT_EQ(s1[2], v2);
		T v3 = {999, "wwwwwww"}; EXPECT_EQ(s1[3], v3);
	}
}

TEST(SliceTest, T) {
	{
		SVector<std::string> s1 {"a", "111", "999999"};
		// SVector<std::string> s1 {"a", "111", "999999"};
		auto s2 = s1.Slice(0, s1.size());
		{
			auto s3 = s2.Slice(1, 2);
			auto s4 = s3.Slice(0, 1);
		}
	}
	int counter = 0;
	{
		SVector<A> s1 {AC(1), AC(2), AC(3), AC(4), AC(V)};
		std::cout << "testing: " << s1 << std::endl;
		{
			auto s2 = s1.Slice(2, 4); // 3, 4
			EXPECT_EQ(s2[0], 3);
			EXPECT_EQ(s2[1], 4);
			s1[2] = AC(-3);
			EXPECT_EQ(s2[0], -3);
			s2[1] = AC(-4);
			EXPECT_EQ(s2[1], -4);
			// 修改容量，与原数组断开联系
			s2.emplace_back(999, &counter);
			EXPECT_EQ(s2.size(), 3);
			EXPECT_GE(s2.capacity(), 3);
			EXPECT_EQ(s2[1], -4);
			EXPECT_EQ(s2[2], 999);
		}
		EXPECT_EQ(s1[3], -4);
		EXPECT_EQ(s1[4], V);
	}
	EXPECT_EQ(counter, 0);
	{
		using std::string;
		SVector<string> s1 {"1", "abc", "wwwwwwww", "\n\n\t"};
		SVector<string> s2 = s1.Slice(1, s1.size());
		EXPECT_EQ(s2[0], "abc");
		EXPECT_EQ(s2[2], "\n\n\t");
		s2[1] = "w";
		EXPECT_EQ(s1[2], "w");

		// 修改容量，两个数组断开联系
		s1.emplace_back("new data");
		EXPECT_EQ(s1.size(), 5);
		EXPECT_EQ(s1[4], "new data");
		EXPECT_EQ(s2.size(), 3);

		s1[2] = "q";
		EXPECT_EQ(s2[1], "w");
		s2[2] = "\n_\n";
		EXPECT_EQ(s1[3], "\n\n\t");
	}
}

// TEST(ResizeTest, T) {

// }

/*
TODO：
检查一下数组中的任何对象是否正常构造。用一个特别的构造函数。
测试功能时要测试多种情况，比如从 vector 构造、从数组指针构造等。

测：s1.reserve; s2=s1; s2.eb; s1再覆盖已有的，不会内存泄露。
*/