#include "resource_fork.h"

#include <cstring>

#if defined(__CYGWIN__) || defined(__MSYS__)
#if !defined(_WIN32)
#define _WIN32
#endif
#endif


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define XATTR_RESOURCEFORK_NAME "AFP_Resource"
#else
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "xattr.h"
#endif

#ifdef __APPLE__
#include <sys/xattr.h>
#include <sys/paths.h>
#ifndef _PATH_RSRCFORKSPEC
#define _PATH_RSRCFORKSPEC "/..namedfork/rsrc"
#endif
#endif

#if defined(__linux__)
#define XATTR_RESOURCEFORK_NAME "user.com.apple.ResourceFork"
#endif


#ifndef XATTR_RESOURCEFORK_NAME
#define XATTR_RESOURCEFORK_NAME "com.apple.ResourceFork"
#endif

#if defined(__linux__) || defined(__FreeBSD__) || defined(_AIX)
#define XATTR_RESOURCE_FORK

#include <vector>

#endif


namespace {


// ENOATTR is not standard enough, so use ENODATA.
#if defined(ENOATTR) && ENOATTR != ENODATA

	void remap_enoattr(std::error_code &ec) {
		if (ec.value() == ENOATTR)
			ec = std::make_error_code(std::errc::no_message_available);
	}
#else
	void remap_enoattr(std::error_code &ec) {}
#endif



#if defined(_WIN32)

#ifdef _MSC_VER
#define AFP_ERROR_FILE_NOT_FOUND ERROR_FILE_NOT_FOUND
#define remap_os_error(x) x
#else
#define AFP_ERROR_FILE_NOT_FOUND ENOENT
extern "C" int remap_os_error(unsigned long);
#endif

	BOOL _(BOOL x, std::error_code &ec) {
		if (!x) ec = std::error_code(remap_os_error(GetLastError()), std::system_category());
		return x;
	}

	HANDLE _(HANDLE x, std::error_code &ec) {
		if (x == INVALID_HANDLE_VALUE)
			ec = std::error_code(remap_os_error(GetLastError()), std::system_category());
		return x;
	}

#undef CreateFile
	template<class ...Args>
	HANDLE CreateFile(const std::string &s, Args... args) {
		return CreateFileA(s.c_str(), std::forward<Args>(args)...);
	}

	template<class ...Args>
	HANDLE CreateFile(const std::wstring &s, Args... args) {
		return CreateFileW(s.c_str(), std::forward<Args>(args)...);
	}

	template<class StringType>
	HANDLE CreateFileX(const StringType &s, afp::resource_fork::open_mode mode, std::error_code &ec) {


		DWORD access = 0;
		DWORD create = 0;

		switch (mode) {
			case afp::resource_fork::read_only:
				access = GENERIC_READ;
				create = OPEN_EXISTING;
				break;
			case afp::resource_fork::read_write:
				access = GENERIC_READ | GENERIC_WRITE;
				create = OPEN_ALWAYS;
				break;
			case afp::resource_fork::write_only:
				access = GENERIC_WRITE;
				create = OPEN_ALWAYS;
				break;
		}

		HANDLE h =  _(CreateFile(s, access, FILE_SHARE_READ, nullptr, create, FILE_ATTRIBUTE_NORMAL, nullptr), ec);

		return h;
	}

	DWORD GetFileAttributesX(const std::string &path) {
		return GetFileAttributesA(path.c_str());
	}
	DWORD GetFileAttributesX(const std::wstring &path) {
		return GetFileAttributesW(path.c_str());
	}

	template<class StringType>
	bool regular_file(const StringType &path, std::error_code &ec) {

		// make sure this isn't a directory.
		DWORD st = GetFileAttributesX(path);
		if (st == INVALID_FILE_ATTRIBUTES) {
			ec = std::error_code(remap_os_error(GetLastError()), std::system_category());
			return false;
		}
		if (st & FILE_ATTRIBUTE_DIRECTORY) {
			ec = std::make_error_code(std::errc::is_a_directory);
			return false;
		}
		if (st & FILE_ATTRIBUTE_DEVICE) {
			ec = std::make_error_code(std::errc::invalid_seek);
			return false;
		}
		return true;
	}


#else

	template<class T>
	T _(T x, std::error_code &ec) {
		if (x < 0) ec = std::error_code(errno, std::system_category());
		return x;
	}

	bool regular_file(int fd, std::error_code &ec) {
			struct stat st;
			if (_(::fstat(fd, &st), ec) < 0) {
				return false;
			}
			if (S_ISREG(st.st_mode)) return true;

			if (S_ISDIR(st.st_mode)) {
				ec = std::make_error_code(std::errc::is_a_directory);
			} else {
				ec = std::make_error_code(std::errc::invalid_seek); // ESPIPE.
			}
			return false;
	}

	/* opens a file read-only and verifies it's a regular file */
	int openX(const std::string &path, std::error_code &ec) {
		int fd = _(::open(path.c_str(), O_RDONLY | O_NONBLOCK), ec);
		if (fd >= 0 && !regular_file(fd, ec)) {
			::close(fd);
			fd = -1;
		}
		return fd;
	}

#endif

}

namespace afp {

	resource_fork::resource_fork(resource_fork &&rhs) {
		std::swap(_fd, rhs._fd);

		#if defined(__linux__) || defined(__FreeBSD__) || defined(_AIX)
		std::swap(_offset, rhs._offset);
		std::swap(_mode, rhs._mode);
		#endif
	}

	resource_fork& resource_fork::operator=(resource_fork &&rhs) {
		if (this != &rhs) {
			close();
			std::swap(_fd, rhs._fd);

			#if defined(__linux__) || defined(__FreeBSD__) || defined(_AIX)
			std::swap(_offset, rhs._offset);
			std::swap(_mode, rhs._mode);
			#endif
		}
		return *this;
	}


#ifdef _WIN32

	bool resource_fork::open(const std::string &path, open_mode mode, std::error_code &ec) {
		ec.clear();
		close();

		if (!regular_file(path, ec)) return false;

		std::string s(path);
		s += ":" XATTR_RESOURCEFORK_NAME;

		HANDLE h = CreateFileX(path, read_only, ec);
		if (ec) return false;
		_fd = CreateFileX(s, mode, ec);
		CloseHandle(h);
		if (ec) {
			if (ec.value() == AFP_ERROR_FILE_NOT_FOUND)
				ec = std::make_error_code(std::errc::no_message_available);
			return false;
		}

		return true;
	}

	bool resource_fork::open(const std::wstring &path, open_mode mode, std::error_code &ec) {
		ec.clear();
		close();

		if (!regular_file(path, ec)) return false;

		std::wstring s(path);
		s += L":" XATTR_RESOURCEFORK_NAME;

		HANDLE h = CreateFileX(path, read_only, ec);
		if (ec) return false;
		_fd = CreateFileX(s, mode, ec);
		CloseHandle(h);
		if (ec) {
			if (ec.value() == AFP_ERROR_FILE_NOT_FOUND)
				ec = std::make_error_code(std::errc::no_message_available);
			return false;
		}

		return true;
	}


	void resource_fork::close() {
		CloseHandle(_fd);
		_fd = INVALID_HANDLE_VALUE;
	}

	size_t resource_fork::read(void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		DWORD transferred = 0;
		BOOL ok = _(ReadFile(_fd, buffer, n, &transferred, nullptr), ec);
		if (ec) return 0;
		return transferred;
	}

	size_t resource_fork::write(const void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		DWORD transferred = 0;
		BOOL ok = _(WriteFile(_fd, buffer, n, &transferred, nullptr), ec);
		if (ec) return 0;
		return transferred;
	}

	size_t resource_fork::size(std::error_code &ec) {
		ec.clear();
		LARGE_INTEGER ll = { };
		BOOL ok = _(GetFileSizeEx(_fd, &ll), ec);
		if (ec) return 0;
		return static_cast<size_t>(ll.QuadPart);
	}

	bool resource_fork::truncate(size_t pos, std::error_code &ec) {
		ec.clear();

		LARGE_INTEGER ll;
		BOOL ok;

		ll.QuadPart = pos;

		ok = _(SetFilePointerEx(_fd, ll, nullptr, FILE_BEGIN), ec);
		if (ec) return false;

		ok = _(SetEndOfFile(_fd), ec);
		if (ec) return false;

		return true;
	}

	bool resource_fork::seek(size_t pos, std::error_code &ec) {
		ec.clear();

		LARGE_INTEGER ll;
		BOOL ok;

		ll.QuadPart = pos;

		ok = _(SetFilePointerEx(_fd, ll, nullptr, FILE_BEGIN), ec);
		if (ec) return false;
		return true;
	}
#else
	void resource_fork::close() {
		::close(_fd);
		_fd = -1;
	}
#endif


#ifdef __sun__
	#define FD_RESOURCE_FORK
	bool resource_fork::open(const std::string &path, open_mode mode, std::error_code &ec) {
		ec.clear();
		close();

		int umode = 0;
		switch(mode) {
			case read_only: umode = O_RDONLY; break;
			case write_only: umode = O_WRONLY | O_CREAT; break;
			case read_write: umode = O_RDWR | O_CREAT; break;
		}

		int fd = openX(path, ec);
		if (ec) return false;

		_fd = ::openat(path.c_str(), XATTR_RESOURCEFORK_NAME, umode | O_XATTR, 0666);
		::close(fd);

		if (ec) {
			if (ec.value() == ENOENT)
				ec = std::make_error_code(std::errc::no_message_available); // ENODATA.
			return false;
		}

		return true;
	}

#endif


#ifdef __APPLE__
	#define FD_RESOURCE_FORK
	bool resource_fork::open(const std::string &path, open_mode mode, std::error_code &ec) {
		ec.clear();
		close();

		std::string s(path);
		s += _PATH_RSRCFORKSPEC;

		int fd = openX(path, ec);
		if (ec) return false;

		int umode = 0;
		switch(mode) {
			case read_only: umode = O_RDONLY; break;
			case write_only: umode = O_WRONLY | O_CREAT; break;
			case read_write: umode = O_RDWR | O_CREAT; break;
		}

		_fd = _(::open(s.c_str(), umode, 0666), ec);
		::close(fd);
		if (ec) {
			if (ec.value() == ENOENT)
				ec = std::make_error_code(std::errc::no_message_available);
			return false;
		}

		return true;
	}

#endif

#ifdef FD_RESOURCE_FORK
	size_t resource_fork::read(void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		auto rv = _(::read(_fd, buffer, n), ec);
		if (ec) return 0;

		return rv;
	}

	size_t resource_fork::write(const void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		auto rv = _(::write(_fd, buffer, n), ec);
		if (ec) return 0;

		return rv;
	}

	bool resource_fork::truncate(size_t pos, std::error_code &ec) {
		ec.clear();
		_(::ftruncate(_fd, pos), ec);
		if (ec) return false;
		return true;
	}

	bool resource_fork::seek(size_t pos, std::error_code &ec) {
		ec.clear();
		_(::lseek(_fd, pos, SEEK_SET), ec);
		if (ec) return false;
		return true;
	}

	size_t resource_fork::size(std::error_code &ec) {
		ec.clear();
		struct stat st;
		_(::fstat(_fd, &st), ec);
		if (ec) return 0;
		return st.st_size;
	}

#endif

#ifdef XATTR_RESOURCE_FORK
	namespace {
		std::vector<uint8_t> read_rfork(int _fd, std::error_code &ec) {
			std::vector<uint8_t> rv;

			for(;;) {
				ssize_t size = 0;
				ssize_t tsize = 0;

				rv.clear();
				ec.clear();
				size = _(::size_xattr(_fd, XATTR_RESOURCEFORK_NAME), ec);
				if (ec)  return rv;

				if (size == 0) return rv;
				rv.resize(size);

				tsize = _(::read_xattr(_fd, XATTR_RESOURCEFORK_NAME, rv.data(), size), ec);
				if (ec) {
					if (ec.value() == ERANGE) continue;
					rv.clear();
					return rv;
				}
				rv.resize(tsize);
				return rv;
			}
		}
	}

	bool resource_fork::open(const std::string &path, open_mode mode, std::error_code &ec) {
		close();
		ec.clear();

		_fd = openX(path, ec);
		if (ec) return false;

		_mode = mode;
		_offset = 0;
		return true;
	}

	size_t resource_fork::size(std::error_code &ec) {
		ec.clear();
		auto rv = _(::size_xattr(_fd, XATTR_RESOURCEFORK_NAME), ec);
		if (ec) {
			remap_enoattr(ec);
			return false;
		}
		return rv;
	}

	size_t resource_fork::read(void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		if (_fd < 0 || _mode == write_only) {
			ec = std::make_error_code(std::errc::bad_file_descriptor);
			return 0;
		}

		if (n == 0) return 0;

		auto tmp = read_rfork(_fd, ec);
		if (ec) {
			remap_enoattr(ec);
			return 0;
		}

		if (_offset >= tmp.size()) return 0;
		size_t count = std::min(n, tmp.size() - _offset);

		std::memcpy(buffer, tmp.data() + _offset, count);
		_offset += count;
		return count;
	}

	size_t resource_fork::write(const void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		if (_fd < 0 || _mode == read_only) {
			ec = std::make_error_code(std::errc::bad_file_descriptor);
			return 0;
		}

		if (n == 0) return 0;

		auto tmp = read_rfork(_fd, ec);
		if (ec) {
			remap_enoattr(ec);
			return 0;
		}

		if (_offset > tmp.size()) {
			tmp.resize(_offset);
		}
		tmp.insert(tmp.end(), (const uint8_t *)buffer, (const uint8_t *)buffer + n);

		auto rv = _(::write_xattr(_fd, XATTR_RESOURCEFORK_NAME, tmp.data(), tmp.size()), ec);
		if (ec) {
			remap_enoattr(ec);
			return 0;
		}

		return n;
	}

	bool resource_fork::truncate(size_t pos, std::error_code &ec) {

		ec.clear();
		if (_fd < 0 || _mode == read_only) {
			ec = std::make_error_code(std::errc::bad_file_descriptor);
			return 0;
		}

		// simple case..
		if (pos == 0) {
			auto rv = _(::remove_xattr(_fd, XATTR_RESOURCEFORK_NAME), ec);
			if (ec) {
				remap_enoattr(ec); // consider that ok?
				return false;
			}
			return true;
		}
		auto tmp = read_rfork(_fd, ec);
		if (ec) {
			remap_enoattr(ec);
			return false;
		}
		if (tmp.size() == pos) return true;
		tmp.resize(pos);
		auto rv = _(::write_xattr(_fd, XATTR_RESOURCEFORK_NAME, tmp.data(), pos), ec);
		if (ec) {
			remap_enoattr(ec);
			return false;
		}

		_offset = pos;
		return true;
	}
	bool resource_fork::seek(size_t pos, std::error_code &ec) {
		ec.clear();
		if (_fd < 0) {
			ec = std::make_error_code(std::errc::bad_file_descriptor);
			return 0;
		}

		_offset = pos;
		return true;
	}


#endif

	size_t resource_fork::size(const std::string &path, std::error_code &ec) {
		resource_fork rf;
		rf.open(path, read_only, ec);
		if (ec) return 0;
		return rf.size(ec);
	}

#ifdef _WIN32
	size_t resource_fork::size(const std::wstring &path, std::error_code &ec) {
		resource_fork rf;
		rf.open(path, read_only, ec);
		if (ec) return 0;
		return rf.size(ec);
	}
#endif

}
