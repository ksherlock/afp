#ifndef __afp_resource_fork_h__
#define __afp_resource_fork_h__

#include <string>
#include <system_error>

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__)
#define AFP_WIN32
#endif

namespace afp {

	class resource_fork {

	public:
		enum open_mode {
			read_only = 1,
			write_only = 2,
			read_write = 3,
		};

		resource_fork() = default;
		resource_fork(const resource_fork &) = delete;
		resource_fork(resource_fork &&rhs);

		resource_fork& operator=(const resource_fork &rhs) = delete;
		resource_fork& operator=(resource_fork &&rhs);


		~resource_fork() { close(); }

		static size_t size(const std::string &path, std::error_code &ec);
#ifdef AFP_WIN32
		static size_t size(const std::wstring &path, std::error_code &ec);
#endif

		bool open(const std::string &s, open_mode mode, std::error_code &ec);

		bool open(const std::string &s, std::error_code &ec) {
			return open(s, read_only, ec);
		}

#ifdef AFP_WIN32
		bool open(const std::wstring &s, open_mode mode, std::error_code &ec);
		bool open(const std::wstring &s, std::error_code &ec) {
			return open(s, read_only, ec);
		}
#endif

		void close();

		size_t read(void *buffer, size_t n, std::error_code &);
		size_t write(const void *buffer, size_t n, std::error_code &);
		bool truncate(size_t pos, std::error_code &ec);
		bool seek(size_t pos, std::error_code &ec);
		size_t size(std::error_code &ec);


	private:
		#ifdef AFP_WIN32
		void *_fd = (void *)-1;
		#else
		int _fd = -1;
		#endif

		#if defined(__linux__) || defined(__FreeBSD__) || defined(_AIX)
		size_t _offset = 0;
		open_mode _mode = read_only;
		#endif
	};

}
#undef AFP_WIN32

#endif
