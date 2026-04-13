#pragma once

#include <cstddef>
#include <vector>
#include <emmintrin.h>

#include "ISingleton.h"
#include "_ProjMacro.h"
#include "_FastWcMappedFile.h"
#include "_FastWcArgParser.h"

#ifndef UNICODE 
#define UNICODE
#endif 

namespace core {
	class _FastWcInternalClass : public ISingleton<_FastWcInternalClass>
	{
		friend class ISingleton<_FastWcInternalClass>;
	public: 
		
		void wc();
		void blazinglyFastWc();
		void initClass(int argc, char** argv);

		__FORCE_INLINE std::size_t wcWord(std::size_t fileIndex = 0);
		__FORCE_INLINE std::size_t wcLine(std::size_t f_idx) noexcept;
		__FORCE_INLINE std::size_t wcCharC(size_t f_idx);
		__FORCE_INLINE std::size_t wcCharM(std::size_t f_idx) noexcept;

		__FORCE_INLINE std::size_t getTotalWord() const noexcept;

		__FORCE_INLINE std::size_t getTotalLine() const noexcept;

		__FORCE_INLINE std::size_t getTotalChar() const noexcept;

		__FORCE_INLINE std::size_t getTotalBytes() const noexcept;
		
		__FORCE_INLINE void printTotal() const noexcept;

	private: 
		_FastWcInternalClass() {
			_argParser.add_argument("-l", "--lines")
				.action([&](const auto&) { countLine = true; })
				.help("count lines")
				.flag();

			_argParser.add_argument("-w", "--words")
				.action([&](const auto&) { countWord = true; })
				.help("count words")
				.flag();

			_argParser.add_argument("-c", "--chars")
				.action([&](const auto&) { countChar = true; })
				.help("count characters")
				.flag();

			_argParser.add_argument("-b", "--bytes")
				.action([&](const auto&) { countByte = true; })
				.help("count bytes")
				.flag();

			_argParser.add_argument("files")
				.remaining();
		};

		int argc{};
		std::vector<std::string> argv;

		bool countLine = false;
		bool countWord = false;
		bool countChar = false;
		bool countByte = false;

		size_t maxBytesWidth{};
		size_t maxLinesWidth{};
		size_t maxWordsWidth{};
		size_t maxCharsWidth{};

		size_t totalBytes{};
		size_t totalLines{};
		size_t totalWords{};
		size_t totalChars{};

		argparse::ArgumentParser _argParser{ "fast-wc" };
		
		std::vector<fs::_FastWcMappedFile> _mappedFile;
	
		void parseArgv(int argc, char** argv);
	};
}