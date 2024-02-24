#include <gtest/gtest.h>

#include "../SVector.h"

using namespace ps;
using std::vector;
using std::string;

constexpr int V = 2024;
constexpr int V_invalid = -998244353;

struct A {
	A(): x(V) {}
	A(int x): x(x) {}
	A(int x, int* desCounter): x(x), desCounter(desCounter) { upd(); }
	~A() {
		x = V_invalid;
		if (!alive) {
			ADD_FAILURE();
		}
		alive = false;
		if (desCounter) {
			(*desCounter)--;
		}
	}

	A(const A& o): x(o.x), desCounter(o.desCounter) { upd(); }
	A(A&& o) noexcept: x(o.x), desCounter(o.desCounter) {o.x = V_invalid; upd(); }
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

namespace ps {
AddRelocatable(A);
}

std::ostream& operator <<(std::ostream& os, const A& arg) {
	os << arg.x;
	return os;
}

// 如果要验证析构次数是否正确，则定义 counter，并用 AC 构造；最后判 counter 是否为0。
#define AC(value) A(value, &counter)

// 不使用 SSO、动态分配内存的字符串常量
static const std::string V_strings[] = {
	"123456789012345678901234567890123456",
	"ccccccccccccccccccccccccccccc\b\n\t ccccccccccccccccccccccccccccccc\n\n\n ",
	"hello, world!\n\n\0\t\r\\\\definition of static const std::string V_strings[] = \"...\";",
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
		SVector<std::string> s(size, V_strings[1]);
		EXPECT_EQ(s.size(), size);
		EXPECT_EQ(s.capacity(), size);
		for (size_t i = 0; i < size; ++i) {
			EXPECT_EQ(s[i], V_strings[1]);
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
	// SVector(const SVector&) SVector(SVector&&)
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
	int counter = 0;
	{
		{
			SVector<A> s1(3, AC(1)); // 1 1 1
			EXPECT_EQ(s1[1], 1);
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
			{
				SVector<A> s3(s1);
				EXPECT_EQ(s1, s3);
				SVector<A> s4(std::move(s1));
				EXPECT_EQ(s3, s4);
			}
			EXPECT_EQ(s1.size(), 0);
			EXPECT_EQ(s1.capacity(), 0);
		}
	}
	EXPECT_EQ(counter, 0);
	{
		SVector<string> s1 {"abc", V_strings[0], V_strings[1], V_strings[2]};
		{
			SVector<string> s2(std::move(s1));
			EXPECT_EQ(s2.capacity(), 4);
			EXPECT_EQ(s2[3], V_strings[2]);
		}
		SVector<string> s3(s1);
		EXPECT_EQ(s3.capacity(), 0);
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
	int counter = 0;
	{
		constexpr size_t size = 5;
		std::allocator<A> alloc{};
		A* a = alloc.allocate(size);
		for (int i = 0; i < 4; ++i) {
			std::construct_at(a + i, i + 1, &counter);
		}
		std::construct_at(a + 4, V, &counter);

		SVector<A> s1 (a, size, true);
		s1.SetAllocator(std::move(alloc));
		s1[1] = AC(-2);
		EXPECT_EQ(a[1], -2);
		{
			// share data between them
			SVector<A> s2 (a, size, false);
			a[2] = AC(-5);
			EXPECT_EQ(s2[2], -5);
			s1[3] = AC(-7);
			EXPECT_EQ(s2[3], -7);
			s2[1] = AC(-3);
		}
		EXPECT_EQ(a[1], -3);
		EXPECT_EQ(s1[2], -5);
	}
	EXPECT_EQ(counter, 0);
	{
		// 与 vector 共享底层存储
		// 注意，修改 vector size 之外的元素将无法被 vector 所用，且并不合法
		vector<A> v {AC(1), AC(2), AC(3), AC(4), AC(V)};
		v.reserve(10);
		SVector<A> s1 (v.data(), v.size());
		s1[1] = AC(-2);
		EXPECT_EQ(v[1], -2);
		// s1[5] = AC(5);
		// EXPECT_EQ(s1[5], 5);
		{
			SVector<A> s2 (s1);
			EXPECT_EQ(s2[1], -2);
			s2[2] = AC(-3);
		}
		EXPECT_EQ(s1[2], -3);
		EXPECT_EQ(v[2], -3);
		// 发生容量更改时，resize 将把原数组中的元素移动到新位置（如果可移动）
		s1.resize(8, AC(9));
		EXPECT_EQ(s1[7], 9);
		EXPECT_EQ(v[0], V_invalid);
		s1[2] = AC(-3);
		EXPECT_EQ(v[2], V_invalid);
	}
	EXPECT_EQ(counter, 0);
	{
		vector<string> v {"abc", V_strings[0], V_strings[1]};
		v.reserve(7);
		SVector<string> s1 (v.data(), v.size());
		EXPECT_EQ(s1[2], V_strings[1]);

		// 发生容量更改时，reserve 将把原数组中的元素移动到新位置（如果可移动）
		s1.reserve(9, V_strings[2]);
		EXPECT_EQ(s1[3], V_strings[2]); // 越界访问
		EXPECT_TRUE(v[0].empty());
		EXPECT_TRUE(v[1].empty());
		EXPECT_TRUE(v[2].empty());
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
	int counter = 0;
	{
		using std::shared_ptr;
		auto sp = std::make_shared<vector<A>>(5, AC(V));
		SVector<A> s1 (sp);
		EXPECT_EQ(s1.size(), 5);
		EXPECT_EQ(s1[1], V);
		s1[1] = AC(1);
		EXPECT_EQ(s1[1], 1);
		EXPECT_EQ((*sp)[1], 1);
		{
			SVector<A> s2 (sp);
			EXPECT_EQ(s2[1], 1);
			s2[1] = AC(2);
			EXPECT_EQ(s2[1], 2);
		}
		EXPECT_EQ(s1[1], 2);
		EXPECT_EQ((*sp)[1], 2);

		sp.reset();
		EXPECT_EQ(s1[1], 2);
		EXPECT_EQ(s1[4], V);
	}
	EXPECT_EQ(counter, 0);
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
	{
		SVector<std::string> s1 {"1", V_strings[0], "qqq", "\n\n\t", "55555", "******"};
		EXPECT_EQ(s1.size(), 6);
		EXPECT_EQ(s1.capacity(), 6);
		EXPECT_EQ(s1[1], V_strings[0]);

		s1.emplace_back(V_strings[2]);
		EXPECT_EQ(s1.size(), 7);
		EXPECT_GE(s1.capacity(), 7);
		EXPECT_EQ(s1[1], V_strings[0]);
		EXPECT_EQ(s1[6], V_strings[2]);
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
		string* p = new string[5] {"1", V_strings[1], "cd", "wwwwwwwww", "\n\n\t"};
		SVector<string> s1;
		s1.CopyFrom(p, 5);
		EXPECT_EQ(s1[1], V_strings[1]);
		EXPECT_EQ(s1[3], "wwwwwwwww");
		EXPECT_EQ(s1[4], "\n\n\t");

		EXPECT_EQ(p[4], "\n\n\t");
		delete[] p;
	}
	// iterator
	{
		using std::string;
		std::map<int, string> m = {
			{1, V_strings[1]},
			{233, V_strings[2]},
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
		T v1 = {1, V_strings[1]}; EXPECT_EQ(s1[1], v1);
		T v2 = {233, V_strings[2]}; EXPECT_EQ(s1[2], v2);
		T v3 = {999, "wwwwwww"}; EXPECT_EQ(s1[3], v3);
	}
}

TEST(SliceTest, T) {
	{
		SVector<std::string> s1 {"a", V_strings[1], "999999"};
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
		SVector<string> s1 {"1", V_strings[1], "wwwwwwww", "\n\n\t"};
		SVector<string> s2 = s1.Slice(1, s1.size());
		EXPECT_EQ(s2[0], V_strings[1]);
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

TEST(ResizeTest, T1) {
	// SVector()
	int counter = 0;
	{
		SVector<A> s1{};
		s1.resize(3, AC(1)); // 1 1 1
		EXPECT_EQ(s1.size(), 3);
		EXPECT_EQ(s1.capacity(), 3);
		EXPECT_EQ(s1[1], 1);

		s1.resize(6, AC(5));
		EXPECT_EQ(s1.size(), 6);
		EXPECT_EQ(s1.capacity(), 6);
		EXPECT_EQ(s1[0], 1);
		EXPECT_EQ(s1[3], 5);

		s1.resize(10, AC(10));
		EXPECT_EQ(s1[2], 1);
		EXPECT_EQ(s1[5], 5);
		EXPECT_EQ(s1[6], 10);
		EXPECT_EQ(s1[9], 10);

		s1.push_back(AC(99));
		EXPECT_EQ(s1.size(), 11);
		EXPECT_EQ(s1[10], 99);
	}
	EXPECT_EQ(counter, 0);
	{
		SVector<string> s1{};
		s1.resize(4, "abc");
		EXPECT_EQ(s1[3], "abc");
		s1.resize(10, V_strings[0]);
		EXPECT_EQ(s1[7], V_strings[0]);
		s1.resize(20, V_strings[2]);
		EXPECT_EQ(s1[15], V_strings[2]);

		EXPECT_EQ(s1[3], "abc");
		EXPECT_EQ(s1[9], V_strings[0]);
		EXPECT_EQ(s1.size(), 20);
		EXPECT_EQ(s1[19], V_strings[2]);
	}
	{
		SVector<vector<A>> s1{};
		s1.resize(2, vector<A>(3, AC(1))); // 2*[1, 1, 1]
		EXPECT_EQ(s1[1].size(), 3);
		EXPECT_EQ(s1[1][2], 1);
		s1.resize(5, vector<A>(6, AC(7))); // 5*[7, 7, 7, 7, 7, 7]
		EXPECT_EQ(s1[4].size(), 6);
		EXPECT_EQ(s1[4][5], 7);
		s1.resize(10, vector<A>(2, AC(9)));
		EXPECT_EQ(s1[0].size(), 3);
		EXPECT_EQ(s1[0][2], 1);
	}
	EXPECT_EQ(counter, 0);
}

TEST(ResizeTest, T2) {
	// SVector(const SVector&) SVector(SVector&&)
	// SVector(const vector&)
	// SVector(const sptr<vector>&)
	int counter = 0;
	{
		SVector<A> s1 {AC(1), AC(2), AC(V)};
		{
			SVector<A> s2(s1);
			s2.resize(6, AC(26));
			EXPECT_EQ(s2.size(), 6);
			EXPECT_EQ(s2[0], 1);
			EXPECT_EQ(s2[5], 26);
			EXPECT_EQ(s1.size(), 3);
			// not sharing
			s2[1] = AC(-2);
			EXPECT_EQ(s1[1], 2);

			s1.resize(9, AC(19)); // [1, 2, V, 19, ...]
			EXPECT_EQ(s1.size(), 9);
			EXPECT_EQ(s1[0], 1);
			EXPECT_EQ(s1[8], 19);
			EXPECT_EQ(s2.size(), 6);
			s1[2] = AC(-V);
			EXPECT_EQ(s2[2], V);
		}
		// new_size < cap 时，不会创建新的底层数组
		SVector<A> s3(s1); // 9
		s3.resize(5);
		EXPECT_EQ(s3.size(), 5);
		EXPECT_EQ(s3.capacity(), 9);
		EXPECT_EQ(s3[4], 19);
		s3[4] = AC(-19);
		EXPECT_EQ(s1.size(), 9);
		EXPECT_EQ(s1[4], -19);
		{
			SVector<A> s4(std::move(s1));
			s4.resize(10, AC(410));
			EXPECT_EQ(s4[2], -V);
			EXPECT_EQ(s4[8], 19);
			EXPECT_EQ(s4[9], 410);
		}
	}
	EXPECT_EQ(counter, 0);
	{
		vector<string> v {"abc", V_strings[0], V_strings[1]};
		SVector<string> s1(v); // copy from vec
		EXPECT_EQ(s1[2], V_strings[1]);
		s1[2] = V_strings[2];
		EXPECT_EQ(v[2], V_strings[1]);

		s1.resize(5, V_strings[0]);
		s1.resize(9, "123");
		EXPECT_EQ(s1[2], V_strings[2]);
		EXPECT_EQ(s1[4], V_strings[0]);
		EXPECT_EQ(s1[8], "123");
	}
	{
		std::shared_ptr<vector<A>> sp = std::make_shared<vector<A>>(5, AC(1));
		SVector<A> s1(sp);
		s1[0] = AC(-1);
		EXPECT_EQ((*sp)[0], -1);
		// 与 sp 断开联系
		// 不是资源的唯一持有者，因此进行复制
		s1.resize(9, AC(10));
		EXPECT_EQ(s1[8], 10);
		s1[1] = AC(5);
		EXPECT_EQ((*sp)[1], 1);

		SVector<A> s2(sp);
		sp.reset();
		EXPECT_EQ(s2[1], 1);
		// s2 为资源的唯一持有者，移动元素，然后释放原内存
		// 注意，s2 的 deleter 来自外部，因此无法修改其实现、进行重定位
		s2.resize(11, AC(20));
		EXPECT_EQ(s2[2], 1);
		EXPECT_EQ(s2[10], 20);
	}
	EXPECT_EQ(counter, 0);
	{
		std::shared_ptr<vector<string>> sp = std::make_shared<vector<string>>(3, V_strings[2]);
		SVector<string> s1(sp);
		// 与 sp 断开联系
		// 不是资源的唯一持有者，因此进行复制
		s1.resize(9, "abc");
		EXPECT_EQ(s1[8], "abc");
		s1[1] = V_strings[0];
		EXPECT_EQ((*sp)[1], V_strings[2]);

		SVector<string> s2(sp);
		sp.reset();
		EXPECT_EQ(s2[1], V_strings[2]);
		// s2 为资源的唯一持有者，移动元素，然后释放原内存
		s2.resize(11, V_strings[0]);
		EXPECT_EQ(s2[2], V_strings[2]);
		EXPECT_EQ(s2[10], V_strings[0]);
	}
}

TEST(ResizeTest, T3) {
	// trivially copyable
	{
		SVector<double> s1 {1., 2., 3.};
		s1.resize(7, 10.);
		EXPECT_EQ(s1[1], 2.);
		EXPECT_EQ(s1[6], 10.);

		s1.reserve(10, 20.);
		s1.emplace_back(8.);
		EXPECT_EQ(s1[7], 8.);
		EXPECT_EQ(s1.at(7), 8.);
		EXPECT_EQ(s1[8], 20.); // 越界访问
		EXPECT_THROW(s1.at(8), std::out_of_range);
	}
	// relocatable
	{
		SVector<vector<string>> s1;
		s1.resize(3); // 3 empty vector
		EXPECT_EQ(s1[0].size(), 0);
		s1[0].emplace_back(V_strings[0]);
		EXPECT_EQ(s1.at(0).at(0), V_strings[0]);

		auto v1 = vector<string>{"abc", V_strings[1]};
		s1.reserve(7, v1);
		EXPECT_EQ(s1[3], v1); // 越界访问
		auto v2 = vector<string> {"121", V_strings[2]};
		s1.push_back(v2);
		EXPECT_EQ(s1[3], v2);

		auto s2 (s1);
		EXPECT_EQ(s1, s2);

		auto v3 = vector<string> {"www"};
		s1.resize(11, v3);
		EXPECT_EQ(s1[6], v1);
		EXPECT_EQ(s1[7], v3);
		s1[0].push_back("999");

		// s1, s2 断开联系
		EXPECT_EQ(s2.capacity(), 7);
		EXPECT_EQ(s2[0].size(), 1);
		s1[3].push_back("777");
		EXPECT_EQ(s2[3], v2);

		s2.resize(10);
		EXPECT_EQ(s1[7], v3);
	}
}

TEST(ReserveTest, T) {
	int counter = 0;
	{
		SVector<A> s1;
		s1.reserve(5, AC(1));
		EXPECT_EQ(s1[0], 1); // 越界访问
		EXPECT_EQ(s1.size(), 0);
		EXPECT_EQ(s1.capacity(), 5);
		s1.emplace_back(2, &counter);
		EXPECT_EQ(s1.size(), 1);
		EXPECT_EQ(s1[0], 2);
		s1.push_back(AC(3));
		EXPECT_EQ(s1.size(), 2);
		EXPECT_EQ(s1[1], 3);

		SVector<A> s2(s1);
		s2.emplace_back(AC(10));
		EXPECT_EQ(s1.size(), 2);
		EXPECT_EQ(s2.size(), 3);
		EXPECT_EQ(s2.capacity(), 5);
		EXPECT_EQ(s2[1], 3);
		EXPECT_EQ(s2[2], 10);

		// s1, s2 共用底层数组，因此其修改可能互相覆盖
		s1.emplace_back(AC(20));
		EXPECT_EQ(s1.size(), 3);
		EXPECT_EQ(s2[2], 20);

		s2.push_back(AC(30));
		EXPECT_EQ(s1.size(), 3);
		EXPECT_EQ(s2[3], 30);
	}
	EXPECT_EQ(counter, 0);
	{
		SVector<string> s1;
		s1.reserve(5, V_strings[0]);
		EXPECT_EQ(s1[0], V_strings[0]); // 越界访问
		EXPECT_EQ(s1.size(), 0);
		EXPECT_EQ(s1.capacity(), 5);
		s1.emplace_back(V_strings[1]);
		EXPECT_EQ(s1.size(), 1);
		EXPECT_EQ(s1[0], V_strings[1]);
		s1.push_back("abc");
		EXPECT_EQ(s1.size(), 2);
		EXPECT_EQ(s1[1], "abc");

		SVector<string> s2(s1);
		s2.emplace_back(V_strings[2]);
		EXPECT_EQ(s1.size(), 2);
		EXPECT_EQ(s2.size(), 3);
		EXPECT_EQ(s2.capacity(), 5);
		EXPECT_EQ(s2[1], "abc");
		EXPECT_EQ(s2[2], V_strings[2]);

		// s1, s2 共用底层数组，因此其修改可能互相覆盖
		s1.emplace_back(V_strings[1]);
		EXPECT_EQ(s1.size(), 3);
		EXPECT_EQ(s2[2], V_strings[1]);
	}
}

TEST(ClearSwapTest, T) {
	int counter = 0;
	{
		SVector<A> s1 {AC(1), AC(2), AC(V)};
		SVector<A> s2 (s1);
		s1.clear();
		s2.clear();
		vector<A> v {AC(-1), AC(-2), AC(-3), AC(-V)};
		s1.CopyFrom(v.begin(), v.end());
		s2 = SVector<A>(v);
		EXPECT_EQ(s1, s2);
		s1.clear();
		s2.clear();
	}
	EXPECT_EQ(counter, 0);
	{
		vector<A> v {AC(-1), AC(-2), AC(-3), AC(-V)};
		SVector<A> s1;
		s1.CopyFrom(v.begin(), v.end());
		SVector<A> s2 = SVector<A>(v);
		EXPECT_EQ(s1, s2);

		SVector<A> s3;
		s3.swap(s1);
		EXPECT_EQ(s3, s2);
		s2.clear();
		EXPECT_EQ(s1, s2);
	}
	EXPECT_EQ(counter, 0);
	{
		vector<string> v {"abc", V_strings[0], V_strings[1]};
		SVector<string> s1;
		s1.CopyFrom(v.begin(), v.end());
		EXPECT_EQ(s1[2], V_strings[1]);
		SVector<string> s2 = SVector<string>(v);
		EXPECT_EQ(s1, s2);

		SVector<string> s3;
		s3.swap(s1);
		EXPECT_EQ(s3, s2);
		s2.clear();
		EXPECT_EQ(s1, s2);
	}
}

/*
测试功能时要测试多种情况，包括：
直接构造；从 SVector 构造；从 vector 构造；从 sptr<vector> 构造；从数组指针构造；空构造然后 CopyFrom。
涉及 resize 时，要测试可平凡复制类型、可重定位类型 (A, vector) 与其它类型 (string)。
*/