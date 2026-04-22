#include <iostream>
#include <iomanip>
#include <algorithm>
#include <array>
#include <string> // add near other includes

#include "_FastWcInternalClass.h"
#include "_FastWcErrorDisplay.h"
#include "_FastWcUtil.h"
#include "_FastWcThreadPool.h"
#include "_FastWcConsoleColor.h"

namespace core {
    std::once_flag _FastWcInternalClass::taskFinishedFlag;

    void _FastWcInternalClass::parseArgv(int argc, char** argv)
    {
        _argParser.parse_args(argc, argv);

        // Determine mode AFTER parse_args, since the action lambdas
        // set the count* booleans during parse_args itself.
        fs::_FastWcMappedFileMode mode = fs::_FastWcMappedFileMode::UseMapping;

        if (!countByte && !countLine && !countWord && !countChar) {
            countByte = countLine = countWord = countChar = true;
        } else if (!countLine && !countWord && !countChar) {
            mode = fs::_FastWcMappedFileMode::BytesOnly;
        }

        _mappedFile.clear();

        try {
            this->argv = _argParser.get<std::vector<std::string>>("files");
        }
        catch (...) {
            this->argv.clear();
        }

        for (const auto& filename : this->argv)
        {
            if (filename == "-") {
                // "-" means read from stdin
                _mappedFile.push_back(fs::_FastWcMappedFile());
            }
            else {
                _mappedFile.push_back(fs::_FastWcMappedFile(filename, mode));
            }
        }

        if (this->argv.empty())
        {
            _mappedFile.push_back(fs::_FastWcMappedFile());
        }
    }

	void _FastWcInternalClass::wc()
	{
        std::size_t var{};

        for (std::size_t i = 0; i < _mappedFile.size(); ++i) {
            if (countLine) {
                var = wcLine(i);
                _mappedFile[i].setLineCnt(var);
                maxLinesWidth = (((maxLinesWidth) > (util::intWidth(var))) ? (maxLinesWidth) : (util::intWidth(var)));
                totalLines += var;
            }
            if (countWord) {
                var = wcWord(i);
                _mappedFile[i].setWordCnt(var);
                maxWordsWidth = (((maxWordsWidth) > (util::intWidth(var))) ? (maxWordsWidth) : (util::intWidth(var)));
                totalWords += var;
            }
            if (countByte) {
                var = wcCharC(i);
                _mappedFile[i].setBytesCnt(var);
                maxBytesWidth = (((maxBytesWidth) > (util::intWidth(var))) ? (maxBytesWidth) : (util::intWidth(var)));
                totalBytes += var;
            }
            if (countChar) {
                var = wcCharM(i);
                _mappedFile[i].setCharCnt(var);
                maxCharsWidth = (((maxCharsWidth) > (util::intWidth(var))) ? (maxCharsWidth) : (util::intWidth(var)));
                totalChars += var;
            }
        }
	}

    //template <class Translation>
    void _FastWcInternalClass::blazinglyFastWc()
    {
		auto* pool = tp::_FastWcThreadPool::Instance(tp::hardware_concurrency());
		const std::size_t numFiles = _mappedFile.size();
        const std::size_t numThread = pool->threadCount(); 
        
        // Handle single file case, no parallelization needed
        // (Can be a performance skyrocket)
        if (numFiles == 1) {
            std::size_t var{};
            if (countLine) {
                var = wcLine(0);
                _mappedFile[0].setLineCnt(var);
                totalLines = var;
                maxLinesWidth = util::intWidth(var);
            }
            if (countWord) {
                var = wcWord(0);
                _mappedFile[0].setWordCnt(var);
                totalWords = var;
                maxWordsWidth = util::intWidth(var);
            }
            if (countByte) {
                var = wcCharC(0);
                _mappedFile[0].setBytesCnt(var);
                totalBytes = var;
                maxBytesWidth = util::intWidth(var);
            }
            if (countChar) {
                var = wcCharM(0);
                _mappedFile[0].setCharCnt(var);
                totalChars = var;
                maxCharsWidth = util::intWidth(var);
            }

            return;
        }

        // Per-thread accumulators to minimize atomic contention.
        // Without thread_local, this implementation can be a candidate
        // for possible data race but it was not proved yet.
        // Multiple workers may write to same accumulator
        // which implies data race / false sharing.
        //
        // Even though total_XXXX are already declared as private members of
        // __wc_internal_class, this struct keeps track of result from workers.
        // At the end, its content will be affected to the corresponding value.
        struct alignas(64) ThreadLocalAccumulator {
            std::size_t total_line   = 0;    // total number of line
            std::size_t total_word   = 0;    // total number of word
            std::size_t total_bytes  = 0;    // total number of bytes
            std::size_t total_char   = 0;    // total number of char

            // To have better formatting, we have to
            // keep track of the max width for each result.
            // then pass that with setw in std::cout streams.
            std::size_t max_line_width = 0;
            std::size_t max_word_width = 0;
            std::size_t max_bytes_width = 0;
            std::size_t max_char_width = 0;
        };

        std::vector<ThreadLocalAccumulator> accumulators(numThread);
        std::vector<std::future<void>> futures;
        futures.reserve(numFiles);

        // Determine optimal chunk size for work distribution
        const std::size_t min_chunk_size = 1;
        const std::size_t max_chunk_size = 10;
        const std::size_t chunk_size =
            std::clamp(numFiles / numThread, min_chunk_size, max_chunk_size);

        for (std::size_t chunk_start = 0; chunk_start < numFiles; chunk_start += chunk_size) {
            std::size_t chunk_end = std::min(chunk_start + chunk_size, numFiles);
            std::size_t thread_idx = (chunk_start / chunk_size) % numThread;
            
            futures.push_back(pool->submit(
                [&, chunk_start, chunk_end, thread_idx]() {
                    auto& acc = accumulators[thread_idx];
                    for (std::size_t i = chunk_start; i < chunk_end; ++i) {
                        std::size_t var{};
                        if (countLine) {
                            var = wcLine(i);
                            _mappedFile[i].setLineCnt(var);
                            acc.total_line += var;
                            acc.max_line_width =
                                std::max(acc.max_line_width, util::intWidth(var));
                        }
                        if (countWord) {
                            var = wcWord(i);
                            _mappedFile[i].setWordCnt(var);
                            acc.total_word += var;
                            acc.max_word_width =
                                std::max(acc.max_word_width, util::intWidth(var));
                        }

                        if (countByte) {
                            var = wcCharC(i);
                            _mappedFile[i].setBytesCnt(var);
                            acc.total_bytes += var;
                            acc.max_bytes_width =
                                std::max(acc.max_bytes_width, util::intWidth(var));
                        }

                        if (countChar) {
                            var = wcCharM(i);
                            _mappedFile[i].setCharCnt(var);
                            acc.total_char += var;
                            acc.max_char_width =
                                std::max(acc.max_char_width, util::intWidth(var));
                        }
                    }
                }));
        }

        // Wait for all tasks
        for (auto& future : futures) {
            future.get();
        }

        // When all tasks is finished, which means
        // accumulator has already the final value, we reduce
        // the runtime to a single-threaded one.
        totalLines = 0;
        totalWords = 0;
        totalBytes = 0;
        totalChars = 0;
        maxLinesWidth = 0;
        maxWordsWidth = 0;
        maxBytesWidth = 0;
		maxCharsWidth = 0;

		_taskFinished = true;

        for (const auto& acc : accumulators) {
            totalLines += acc.total_line;
            totalWords += acc.total_word;
            totalBytes += acc.total_bytes;
            totalChars += acc.total_char;

            maxLinesWidth = std::max(maxLinesWidth, acc.max_line_width);
            maxWordsWidth = std::max(maxWordsWidth, acc.max_word_width);
            maxBytesWidth = std::max(maxBytesWidth, acc.max_bytes_width);
            maxCharsWidth = std::max(maxCharsWidth, acc.max_char_width);
        }
    }

    void _FastWcInternalClass::initClass(int argc, char** argv)
    {
        parseArgv(argc, argv);
    }

	

#ifdef DUFF_DEVICE 
	// Switch implementation to use Duff's device for the tail processing of wcWord.
	// TODO: Benchmark this against the current implementation to see if it provides a performance boost.

#define CAST_UCHAR(c) (static_cast<unsigned char>(c))
#define IS_SPACE(c) (kIsSpace[CAST_UCHAR(c)])
    using _BoolArray = std::array<bool, 256>;

    static constexpr _BoolArray make_kIsSpace() {
        _BoolArray __local_array{};
        __local_array[CAST_UCHAR(' ')]  = true;
        __local_array[CAST_UCHAR('\t')] = true;
        __local_array[CAST_UCHAR('\r')] = true;
        __local_array[CAST_UCHAR('\n')] = true;
        return __local_array;
    }

	// Check if a given character is a whitespace character (space, tab, carriage return, newline).
    alignas(64) static constexpr _BoolArray kIsSpace = make_kIsSpace();

    // The repeated body — keeps Duff's switch readable
#define WORD_STEP()                                             \
    do {                                                        \
        const bool s = kIsSpace[(unsigned char)data[i++]];      \
        wCount += ((!inWord) & (!s));                           \
        inWord = !s;                                            \
    } while(0)

#endif // DUFF_DEVICE
    
    std::size_t _FastWcInternalClass::wcWord(std::size_t fileIndex)
    {
        if (_mappedFile.empty() || !_mappedFile[fileIndex].valid())
        {
            _FastWcErrorDisplay(TEXT("_mappedFile: file is empty or index is invalid"));
            ExitProcess(EXIT_FAILURE);  // Hard exit, no unwinding
        }

        auto data = _mappedFile[fileIndex].as_span();
        if (data.size() == 0)
        {
            _FastWcErrorDisplay(TEXT("_mappedFile: mapped region has zero size"));
            ExitProcess(EXIT_FAILURE);
        }
        
        std::size_t wCount = 0;
        std::size_t i = 0;

#ifdef DUFF_DEVICE
		bool inWord = false;

        std::size_t n = (data.size() + 7) / 8;   // ceil(size / 8) iterations

        // Jump directly into the unrolled body to handle the remainder first,
        // then spin through full 8-step chunks until exhausted.
        switch (data.size() % 8)
        {
        case 0: do {
            WORD_STEP();
        case 7:      WORD_STEP();
        case 6:      WORD_STEP();
        case 5:      WORD_STEP();
        case 4:      WORD_STEP();
        case 3:      WORD_STEP();
        case 2:      WORD_STEP();
        case 1:      WORD_STEP();
            } while (--n > 0); // one branch per 8 bytes instead of 1 per byte
        }
#else 
        // prevWasSpace=true so a word at offset 0 gets counted
        std::uint32_t prevLastBit = 1u;
		auto dataSize = data.size();

        if (dataSize >= 32)
        {
            const __m256i vSpace    = _mm256_set1_epi8(' ');
            const __m256i vTab      = _mm256_set1_epi8('\t');
            const __m256i vCR       = _mm256_set1_epi8('\r');
            const __m256i vLF       = _mm256_set1_epi8('\n');

            for (; i + 32 <= dataSize; i += 32)
            {
                __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data.data() + i));

                // Build a whitespace mask: 0xFF per byte if whitespace, else 0x00
                __m256i isSpace = _mm256_or_si256(
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk, vSpace),
                    _mm256_cmpeq_epi8(chunk, vTab)),
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk, vCR),
                    _mm256_cmpeq_epi8(chunk, vLF))
                );

                // Collapse to 1 bit per byte
                const std::uint32_t spaceMask = static_cast<std::uint32_t>(_mm256_movemask_epi8(isSpace));

                // Shift in the previous chunk's last bit to detect cross-chunk boundaries
                // prevSpaceMask[i] = spaceMask[i-1], with bit 0 = last char of previous chunk
                const std::uint64_t shifted = (static_cast<std::uint64_t>(spaceMask) << 1) | prevLastBit;
                const std::uint32_t prevSpaceMask = static_cast<std::uint32_t>(shifted);

                const std::uint32_t wordStarts = prevSpaceMask & ~spaceMask;
                wCount += std::popcount(wordStarts);

                prevLastBit = (spaceMask >> 31) & 1u;
            }
        }

        bool inWord = (prevLastBit == 0);
        for (; i < dataSize; ++i) 
        {
            const unsigned char c = data[i];
            const bool isSpace = (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
            wCount += (!inWord && !isSpace);
            inWord = !isSpace;
        }
#endif // DUFF_DEVICE
        return wCount;
    }

#ifdef __AVX512F__
        // Using AVX512
        // Slightly better (Use inline assembly)

        /**
         * @brief Counts lines in a mapped file using AVX512 SIMD instructions.
         *
         * Processes 64 bytes at a time, comparing each byte to `'\n'` using
         * `_mm512_cmpeq_epi8_mask` and counting set bits. Falls back to scalar
         * counting for the remaining bytes. Extremely fast for large files.
         *
         * @param translation Optional translation/mapping functor (default:
         * identity).
         * @param f_idx Index of the mapped file to process (default: 0).
         * @return size_t Total number of newline characters in the file.
         *
         * @note Requires CPU with AVX512F support. Returns 0 if the mapped file
         *       is invalid or empty.
         */
        __FORCE_INLINE std::size_t _FastWcInternalClass::wcLine(size_t f_idx = 0) noexcept {
        if (_mappedFile.empty() || !_mappedFile[f_idx].valid()) {
            return 0;
        }

        size_t __l_count{};
        auto data = mapped_file[f_idx].as_span();
        const char* ptr = data.data();
        const char* end = ptr + data.size();
        const __m512i newline = _mm512_set1_epi8('\n');

        // Process 64 bytes at a time
        // If a newline is found on the n-th bits,
        // it is switched to one, else every non newline bits is
        // set to 0. That's what the function _mm512_cmpeq_epi8_mask() does.

        while (ptr + 64 <= end) [[likely]] {
            __builtin_prefetch(ptr + 128, 0, 0);
            __m512i chk =
                _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));
            __mmask64 __m = _mm512_cmpeq_epi8_mask(chk, newline);

            // Count the number of set bits (population count)
            // for data type unsigned long long
            __l_count += __builtin_popcountll(__m);
            ptr += 64;
        }

        // Handle remainder (Because
        // every input is not forcefully with 0 mod 64.
        while (ptr < end) [[likely]] {
            __l_count += (*ptr++ == '\n');
        }

        return __l_count;
    }

#elif defined(__AVX2__)
    /**
     * @brief Counts lines in a mapped file using AVX2 SIMD instructions.
     *
     * Processes 32 bytes at a time, comparing each byte to `'\n'` using
     * `_mm256_cmpeq_epi8` and `_mm256_movemask_epi8`, then counts set bits
     * with `std::popcount`. Remaining bytes are counted with a scalar loop.
     *
     * @param translation Optional translation/mapping functor (default:
     * identity).
     * @param f_idx Index of the mapped file to process (default: 0).
     * @return size_t Total number of newline characters in the file, 0 if
     *         the mapped file is empty or invalid.
     *
     * @note Requires CPU with AVX2 support.
     */
    [[gnu::target("avx2")]]
    __FORCE_INLINE std::size_t _FastWcInternalClass::wcLine(size_t f_idx = 0) noexcept {
        if (_mappedFile.empty() || !_mappedFile[f_idx].valid()) {
            std::cerr << "Mapped file is empty" << std::endl;
            return 0;
        }
        std::size_t lineCount{};
        auto data = _mappedFile[f_idx].as_span();
        const char* ptr = data.data();
        const char* end = ptr + data.size();
        const __m256i newline = _mm256_set1_epi8('\n');
        std::size_t simd_iterations = 0;
        std::size_t simd_newlines = 0;

        // Same logic as with __AVX512F__
        // Just different instruction set
        // More high level and understandable
        while (ptr + 32 <= end) [[likely]]
        {
            // __builtin_prefetch ( ptr + 128, 0, 3 );  // TEMPORARILY DISABLED
            __m256i __chk =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
            __m256i __cmp = _mm256_cmpeq_epi8(__chk, newline);
            int __m = _mm256_movemask_epi8(__cmp);
            int count = std::popcount(static_cast<uint32_t>(__m));

            lineCount += count;
            simd_newlines += count;
            simd_iterations++;
            ptr += 32;
        }

        // Handle remainder
        size_t rem = 0;
        while (LIKELY(ptr < end)) {
            rem += (*ptr++ == '\n');
        }

        lineCount += rem;
        return lineCount;
    }

#elif _M_IX86_FP == 2
    //  SSE is the newest instruction set for
    //  x86-64 CPU.

    /**
     * @brief Counts lines in a mapped file using SSE2 SIMD instructions.
     *
     * Processes 16 bytes at a time, comparing each byte to `'\n'` using
     * `_mm_cmpeq_epi8` and `_mm_movemask_epi8`, then counts set bits with
     * `std::popcount`. Remaining bytes are counted with a scalar loop.
     *
     * @param translation Optional translation/mapping functor (default:
     * identity).
     * @param f_idx Index of the mapped file to process (default: 0).
     * @return size_t Total number of newline characters in the file, or 0 if
     *         the mapped file is empty or invalid.
     *
     * @note Requires CPU with SSE2 support.
     */
    __SSE2_TARGET
    __FORCE_INLINE std::size_t _FastWcInternalClass::wcLine(std::size_t f_idx = 0) noexcept {
        if (_mappedFile.empty() || !_mappedFile[f_idx].valid()) {
            return 0;
        }

        std::size_t lineCount{};
        auto data = _mappedFile[f_idx].as_span();
        const char* ptr = data.data();
        const char* end = ptr + data.size();
        const __m128i newline = _mm_set1_epi8('\n');

        while (ptr + 16 <= end) [[likely]]
        {
            __builtin_prefetch(ptr + 64, 0, 3);
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
            __m128i cmp = _mm_cmpeq_epi8(chunk, newline);
            int mask = _mm_movemask_epi8(cmp);
            lineCount += std::popcount(static_cast<uint32_t>(mask));
            ptr += 16;
        }

        while (ptr < end) [[likely]] {
            lineCount += (*ptr++ == '\n');
        }

        return lineCount;
    }

#else // Fallback to a scalar implementation

    /**
     * @brief Counts lines in a mapped file using loop unrolling.
     *
     * Processes 32 bytes at a time with manual unrolling to count newline
     * characters (`'\n'`) efficiently, then handles any remaining bytes
     * with a scalar loop. This is a fast, portable alternative to SIMD.
     *
     * @param f_idx Index of the mapped file to process (default: 0).
     * @return size_t Total number of newline characters in the file.
     *
     * @note Optimized for large files; works on all architectures.
     */
    __FORCE_INLINE std::size_t _FastWcInternalClass::wcLine(std::size_t f_idx) noexcept {
        std::size_t lineCount{};
        auto data = _mappedFile[f_idx].as_span();
        const char* ptr = data.data();
        const char* end = ptr + data.size();

        // Process 32 bytes at a time (Ah the mf)
        while (ptr + 32 <= end) [[likely]] {
            lineCount += (ptr[0] == '\n') + (ptr[1] == '\n') + (ptr[2] == '\n') +
                (ptr[3] == '\n') + (ptr[4] == '\n') + (ptr[5] == '\n') +
                (ptr[6] == '\n') + (ptr[7] == '\n') + (ptr[8] == '\n') +
                (ptr[9] == '\n') + (ptr[10] == '\n') + (ptr[11] == '\n') +
                (ptr[12] == '\n') + (ptr[13] == '\n') + (ptr[14] == '\n') +
                (ptr[15] == '\n') + (ptr[16] == '\n') + (ptr[17] == '\n') +
                (ptr[18] == '\n') + (ptr[19] == '\n') + (ptr[20] == '\n') +
                (ptr[21] == '\n') + (ptr[22] == '\n') + (ptr[23] == '\n') +
                (ptr[24] == '\n') + (ptr[25] == '\n') + (ptr[26] == '\n') +
                (ptr[27] == '\n') + (ptr[28] == '\n') + (ptr[29] == '\n') +
                (ptr[30] == '\n') + (ptr[31] == '\n');
            ptr += 32;
        }

        // Handle remainder
        while (ptr < end) {
            lineCount += (*ptr++ == '\n');
        }

        return lineCount;
    }

#endif // __AVX512F__

    /**
   * @brief Returns the size in bytes of a mapped file.
   *
   * Uses the pre-mapped file size directly, making it extremely fast.
   * Suitable for counting bytes (e.g., `-c` option). Returns 0 if the
   * file is invalid or the mapped file array is empty.
   *
   * @param translation Optional translation/mapping functor (default:
   * identity).
   * @param f_idx Index of the mapped file to query (default: 0).
   * @return size_t File size in bytes, or 0 if invalid.
   */
    size_t _FastWcInternalClass::wcCharC(size_t f_idx) {
        if (_mappedFile.empty() || !_mappedFile[f_idx].valid()) 
        {
            return 0;
        }

        return _mappedFile[f_idx].size();
    }

#ifdef __AVX512F__
    /**
     * @brief Counts UTF-8 characters in a mapped file using AVX512F SIMD.
     *
     * Processes 64 bytes at a time, detecting UTF-8 character starts by
     * identifying non-continuation bytes (bytes not matching 10xxxxxx).
     * Remaining bytes are counted with a scalar loop.
     *
     * @param translation Optional translation/mapping functor (default:
     * identity).
     * @param f_idx Index of the mapped file to process (default: 0).
     * @return size_t Total number of UTF-8 characters in the file, 0 if
     *         the mapped file is empty or invalid.
     *
     * @note Requires CPU with AVX512F support.
     */
    __FORCE_INLINE std::size_t _FastWcInternalClass::wcCharM(std::size_t f_idx = 0) noexcept {
        if (_mappedFile.empty() || !_mappedFile[f_idx].valid()) {
            return 0;
        }

        auto data = _mappedFile[f_idx].as_span();
        const char* ptr = data.data();
        const char* end = ptr + data.size();
        std::size_t charCount = 0;

        // UTF-8 continuation bytes: 10xxxxxx (top 2 bits = 10)
        const __m512i continuationMask = _mm512_set1_epi8(0xC0);    // 11000000
        const __m512i continuationPattern = _mm512_set1_epi8(0x80); // 10000000

        while (ptr + 64 <= end) {
            __builtin_prefetch(ptr + 128, 0, 0);
            __m512i chunk =
                _mm512_loadu_si512(reinterpret_cast<const __m512i*>(ptr));

            // Mask to get top 2 bits of each byte
            __m512i masked = _mm512_and_si512(chunk, continuationMask);

            // Compare with continuation pattern (10xxxxxx)
            __mmask64 is_continuation =
                _mm512_cmpeq_epi8_mask(masked, continuationPattern);

            // Count non-continuation bytes (these are character starts)
            charCount += 64 - __builtin_popcountll(is_continuation);
            ptr += 64;
        }

        // Handle remainder
        while (LIKELY(ptr < end)) {
            charCount += ((*ptr & 0xC0) != 0x80);
            ptr++;
        }

        return charCount;
    }

#elif defined(__AVX2__)
    /**
     * @brief Counts UTF-8 characters in a mapped file using AVX2 SIMD.
     *
     * Processes 32 bytes at a time, detecting UTF-8 character starts by
     * identifying non-continuation bytes (bytes not in 0x80–0xBF range).
     * Remaining bytes are counted using a scalar loop.
     *
     * @param translation Optional translation/mapping functor (default:
     * identity).
     * @param f_idx Index of the mapped file to process (default: 0).
     * @return size_t Total number of UTF-8 characters, 0 if file is
     * empty/invalid.
     *
     * @note Requires CPU with AVX2 support.
     */
    [[gnu::target("avx2")]]
    __FORCE_INLINE size_t __wc_char_m(Translation translation = std::identity{},
        size_t f_idx = 0) noexcept {
        if (mapped_file.empty() || !mapped_file[f_idx].valid()) {
            return 0;
        }

        auto __data = mapped_file[f_idx].as_span();
        const char* ptr = __data.data();
        const char* end = ptr + __data.size();
        size_t char_count = 0;

        // UTF-8 continuation bytes start with 10xxxxxx (0x80-0xBF)
        // We count all bytes that are NOT continuation bytes
        const __m256i continuation_mask = _mm256_set1_epi8(0xC0);    // 11000000
        const __m256i continuation_pattern = _mm256_set1_epi8(0x80); // 10000000

        while (LIKELY(ptr + 32 <= end)) {
            __m256i chunk =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));

            // Get top 2 bits: (byte & 11000000)
            __m256i masked = _mm256_and_si256(chunk, continuation_mask);

            // Compare with 10000000 (continuation bytes)
            __m256i is_continuation = _mm256_cmpeq_epi8(masked, continuation_pattern);

            // Get mask of continuation bytes
            int continuation_bits = _mm256_movemask_epi8(is_continuation);

            // Count non-continuation bytes (characters)
            // 32 total bytes - number of continuation bytes
            char_count +=
                32 - __builtin_popcount(static_cast<uint32_t>(continuation_bits));
            ptr += 32;
        }
        // Handle remainder
        while (LIKELY(ptr < end)) {
            // A byte is a character start if top 2 bits are NOT 10
            char_count += ((*ptr & 0xC0) != 0x80);
            ptr++;
        }

        return char_count;
    }

#elif _M_IX86_FP == 2
    /**
     * @brief Counts UTF-8 characters in a mapped file using SSE2 SIMD.
     *
     * Processes 16 bytes at a time, detecting UTF-8 character starts by
     * identifying non-continuation bytes (bytes not in 0x80–0xBF range).
     * Remaining bytes are counted using a scalar loop.
     *
     * @param translation Optional translation/mapping functor (default:
     * identity).
     * @param f_idx Index of the mapped file to process (default: 0).
     * @return size_t Total number of UTF-8 characters, 0 if file is empty or
     * invalid.
     *
     * @note Requires CPU with SSE2 support.
     */
    __SSE2_TARGET
    __FORCE_INLINE std::size_t _FastWcInternalClass::wcCharM(std::size_t f_idx = 0) {
        if (_mappedFile.empty() || !_mappedFile[f_idx].valid()) {
            return 0;
        }

        auto data = _mappedFile[f_idx].as_span();
        const char* ptr = data.data();
        const char* end = ptr + data.size();
        std::size_t charCount = 0;

        // Optimizations:
        // 1. Use a single mask and pattern for the entire loop
        const __m128i continuation_mask = _mm_set1_epi8(0xC0);
        const __m128i continuation_pattern = _mm_set1_epi8(0x80);

        // 2. Use a single _mm_cmpistri intrinsic to count the characters
        while (ptr + 16 <= end) [[likely]] {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
            int count = _mm_cmpistri(continuation_pattern, chunk, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY);
            charCount += 16 - count;
            ptr += 16;
        }

        // 3. Use a simple loop for the remaining bytes
        while (ptr < end) [[likely]] {
            charCount += ((*ptr & 0xC0) != 0x80);
            ptr++;
        }

        return charCount;
    }

#else // Scalar fallback
    /**
     * @brief Counts UTF-8 characters in a mapped file using unrolled loops.
     *
     * Processes 32 bytes at a time, treating any byte that is NOT a UTF-8
     * continuation byte (0x80–0xBF) as a character start.
     * Remainder bytes are handled with a scalar loop.
     *
     * @param translation Optional translation/mapping functor (default:
     * identity).
     * @param f_idx Index of the mapped file to process (default: 0).
     * @return size_t Total number of UTF-8 characters, 0 if file is
     * empty/invalid.
     *
     * @note This is a high-performance branch-unrolled implementation, portable
     *       across CPUs without SIMD support.
     */
    __FORCE_INLINE std::size_t _FastWcInternalClass::wcCharM(std::size_t f_idx) noexcept {
        if (_mappedFile.empty() || !_mappedFile[f_idx].valid()) {
            return 0;
        }

        auto data = _mappedFile[f_idx].as_span();
        const char* ptr = data.data();
        const char* end = ptr + data.size();
        size_t charCount = 0;

        // Process 32 bytes at a time
        while (ptr + 32 <= end)[[likely]] {
            charCount += ((ptr[0] & 0xC0) != 0x80) + ((ptr[1] & 0xC0) != 0x80) +
                ((ptr[2] & 0xC0) != 0x80) + ((ptr[3] & 0xC0) != 0x80) +
                ((ptr[4] & 0xC0) != 0x80) + ((ptr[5] & 0xC0) != 0x80) +
                ((ptr[6] & 0xC0) != 0x80) + ((ptr[7] & 0xC0) != 0x80) +
                ((ptr[8] & 0xC0) != 0x80) + ((ptr[9] & 0xC0) != 0x80) +
                ((ptr[10] & 0xC0) != 0x80) + ((ptr[11] & 0xC0) != 0x80) +
                ((ptr[12] & 0xC0) != 0x80) + ((ptr[13] & 0xC0) != 0x80) +
                ((ptr[14] & 0xC0) != 0x80) + ((ptr[15] & 0xC0) != 0x80) +
                ((ptr[16] & 0xC0) != 0x80) + ((ptr[17] & 0xC0) != 0x80) +
                ((ptr[18] & 0xC0) != 0x80) + ((ptr[19] & 0xC0) != 0x80) +
                ((ptr[20] & 0xC0) != 0x80) + ((ptr[21] & 0xC0) != 0x80) +
                ((ptr[22] & 0xC0) != 0x80) + ((ptr[23] & 0xC0) != 0x80) +
                ((ptr[24] & 0xC0) != 0x80) + ((ptr[25] & 0xC0) != 0x80) +
                ((ptr[26] & 0xC0) != 0x80) + ((ptr[27] & 0xC0) != 0x80) +
                ((ptr[28] & 0xC0) != 0x80) + ((ptr[29] & 0xC0) != 0x80) +
                ((ptr[30] & 0xC0) != 0x80) + ((ptr[31] & 0xC0) != 0x80);
            ptr += 32;
        }

        while (ptr < end) [[likely]] {
            charCount += ((*ptr++ & 0xC0) != 0x80);
        }

        return charCount;
    }

#endif // __AVX512F__

#define BLOCK_CHECK_WORKER_TERMINATION(func_name) \
    do {                                           \
        std::call_once(taskFinishedFlag, [&]() {   \
            if (!_taskFinished) {                  \
                throw std::logic_error(            \
                    std::string(func_name) +       \
                    " should only be called after counting tasks are finished."); \
            }                                      \
        });                                        \
    } while (0)
    /**
   * @brief Returns the total number of words counted.
   * @return size_t Total words.
   */
    [[nodiscard]] __FORCE_INLINE std::size_t _FastWcInternalClass::getTotalWord() const noexcept {
		BLOCK_CHECK_WORKER_TERMINATION("getTotalWord()");
        
        return totalWords;
    }

    /**
     * @brief Returns the total number of lines counted.
     * @return size_t Total lines.
     */
    [[nodiscard]] __FORCE_INLINE std::size_t _FastWcInternalClass::getTotalLine() const noexcept {
		BLOCK_CHECK_WORKER_TERMINATION("getTotalLine()");
        
        return totalLines;
    }

    /**
     * @brief Returns the total number of characters counted.
     * @return size_t Total characters.
     */
    [[nodiscard]] __FORCE_INLINE std::size_t _FastWcInternalClass::getTotalChar() const noexcept {
        // Assure first that every worker have stopped working
        // at the function call, can be a performance bottleneck
		BLOCK_CHECK_WORKER_TERMINATION("getTotalChar()");

        return totalChars;
    }

    /**
     * @brief Returns the total number of bytes counted.
     * @return size_t Total bytes.
     */
    [[nodiscard]] __FORCE_INLINE std::size_t _FastWcInternalClass::getTotalBytes() const noexcept {
		BLOCK_CHECK_WORKER_TERMINATION("getTotalBytes()");
        
        return totalBytes;
    }



#if __cplusplus < 201402L
    auto mark(const std::string& str, std::string color) -> decltype(dye::vanilla(""))
#else
    auto mark(const std::string& str, std::string color)
#endif
    {
        std::istringstream iss(str);
        auto marked = dye::vanilla("");
		// TODO: Implement mark logic, coloring the string based on the specified color.
        return marked;
    }

    void _FastWcInternalClass::printHeader() const
    {
        std::cout << std::left;
        if (countLine) {
            std::cout << std::setw(maxLinesWidth) << dye::green_on_aqua("Lines") << ' ';
        }
        if (countWord) {
            std::cout << std::setw(maxWordsWidth) << dye::green_on_aqua("Words") << ' ';
        }
        if (countChar) {
            std::cout << std::setw(maxCharsWidth) << dye::green_on_aqua("Chars") << ' ';
        }
        if (countByte) {
            std::cout << std::setw(maxBytesWidth) << dye::green_on_aqua("Bytes") << ' ';
        }

		std::cout << dye::green_on_aqua("File") << std::endl;
    }

    /**
     * @brief Prints the counts (lines, words, characters, bytes) for each mapped
     * file and the total.
     *
     * The output respects column widths for alignment. Only enabled counters
     * (count_line, count_word, count_char, count_bytes) are printed.
     */
    __FORCE_INLINE void _FastWcInternalClass::printTotal() const noexcept {
        printHeader();

        for (const auto& file : _mappedFile) {
            if (countLine) {
                std::cout << std::setw(std::max((size_t)5, maxLinesWidth+1)) << file.getLineCnt() << ' ';
            }
            if (countWord) {
                std::cout << std::setw(std::max((size_t)5, maxWordsWidth + 1)) << file.getWordCnt() << ' ';
            }
            if (countChar) {
                std::cout << std::setw(std::max((size_t)5, maxCharsWidth + 1)) << file.getCharCnt() << ' ';
            }
            if (countByte) {
                std::cout << std::setw(std::max((size_t)5, maxBytesWidth + 1)) << file.getBytesCnt() << ' ';
            }
            std::cout << file.filename() << std::endl;
        }
        if (countLine) {
            std::cout << std::setw(std::max((size_t)5, maxLinesWidth + 1)) << totalLines << ' ';
        }
        if (countWord) {
            std::cout << std::setw(std::max((size_t)5, maxWordsWidth + 1)) << totalWords << ' ';
        }
        if (countChar) {
            std::cout << std::setw(std::max((size_t)5, maxCharsWidth + 1)) << totalChars << ' ';
        }
        if (countByte) {
            std::cout << std::setw(std::max((size_t)5, maxBytesWidth + 1)) << totalBytes << ' ';
        }
        std::cout << "total" << std::endl;
    }
}