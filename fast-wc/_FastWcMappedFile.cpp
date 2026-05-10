#include <iostream>
#include <vector>
#include "_FastWcMappedFile.h"
#include "_FastWcErrorDisplay.h"

std::once_flag fs::_FastWcMappedFile::_checkValidityFlag;

fs::_FastWcMappedFile::_FastWcMappedFile()
	: filenameInfo(""), isStdIn(true), mode(_FastWcMappedFileMode::UseMapping)
{
	std::vector<char> buffer;
	constexpr size_t chunk_size = 8192; // 8KB chunks
	char temp_buf[chunk_size];

	while (std::cin.read(temp_buf, chunk_size) || std::cin.gcount() > 0) {
		buffer.insert(buffer.end(), temp_buf, temp_buf + std::cin.gcount());
	}

	dataSize = buffer.size();
	if (dataSize > 0)
	{
		// Allocate memory and copy data
		data = malloc(dataSize);
		if (data) {
			std::memcpy(data, buffer.data(), dataSize);
		}
		else {
			dataSize = 0;
		}
	}
}

fs::_FastWcMappedFile::_FastWcMappedFile(const std::string& filename)
	: filenameInfo(filename), isStdIn(false)
{
	hFile = CreateFileA(filename.c_str(), FILE_READ_ACCESS, NULL, NULL, NULL, NULL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		_FastWcErrorDisplay("mm_file constructor(): CreateFileA");
		std::abort();
	}
}

fs::_FastWcMappedFile::_FastWcMappedFile(std::string_view filename, _FastWcMappedFileMode mode)
{
	if (mode == _FastWcMappedFileMode::UseMapping) {
		filenameInfo = std::string(filename);

		hFile = CreateFileA(filenameInfo.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (hFile == INVALID_HANDLE_VALUE)
		{
			_FastWcErrorDisplay(TEXT("CreateFileA"));
			return;
		}

		LARGE_INTEGER fileSize;

		if (!GetFileSizeEx(hFile, &fileSize))
		{
			_FastWcErrorDisplay(TEXT("GetFileSizeEx"));
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			return;
		}

		dataSize = static_cast<size_t>(fileSize.QuadPart);

		if (dataSize == 0)
		{
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			return;
		}

		hMapping = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);

		if (hMapping == nullptr)
		{
			_FastWcErrorDisplay(TEXT("CreateFileMappingA"));
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			return;
		}

		data = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);

		if (data == nullptr)
		{
			_FastWcErrorDisplay(TEXT("MapViewOfFile"));
			CloseHandle(hMapping);
			CloseHandle(hFile);
			hMapping = INVALID_HANDLE_VALUE;
			hFile = INVALID_HANDLE_VALUE;
			return;
		}
	}
	else {
		hFile =
			CreateFileA(filenameInfo.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			_FastWcErrorDisplay(TEXT("CreateFileA"));
			return;
		}

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(hFile, &fileSize))
		{
			_FastWcErrorDisplay(TEXT("GetFileSizeEx"));
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			return;
		}

		dataSize = static_cast<std::size_t>(fileSize.QuadPart);
		if (dataSize == 0)
		{
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			return;
		}
	}

	mode = mode;
}

fs::_FastWcMappedFile::_FastWcMappedFile(_FastWcMappedFile&& other) noexcept
	: data(other.data), dataSize(other.dataSize), hFile(other.hFile), hMapping(other.hMapping), filenameInfo(std::move(other.filenameInfo)), mode(other.mode),
	wordCount(other.wordCount), lineCount(other.lineCount), charCount(other.charCount), bytesCount(other.bytesCount), isStdIn(other.isStdIn)
{
	other.data = nullptr;
	other.dataSize = 0;
	other.hFile = INVALID_HANDLE_VALUE;
	other.hMapping = INVALID_HANDLE_VALUE;
}

fs::_FastWcMappedFile& fs::_FastWcMappedFile::operator=(_FastWcMappedFile&& other) noexcept
{
	if (this != &other)
	{
		if (valid()) {
			if (mode == _FastWcMappedFileMode::UseMapping) {
				if (data) {
					UnmapViewOfFile(data);
				}
				if (hMapping != INVALID_HANDLE_VALUE) {
					CloseHandle(hMapping);
				}
			}
			else {
				if (hFile != INVALID_HANDLE_VALUE) {
					CloseHandle(hFile);
				}
			}
		}

		data = other.data;
		dataSize = other.dataSize;
		hFile = other.hFile;
		hMapping = other.hMapping;
		filenameInfo = std::move(other.filenameInfo);
		mode = other.mode;
		wordCount = other.wordCount;
		lineCount = other.lineCount;
		charCount = other.charCount;
		bytesCount = other.bytesCount;
		isStdIn = other.isStdIn;
		other.data = nullptr;
	}

	return *this;
}

char fs::_FastWcMappedFile::operator[](std::size_t __idx) const
{
	if (__idx >= 0 && __idx < this->size())
	{
		_FastWcErrorDisplay("operator[] : index");
		std::abort();
	}

	std::call_once(_checkValidityFlag, [&]() {
		if (!valid())
		{
			_FastWcErrorDisplay("operator[] : index");
			std::abort();
		}
		});

	auto data = this->as_span();
	auto* _data = data.data();

	return _data[__idx];
}

fs::_FastWcMappedFile::~_FastWcMappedFile()
{
	if (valid()) {
		if (isStdIn && data != nullptr) {
			free(data);
		}
		else {
			if (data != nullptr) {
				UnmapViewOfFile(data);
			}
		}
	}
}

/**
   * @brief Returns the file contents as a read-only span.
   * @return std::span<const char> Span over the mapped data, or empty if
   * BytesOnly mode or data is null.
   */
[[nodiscard]] __FORCE_INLINE std::span<const char> fs::_FastWcMappedFile::as_span() const noexcept {
	if (mode == _FastWcMappedFileMode::BytesOnly || data == nullptr)
		return {};
	return { static_cast<const char*>(data), dataSize };
}

/**
 * @brief Checks if the mapped file object is valid.
 * @return true if mapped data exists or size is non-zero, false otherwise.
 */
[[nodiscard]] __FORCE_INLINE bool fs::_FastWcMappedFile::valid() const noexcept {
	return (mode == _FastWcMappedFileMode::UseMapping) ? data != nullptr : dataSize > 0;
}

/**
 * @brief Returns the size of the mapped data.
 * @return size_t Size in bytes.
 */
[[nodiscard]] __FORCE_INLINE std::size_t fs::_FastWcMappedFile::size() const noexcept { return dataSize; }

/**
 * @brief Returns the filename associated with the mapped file.
 * @return std::string Copy of the filename.
 */
[[nodiscard]] __FORCE_INLINE std::string fs::_FastWcMappedFile::filename() const noexcept {
	return filenameInfo;
}

__FORCE_INLINE void fs::_FastWcMappedFile::setWordCnt(std::size_t w_cnt) noexcept { wordCount = w_cnt; }
__FORCE_INLINE void fs::_FastWcMappedFile::setLineCnt(std::size_t l_cnt) noexcept { lineCount = l_cnt; }
__FORCE_INLINE void fs::_FastWcMappedFile::setCharCnt(std::size_t c_cnt) noexcept { charCount = c_cnt; }
__FORCE_INLINE void fs::_FastWcMappedFile::setBytesCnt(std::size_t b_cnt) noexcept { bytesCount = b_cnt; }

[[nodiscard]] std::size_t fs::_FastWcMappedFile::getWordCnt() const noexcept { return wordCount; }
[[nodiscard]] std::size_t fs::_FastWcMappedFile::getLineCnt() const noexcept { return lineCount; }
[[nodiscard]] std::size_t fs::_FastWcMappedFile::getCharCnt() const noexcept { return charCount; }
[[nodiscard]] std::size_t fs::_FastWcMappedFile::getBytesCnt() const noexcept { return bytesCount; }