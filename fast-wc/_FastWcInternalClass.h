#pragma once

#include <cstddef>
#include <vector>
#include <emmintrin.h>

#include "ISingleton.h"
#include "_ProjMacro.h"
#include "_FastWcMappedFile.h"
#include "_FastWcArgParser.h"
#include "_FastWcCapabilityToken.h"

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

		void printHeader() const;
		__FORCE_INLINE void printTotal() const noexcept;

	private:
		__FORCE_INLINE _FastWcInternalClass() {
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

			_argParser.add_argument("-m", "--bytes")
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

		std::size_t maxBytesWidth{};
		std::size_t maxLinesWidth{};
		std::size_t maxWordsWidth{};
		std::size_t maxCharsWidth{};

		std::size_t totalBytes{};
		std::size_t totalLines{};
		std::size_t totalWords{};
		std::size_t totalChars{};

		bool _taskFinished = false;
		argparse::ArgumentParser _argParser{ "fast-wc" };

		std::vector<fs::_FastWcMappedFile> _mappedFile;
		tp::_FastWcTokenAuthority _tokenAuthority;

		static std::once_flag taskFinishedFlag;
		void parseArgv(int argc, char** argv);
	};
}