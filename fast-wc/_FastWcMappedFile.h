#pragma once

#include <string>
#include <string_view>

#include <Windows.h>
#include <span>

#include "_ProjMacro.h"

namespace fs {
#ifdef __cplusplus
	// Can use typed enum class, 
	// limit memory allocation to short
	enum class _FastWcMappedFileMode : short {
		UseMapping,
		BytesOnly,
		DefaultValue
	};
#else
	extern "C" {
		// Fallback to enum types
		enum _FastWcMappedFileMode {
			UseMapping,				// 0x0000
			BytesOnly,				// 0x0001
			DefaultValue			// 0x0002
		};
	}
#endif 
	// Mapped file class, which is used to read file content directly from memory, 
	// and the file content is automatically released when the object is destroyed.
	// This can cause some performance improvement since we can avoid the overhead of file I/O and memory copying.
	// TODO
	struct _FastWcMappedFile
	{
		void* data = nullptr;
		size_t dataSize{};

		HANDLE hFile = INVALID_HANDLE_VALUE;
		HANDLE hMapping = INVALID_HANDLE_VALUE;

		std::string filenameInfo;
		static std::once_flag _checkValidityFlag;
		_FastWcMappedFileMode mode{ _FastWcMappedFileMode::UseMapping };

		size_t wordCount{};
		size_t lineCount{};
		size_t charCount{};
		size_t bytesCount{};

		bool isStdIn = false;

		[[nodiscard]]
		__FORCE_INLINE bool valid() const noexcept;
		__FORCE_INLINE std::size_t size() const noexcept;
		__FORCE_INLINE std::string filename() const noexcept;

		__FORCE_INLINE void setWordCnt(std::size_t w_cnt) noexcept;
		__FORCE_INLINE void setLineCnt(std::size_t l_cnt) noexcept;
		__FORCE_INLINE void setCharCnt(std::size_t c_cnt) noexcept;
		__FORCE_INLINE void setBytesCnt(std::size_t b_cnt) noexcept;

		std::size_t getWordCnt() const noexcept;
		std::size_t getLineCnt() const noexcept;
		std::size_t getCharCnt() const noexcept;
		std::size_t getBytesCnt() const noexcept;

		using DynamicExtent = std::integral_constant<std::size_t, std::dynamic_extent>;

		[[nodiscard]]
		__FORCE_INLINE std::span<const char, std::dynamic_extent> as_span() const noexcept;
		__FORCE_INLINE std::span<const char> __weak_span() const noexcept;

		explicit _FastWcMappedFile();
		explicit _FastWcMappedFile(const std::string& filename);
		explicit _FastWcMappedFile(std::string_view filename, _FastWcMappedFileMode mode);
		_FastWcMappedFile(_FastWcMappedFile&& other) noexcept;
		_FastWcMappedFile& operator=(_FastWcMappedFile&& other) noexcept;

		char operator[](std::size_t index) const;

		~_FastWcMappedFile();
		_FastWcMappedFile(const _FastWcMappedFile&) = delete;
		_FastWcMappedFile& operator=(const _FastWcMappedFile&) = delete;
	};
}

