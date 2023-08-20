#pragma once

#include <memory>
#include <string>

class Image
{
public:
	static std::shared_ptr<Image> fromFile(const std::string& filename, int channels=4);

	int width() const { return m_width; }
	int height() const { return m_height; }
	int channels() const { return m_channels; }
	int bytesPerPixel() const { return m_channels * (m_hdr ? sizeof(float) : sizeof(unsigned char)); }
	int pitch() const { return m_width * bytesPerPixel(); }

	bool isHDR() const { return m_hdr; }

	template<typename T>
	const T* pixels() const
	{
		static_assert(std::is_same<T, unsigned char>::value || std::is_same<T, float>::value, 
                      "Image::pixels can only return unsigned char or float");
		return reinterpret_cast<const T*>(m_pixels.get());
	}

	Image();
private:

	int m_width;
	int m_height;
	int m_channels;
	bool m_hdr;
	std::unique_ptr<unsigned char[]> m_pixels;  // Changed to unique_ptr to an array
};
