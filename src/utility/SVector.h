/**
 * @file SVector.h
 * @date 2024-01-25
 */
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cstring>
#include <sstream>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <initializer_list>

#include "base/log.h"

namespace ps {

// --- Relocatable
template <typename T>
struct IsRelocatable: std::false_type {};
template <typename T>
constexpr bool IsRelocatableV = IsRelocatable<T>::value;

#define AddRelocatable(type) \
	template <> \
	struct IsRelocatable<type>: std::true_type {};
#define AddRelocatableT(type) \
	template <typename T> \
	struct IsRelocatable<type<T>>: std::true_type {};

AddRelocatableT(std::vector)
// AddRelocatable(std::string) // 部分SSO实现可能导致它不是

// #undef AddRelocatable
// #undef AddRelocatableT

// --- Deleter
namespace {
/**
 * 因为对象被从 src relocate 到 dst 后，不能再调用 src 的析构，因为这相当于调用 dst 的析构。
 * 此时需要仅释放内存而不调用元素的析构函数。
 * 因此当进行 relocate 时，需要`deallocate(data)`；当不进行 relocate、单纯释放时，需要析构元素再释放。
 * 即传给 shared_ptr 的 deleter 必须有状态，或可改变，以根据情况执行不同语句。
 * 但 sptr 创建后不能改变 deleter，故只能为其附加状态：用一个 enum 值表示是否要析构元素，然后在需要时通过 get_deleter 修改。
 * 注意 data [size, capacity) 的部分元素尚未进行构造，因此只能析构 [0, size)；但释放内存时需要传入 capacity。因此两者都要保存。
 */

/**
 * @brief SVector 中 sptr 的 deleter 有三种可能，因此通过枚举表示它们。
 */
enum DeleterType: uint8_t {
	CannotFree, // 不能进行释放内存（即不做任何事）
	NeedDestruct, // 析构元素再释放内存（需传入 Allocator）
	CanOnlyFree, // 只释放内存，不能析构元素（需传入 Allocator）
};

/**
 * @brief SVector::sptr 专用的 deleter。
 */
template <typename T, typename Allocator>
struct Deleter {
	explicit Deleter(DeleterType dt, Allocator* alloc = nullptr, size_t cap = 0)
			: flag(dt), allocator(alloc), capacity(cap) {
		if (flag != CannotFree) {
			DCHECK_NOTNULL(allocator);
		}
	}

	void operator()(T* data) {
		if (flag != CannotFree) {
			if (flag == NeedDestruct) {
				for (size_t i = 0; i < capacity; ++i) {
					std::destroy_at(data + i);
				}
				// delete[] data;
			}
			allocator->deallocate(data, capacity);
		}
	}

	DeleterType flag;
	Allocator* allocator;
	size_t capacity;
};
} // namespace

// --- SVector
/**
 * SVector<T> 是一个变长数组， 提供的接口与 vector<T> 类似。
 * 特点是从另一个 SVector、vector 或数组（1）构造 SVector 时，可以选择零拷贝的方式，
 * 也就是让 SVector 直接与被赋值对象共享底层数组的 data 指针、而无需拷贝数据，
 * 使用引用计数保证底层的 data 被正确释放、不会出现悬垂指针。
 * 与 shared_ptr 类似，SVector 的非 const 操作不是线程安全的。

 * 注：为了减少接口数量，allocator 默认进行值初始化，且只有最基本的构造函数支持设置 allocator。可以先设置后，再调用其它逻辑实现其它的构造方式。
 * 因此由于共享的复杂性，不能与 vector 行为一致：整个 data [0, capacity) 的位置的元素都要完成构造，即使没有使用（因此 T 需要可默认构造）。这样释放时不需要已知 size，只需要 capacity。
 */

/**
 * @brief 可共享的 Vector
 * @tparam T 元素的类型
 */
template <typename T, typename Allocator = std::allocator<T>>
	requires
		requires (Allocator a, size_t n, T* p) {
			// T 需要可默认构造
			T{};
			// Allocator 需满足分配器 (Allocator) 要求
			{ a.allocate(n) } -> std::same_as<T*>;
			a.deallocate(p, n);
		}
class SVector {
	using DeleterT = Deleter<T, Allocator>;

 public:
	SVector(): size_(0), capacity_(0), ptr_(nullptr), allocator() {}

	/** @brief 由 shared_ptr<T> 的 deleter 决定何时释放数组 */
	~SVector() = default;

	/**
	 * @brief 构造拥有 count 个值 value 的元素的 SVector。
	 * @param count 大小
	 * @param value 初始值
	 */
	explicit SVector(size_t count, const Allocator& alloc = Allocator())
			: size_(0), capacity_(0), ptr_(nullptr), allocator(alloc) {
		// 注意 resize 函数本身不会先初始化成员，但是使用了成员，需要自行初始化成员
		resize(count);
	}
	explicit SVector(size_t count, const T& value, const Allocator& alloc = Allocator())
			: size_(0), capacity_(0), ptr_(nullptr), allocator(alloc) {
		resize(count, value);
	}

	/**
	 * @brief 通过 SVector 构造，与其共享底层数组和引用计数，无需拷贝。
	 * @param other 另一个 SVector。~~不需要与其有相同的模板类型，但 U* 必须可转换为 T*~~
	 */
	SVector(const SVector& other) {
		*this = other;
	}
	SVector(SVector&& other) {
		*this = std::move(other);
	}

	/**
	 * @brief 通过 SVector 赋值，与其共享底层数组和引用计数，无需拷贝。
	 * @param other 另一个 SVector。~~不需要与其有相同的模板类型，但 U* 必须可转换为 T*~~
	*/
	SVector& operator = (const SVector& other) {
		if (this == &other) {
			return *this;
		}
		size_ = other.size();
		capacity_ = other.capacity();
		ptr_ = other.ptr_;
		allocator = other.allocator;
		return *this;

		// // 与另一个 shared_ptr 共享计数，并拷贝其析构函数；将其保存的指针转为 T* 保存。
		// // ptr_ = std::shared_ptr<T>(other.getSharedPtr(), reinterpret_cast<T*>(other.data()));
	}
	SVector& operator = (SVector&& other) {
		if (this == &other) {
			return *this;
		}
		size_ = other.size_;
		capacity_ = other.capacity_;
		ptr_ = std::move(other.ptr_);
		allocator = std::move(other.allocator);
		other.size_ = 0;
		other.capacity_ = 0;
		return *this;
	}

	/**
	 * @brief 通过数组构造，直接将它作为 SVector 的底层数组，无需拷贝。
	 * 当 vector 非 const 时，可以用它的 data 调用此构造函数，以零拷贝使用 vector：SVector(v.data(), v.size(), false)。但要注意生命周期及 v.data() 的有效性，且 deletable 必须为 false。
	 * 这样构造的 SVector 不应该发生容量更改，否则 reserve 将把原数组中的元素移动到新位置（如果可移动）。
	 * @param data 数组指针
	 * @param size 数组长度
	 * @param deletable 当引用计数变为0时，是否要进行析构并释放该数组。应该始终为 false，除非使用同样的 allocator 分配。
	 */
	explicit SVector(T* data, size_t size, bool deletable = false): allocator() {
		if (!deletable) {
			reset(data, size, CannotFree);
		} else {
			reset(data, size, NeedDestruct);
		}
	}

	/**
	 * @brief 通过 vector 构造，拷贝其数据。
	 * 当 vector 非 const 时，可调用 SVector(v.data(), v.capacity(), false) 零拷贝获取其数据。但要注意生命周期及 v.data() 的有效性。
	 */
	explicit SVector(const std::vector<T>& vec): allocator() {
		CopyFrom(vec.data(), vec.size());
	}

	/**
	 * @brief 通过 shared_ptr<vector> 构造，与其共享底层数组和引用计数，无需拷贝。但要注意尽管 vector 的生命周期与 SVector 一致，仍要保证 v.data() 的有效性。
	 */
	explicit SVector(const std::shared_ptr<std::vector<T>>& sp): allocator() {
		size_ = sp->size();
		capacity_ = sp->capacity();
		// shared_ptr 的别名构造函数
		ptr_ = std::shared_ptr<T>(sp, sp->data());
	}

	/**
	 * @brief 通过初始化列表构造。
	 */
	explicit SVector(const std::initializer_list<T>& init_list): allocator() {
		CopyFrom(init_list.begin(), init_list.end());
	}

	// --- 从其它对象复制数据
	/**
	 * @brief 从 SVector 拷贝数据。创建新的引用计数。
	 * 将拷贝整个底层数组（包括 [size, capacity) 的部分）。
	 */
	void CopyFrom(const SVector<T>& other) {
		if (this == &other) {
			return;
		}
		CopyFrom(other.data(), other.capacity());
		size_ = other.size_;
	}

	/**
	 * @brief 从数组拷贝数据。创建新的引用计数。更新 size_、capacity_ 为 size。
	 */
	void CopyFrom(const T* src, size_t size) {
		T* newData = Allocate(size);
		// 需要拷贝构造与原数组无关的对象，因此不能 relocate/move
		if constexpr (std::is_trivially_copy_constructible_v<T>) {
			memcpy(newData, src, size * sizeof(T));
		} else {
			// copy
			T* cur = newData;
			auto first = src;
			auto last = src + size;
			for (; first != last; ++first, ++cur) {
				std::construct_at(cur, *first);
			}
		}
		reset(newData, size, NeedDestruct);
	}

	/**
	 * @brief 遍历迭代器拷贝数据。创建新的引用计数。
	 * @param first, last 要复制的元素范围
	 */
	template <std::forward_iterator ForwardIt>
	void CopyFrom(ForwardIt first, ForwardIt last) {
		size_t size = std::distance(first, last);
		T* newData = Allocate(size);
		for (T* cur = newData; first != last; ++first, ++cur) {
			std::construct_at(cur, *first);
		}
		reset(newData, size, NeedDestruct);
	}

	/**
	 * @brief 获取底层数组的切片 [left, right)，持有引用计数（类似 go 的切片）。
	 * 对切片的读写和修改是安全的。当容量改变时会创建新的底层数组，与原数组断开联系。
	 * @return SVector<T> 包含切片的 SVector
	 */
	SVector<T> Slice(size_t left, size_t right) {
		DCHECK_LE(left, right); DCHECK_LE(right, size_);

		SVector<T> ret;
		ret.size_ = right - left;
		ret.capacity_ = right - left;
		// 共享引用计数与 deleter
		ret.ptr_ = std::shared_ptr<T>(ptr_, data() + left);
		ret.allocator = allocator;
		return ret;
	}

	/**
	 * @brief 设置 allocator。不推荐使用。使用前需确保构造函数中没有发生内存分配。
	 */
	void SetAllocator(const Allocator& alloc) {
		allocator = alloc;
	}
	void SetAllocator(Allocator&& alloc) {
		allocator = std::move(alloc);
	}

	// --- 修改器
	// 不支持删除（erase, pop_back, shrink_to_fit），因为底层数组可能会与其它 SVector 共享，难以决定何时进行析构。
	// 除非删除时创建新数组拷贝。

	/**
	 * @brief 以 ptr 所指向的对象替换被管理对象，并更新 size_、capacity_ 为 size。
	 */
	void reset(T* data, size_t size, DeleterType dt) {
		size_ = size;
		capacity_ = size;
		DeleterT deleter = dt == CannotFree ? DeleterT(CannotFree) : DeleterT(dt, &allocator, size);
		ptr_.reset(data, deleter);
	}

	/**
	 * @brief 更改 SVector 的大小和容量以容纳 size 个元素。超过原容量的位置用 default_value 填充。
	 * 与 vector 不同的是当 new_size < capacity 时不会缩小容器；当 new_size < size 时不会进行析构。
	 * 注意：new_size <= capacity 时，SVector 不会创建新数组、仅更改 size；否则会创建新的底层数组。
	 * @param size 新的大小
	 */
	void resize(size_t new_size, const T& default_value = T{}) {
		// 即使 new_size < capacity_，也不减小容量，仅设置 size_，且不进行元素析构
		if (new_size <= capacity_) {
			size_ = new_size;
			return;
		}
		// 分配新空间，并拷贝/移动旧的元素，包括 [size, capacity) 部分的元素（它们已构造）
		// 会新建一个引用计数存放新的 data，释放旧 data 的计数
		T* newData = Allocate(new_size);
		if constexpr (std::is_trivially_copy_constructible_v<T>) {
			// memcpy 会隐式构造对象
			memcpy(newData, data(), capacity_ * sizeof(T));
		} else if (ptr_.use_count() == 1) {
			bool done = false;
			// 需要 -Wnoclass-memaccess 关闭不可 memcpy 的警告
			if constexpr (IsRelocatableV<T>) {
				DeleterT* deleter = std::get_deleter<DeleterT>(ptr_);
				// 如果 deleter 从其它位置获取（如 shared_ptr<vector>），则 deleter 为 nullptr
				// 此时无法修改其实现、进行 relocate，直接跳过
				if (deleter != nullptr && deleter->flag != CannotFree) {
					// relocate
					// relocatable 的对象在 memcpy 后，src 和 dst 有且只能有一个调用析构。因此只有当前对象唯一持有 data 的所有权时，才可避免执行析构或不恰当的修改，才能进行 relocate。
					done = true;
					memcpy(newData, data(), capacity_ * sizeof(T));

					// 进行 relocate 后，需要释放内存，但不能进行元素析构。
					deleter->flag = CanOnlyFree;
				}
			}
			if (!done) {
				// move or copy
				// 尝试移动原数组元素（仅在可 noexcept 移动或不可拷贝时使用移动）
				// 只有当前对象唯一持有 data 的所有权、将在 reset 后释放它时，才可确保能移动对象
				T* cur = newData;
				auto first = getMoveIterIfNoexcept(begin());
				auto last = getMoveIterIfNoexcept(begin() + capacity_);
				for (; first != last; ++first, ++cur) {
					std::construct_at(cur, *first);
					// *cur = *first;
				}
			}
		} else {
			// copy
			T* cur = newData;
			auto first = begin();
			auto last = begin() + capacity_;
			for (; first != last; ++first, ++cur) {
				std::construct_at(cur, *first);
			}
		}

		// 构造和初始化 [capacity, new_size) 的部分
		T* cur = newData + capacity_;
		T* last = newData + new_size;
		for (; cur != last; ++cur) {
			std::construct_at(cur, default_value);
		}

		// newData 是 SVector 分配并构造的，因此最后需要进行析构并回收
		reset(newData, new_size, NeedDestruct);
	}

	/**
	 * @brief 增加 SVector 的容量以容纳 new_cap 个元素。不改变 size。
	 * 与 vector 不同，新元素会进行构造和值初始化。
	 * @param new_cap 新的容量。小于等于原容量时不做任何事。
	 */
	void reserve(size_t new_cap, const T& default_value = T{}) {
		if (new_cap <= capacity_) {
			return;
		}
		auto oldSize = size_;
		resize(new_cap, default_value);
		size_ = oldSize;
	}

	template <typename... Args>
	T& emplace_back(Args... args) {
		if (size_ == capacity_) {
			if (capacity_ == 0) {
				reserve(2);
			} else {
				reserve(capacity_ * 2);
			}
		}
		DCHECK_LT(size_, capacity_);
		size_++;
		std::destroy_at(end() - 1); // 注意末尾处已有构造好的元素，要进行析构
		return *std::construct_at(end() - 1, std::forward<Args>(args)...);

		// 如果不使用 construct_at 或 placement new，则会多一次拷贝/移动，不能原地构造
		// return (*(data() + size_ - 1) = T(std::forward<Args>(args)...)); // 已构造，直接赋值即可
	}

	void push_back(const T& value) {
		emplace_back(value);
	}
	void push_back(T&& value) {
		emplace_back(std::move(value));
	}

	// 不支持
	// void pop_back() noexcept {
	// 	DCHECK_GT(size_, 0);
	// 	size_--;
	// 	end()->~T(); // 注意，删除元素后要析构它
	// 	// std::destroy_at(end());
	// }

	void clear() noexcept {
		// 注意不需要显式进行元素析构，这是 sptr 的事
		reset(nullptr, 0, CannotFree);
	}

	void swap(SVector& other) noexcept {
		// 注意，swap 应为 noexcept
		std::swap(size_, other.size_);
		std::swap(capacity_, other.capacity_);
		ptr_.swap(other.ptr_);
		// allocator 就不进行交换了
	}

	// --- 元素访问
	T* data() noexcept {
		return ptr_.get();
	}
	const T* data() const noexcept {
		return ptr_.get();
	}
	T& operator [] (size_t index) {
		return data()[index];
	}
	const T& operator [] (size_t index) const {
		return data()[index];
	}
	T& at(size_t index) {
		if (index >= size_) {
			throw std::out_of_range("");
		}
		return data()[index];
	}
	const T& at(size_t index) const {
		if (index >= size_) {
			throw std::out_of_range("");
		}
		return data()[index];
	}
	// 不进行边界检查
	T& front() {
		return data()[0];
	}
	const T& front() const {
		return data()[0];
	}
	T& back() {
		return data()[size_ - 1];
	}
	const T& back() const {
		return data()[size_ - 1];
	}

	// --- 迭代器
	T* begin() noexcept {
		return data();
	}
	const T* begin() const noexcept {
		return data();
	}
	const T* cbegin() const noexcept {
		return data();
	}
	T* end() noexcept {
		return data() + size_;
	}
	const T* end() const noexcept {
		return data() + size_;
	}
	const T* cend() const noexcept {
		return data() + size_;
	}

	// --- 容量
	bool empty() const noexcept {
		return size_ == 0;
	}
	size_t size() const noexcept {
		return size_;
	}
	size_t capacity() const noexcept {
		return capacity_;
	}

	const std::shared_ptr<T>& getSharedPtr() const {
		return ptr_;
	}

	/**
	 * @brief 生成调试字符串。
	 * @param tab 起始缩进。
	 * @param lim 元素的输出数量上限。-1 为全部输出。
	 */
	std::string DebugString(size_t lim = 10, size_t tab = 0) const
		requires
			requires(T t, std::stringstream ss) {
				ss << t; // 仅为支持 << 的对象生成函数
			} {
		std::string tabStr(tab, '\t');
		std::stringstream ss;
		#define Output(str) ss << tabStr << str
		Output("{ SVector\n");
		Output("\tsize: ") << size_ << " capacity: " << capacity_ << '\n';
		// data
		Output("\tdata: [ ");
		const T* data = this->data(); // 注意是 const T*
		if (size_ < 2 * lim) {
			for (size_t i = 0; i < size_; ++i) {
				ss << data[i] << ' ';
			}
		} else {
			for (size_t i = 0; i <lim; ++i) {
				ss << data[i] << ' ';
			}
			ss << "... ";
			for (size_t i = size_ - lim; i < size_; ++i) {
				ss << data[i] << ' ';
			}
		}
		ss << "]\n";
		Output("}");
		#undef Output
		return ss.str();
	}

 private:
	/**
	 * @brief 分配指定数量个元素的内存。不会进行构造。
	 */
	T* Allocate(size_t size) {
		return allocator.allocate(size);
	}

	/**
	 * @brief 当对象的移动构造为 noexcept 时，将 iter 转为 move iterator 返回；
	 * 否则返回 iter 使用普通 iterator。
	 * @param iter 要转换的 iterator
	 */
	template <typename Iter,
		typename RetType = std::conditional_t<
			(std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>),
			std::move_iterator<Iter>,
			Iter
		>>
	constexpr RetType getMoveIterIfNoexcept(Iter iter) {
		return RetType(iter);
	}

 private:
	size_t size_;
	size_t capacity_;
	std::shared_ptr<T> ptr_;
	Allocator allocator;
};

template <typename T, typename Allocator>
	requires
		requires(T t, std::stringstream ss) {
			ss << t;
		}
std::ostream& operator <<(std::ostream& os, const SVector<T, Allocator>& arg) {
	os << arg.DebugString();
	return os;
}

template <class T, class Alloc>
	requires
		requires (T a, T b) {
			{ a == b } -> std::convertible_to<bool>;
		}
bool operator ==(const SVector<T, Alloc>& lhs, const SVector<T, Alloc>& rhs) {
	if (lhs.size() != rhs.size() || lhs.capacity() != rhs.capacity()) {
		return false;
	}
	auto cur = lhs.begin();
	auto first = rhs.begin();
	auto last = rhs.end();
	for (; first != last; ++cur, ++first) {
		if (*cur != *first) {
			return false;
		}
	}
	return true;
}

} // namespace ps