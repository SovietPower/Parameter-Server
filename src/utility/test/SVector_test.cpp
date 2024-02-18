#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "../SVector.h"

using namespace ps;
using std::vector;

constexpr int V = 2024;

struct A {
	A(): x(V) {}
	A(int x): x(x) {}
	~A() {x = 0;}

	A(const A& o) {x = o.x;}
	A(A&& o) {x = o.x; o.x = 0;}
	A& operator =(const A& o) {
		x = o.x;
		return *this;
	}
	A& operator =(A&& o) {
		x = o.x;
		o.x = 0;
		return *this;
	}

	bool operator == (const A& o) const {
		return x == o.x;
	}
	bool operator == (int value) const {
		return x == value;
	}
	int x;
};

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
		vector<A> v {1, 2, 3, 4, V};
		SVector<A> s1 (v.data(), v.capacity());
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
		}
		EXPECT_EQ(v[0], 1);
		EXPECT_EQ(v[1], 2);
	}
}


/*
TODO：
检查一下数组中的任何对象是否正常构造。用一个特别的构造函数。
测试功能时要测试多种情况，比如从 vector 构造、从数组指针构造等。
*/