#pragma once

#include <string>
#include <vector>
#include <type_traits>

// Utility functions for file operations
namespace FileUtility
{
	/**
	 * @brief Reads a text file and returns its content as a string.
	 * 
	 * @param filename Path to the file.
	 * @return Contents of the file.
	 */
	std::string readText(const std::string& filename);

	/**
	 * @brief Reads a binary file and returns its content as a byte vector.
	 * 
	 * @param filename Path to the file.
	 * @return Contents of the file as bytes.
	 */
	std::vector<char> readBinary(const std::string& filename);
};

// General utility functions
namespace Utility
{
	/**
	 * @brief Checks if a number is a power of two.
	 * 
	 * @param value The number to check.
	 * @return true if the number is a power of two, false otherwise.
	 */
	template<typename T>
	inline constexpr bool isPowerOfTwo(T value)
	{
		static_assert(std::is_integral_v<T>, "Type must be integral.");
		return value != 0 && (value & (value - 1)) == 0;
	}

	/**
	 * @brief Rounds a number to the nearest power of two.
	 * 
	 * @param value The number to round.
	 * @param POT Power of two value to round to.
	 * @return Number rounded to the nearest power of two.
	 */
	template<typename T>
	inline constexpr T roundToPowerOfTwo(T value, int POT)
	{
		static_assert(std::is_integral_v<T>, "Type must be integral.");
		return (value + POT - 1) & -POT;
	}

	/**
	 * @brief Calculates the number of mipmap levels given the dimensions.
	 * 
	 * @param width Width of the image/texture.
	 * @param height Height of the image/texture.
	 * @return Number of mipmap levels.
	 */
	template<typename T>
	inline constexpr T numMipmapLevels(T width, T height)
	{
		static_assert(std::is_integral_v<T>, "Type must be integral.");
		T levels = 1;
		while((width | height) >> levels) {
			++levels;
		}
		return levels;
	}

};
