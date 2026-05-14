#pragma once
#include <cstdint>

namespace crt
{
	void copy_memory(void* destination, const void* source, std::uint64_t size);
	void set_memory(void* destination, std::uint8_t value, std::uint64_t size);

	template <class T>
	T min(T a, T b)
	{
		return (a < b) ? a : b;
	}

	template <class T>
	T max(T a, T b)
	{
		return (a < b) ? b : a;
	}

	template <class T>
	T abs(T n)
	{
		return (n < 0) ? -n : n;
	}

	template <class T>
	void swap(T& a, T& b)
	{
		T cache = a;

		a = b;
		b = cache;
	}

	class mutex_t
	{
	protected:
		volatile std::int64_t value;

	public:
		mutex_t();
		
		void lock();
		void release();
	};

	class bitmap_t
	{
	protected:
		constexpr static std::uint64_t bit_count_in_row = 64;

		std::uint64_t* value = nullptr;
		std::uint64_t value_count = 0;

		std::uint64_t* get_row(std::uint64_t index) const;

	public:
		bitmap_t() = default;

		void set_all() const;
		void set(std::uint64_t index) const;
		void clear(std::uint64_t index) const;

		std::uint8_t is_set(std::uint64_t index) const;

		void set_map_value(std::uint64_t* value);
		void set_map_value_count(std::uint64_t value_count);
	};
}
