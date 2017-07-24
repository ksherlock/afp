#include "resource_fork.h"
#include "xattr.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define XATTR_RESOURCEFORK_NAME "AFP_ResourceFork"
#else
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#ifdef __APPLE_
#include <sys/xattr.h>
#include <sys/paths.h>
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

	void set_or_throw_error(std::error_code *ec, int error, const std::string &what) {
		if (ec) *ec = std::error_code(error, std::system_category());
		else throw std::system_error(error, std::system_category(), what);
	}


#ifdef _WIN32

	/*
	* allocating a new string could reset GetLastError() to 0.
	*/
	void set_or_throw_error(std::error_code *ec, const char *what) {
		auto e = GetLastError();
		set_or_throw_error(ec, e, what);
	}

	void set_or_throw_error(std::error_code *ec, const std::string &what) {
		auto e = GetLastError();
		set_or_throw_error(ec, e, what);
	}

	template<class ...Args>
	HANDLE CreateFileX(const std::string &s, Args... args) {
		return CreateFileA(s.c_str(), std::forward<Args>(args)...);
	}

	template<class ...Args>
	HANDLE CreateFileX(const std::wstring &s, Args... args) {
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

		HANDLE h =  CreateFileX(s, access, FILE_SHARE_READ, nullptr, create, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (h == INVALID_HANDLE_VALUE) {
			set_or_throw_error(&ec, "CreateFile");
		}
		return h;
	}
#else
	void set_or_throw_error(std::error_code *ec, const char *what) {
		auto e = errno
		set_or_throw_error(ec, e, what);
	}

	void set_or_throw_error(std::error_code *ec, const std::string &what) {
		auto e = errno
		set_or_throw_error(ec, e, what);
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
		close();
		std::swap(_fd, rhs._fd);

		#if defined(__linux__) || defined(__FreeBSD__) || defined(_AIX)
		std::swap(_offset, rhs._offset);
		std::swap(_mode, rhs._mode);
		#endif

		return *this;
	}


#ifdef _WIN32

	bool resource_fork::open(const std::string &path, open_mode mode, std::error_code &ec) {
		ec.clear();
		close();

		std::string s(path);
		s += ":" XATTR_RESOURCEFORK_NAME;

		_fd = CreateFileX(s, mode, ec);
		if (ec) return false;
		return true;
	}

	bool resource_fork::open(const std::wstring &path, open_mode mode, std::error_code &ec) {
		ec.clear();
		close();

		std::wstring s(path);
		s += L":" XATTR_RESOURCEFORK_NAME;

		_fd = CreateFileX(s, mode, ec);
		if (ec) return false;
		return true;
	}


	void resource_fork::close() {
		CloseHandle(_fd);
		_fd = INVALID_HANDLE_VALUE;
	}

	size_t resource_fork::read(void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		DWORD transferred = 0;
		BOOL ok = ReadFile(_fd, buffer, n, &transferred, nullptr);
		if (!ok) {
			set_or_throw_error(&ec, "ReadFile");
			return 0;
		}
		return transferred;
	}

	size_t resource_fork::write(const void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		DWORD transferred = 0;
		BOOL ok = WriteFile(_fd, buffer, n, &transferred, nullptr);
		if (!ok) {
			set_or_throw_error(&ec, "WriteFile");
			return 0;
		}
		return transferred;
	}

	size_t resource_fork::size(std::error_code &ec) {
		ec.clear();
		LARGE_INTEGER ll = { };
		BOOL ok = GetFileSizeEx(_fd, &ll);
		if (!ok) {
			set_or_throw_error(&ec, "GetFileSizeEx");
			return 0;			
		}
		return ll.QuadPart;
	}

	bool resource_fork::truncate(size_t pos, std::error_code &ec) {
		ec.clear();

		LARGE_INTEGER ll;
		BOOL ok;

		ll.QuadPart = pos;

		ok = SetFilePointerEx(_fd, ll, nullptr, FILE_BEGIN);
		if (!ok) {
			set_or_throw_error(&ec, "SetFilePointerEx");
			return false;			
		}

		ok = SetEndOfFile(_fd);
		if (!ok) {
			set_or_throw_error(&ec, "SetEndOfFile");
			return false;			
		}
		return true;
	}

	bool resource_fork::seek(size_t pos, std::error_code &ec) {
		ec.clear();

		LARGE_INTEGER ll;
		BOOL ok;

		ll.QuadPart = pos;

		ok = SetFilePointerEx(_fd, ll, nullptr, FILE_BEGIN);
		if (!ok) {
			set_or_throw_error(&ec, "SetFilePointerEx");
			return false;			
		}
		return true;		
	}
#else
	resource_fork::close() {
		close(_fd);
		_fd = -;1
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
			case write_only umode = O_WRONLY | O_CREAT; break;
			case read_write: umode = O_RDWR | O_CREAT; break;
		}
		_fd = attropen(path.c_str(), XATTR_RESOURCEFORK_NAME, umode, 0666);
		if (_fd < 0) {
			set_or_throw_error(ec, "attropen");
			return false;
		}
		return true;
	}

#endif


#ifdef __APPLE__
	#define FD_RESOURCE_FORK
	bool open(const std::string &path, open_mode mode, std::error_code &ec) {
		ec.clear();
		close();

		std::string s(path);
		s += _PATH_RSRCFORKSPEC;

		int umode = 0;
		switch(mode) {
			case read_only: umode = O_RDONLY; break;
			case write_only umode = O_WRONLY | O_CREAT; break;
			case read_write: umode = O_RDWR | O_CREAT; break;
		}

		_fd = open(s.c_str(), umode, 0666);
		if (_fd < 0) {
			set_or_throw_error(ec, "open");
			return false;
		}
		return true;
	}

#endif

#ifdef FD_RESOURCE_FORK
	size_t resource_fork::read(void *buffer, size_t n, std::error_code &) {
		ec.clear();
		auto rv = read(_fd, buffer, n);
		if (rv < 0) {
			set_or_throw_error(ec, "read");
			return 0;
		}
		return rv;
	}

	size_t resource_fork::write(const void *buffer, size_t n, std::error_code &) {
		ec.clear();
		auto rv = write(_fd, buffer, n);
		if (rv < 0) {
			set_or_throw_error(ec, "write");
			return 0;
		}
		return rv;
	}

	bool resource_fork::truncate(size_t pos, std::error_code &ec) {
		ec.clear();
		if (ftruncate(_fd, pos) < 0) {
			set_or_throw_error(ec, "ftruncate");
			return false;
		}
		return true;
	}

	bool resource_fork::seek(size_t pos, std::error_code &ec) {
		ec.clear();
		if (lseek(_fd, pos, SEEK_SET) < 0) {
			set_or_throw_error(ec, "lseek");
			return false;
		}
		return true;
	}

	size_t resource_fork::size(std::error_code &ec) {
		ec.clear();
		struct stat st;
		if (fstat(_fd, &st) < 0) {
			set_or_throw_error(ec, "fstat");
			return 0;
		}
		return st.st_size;
	}

#endif

#ifdef XATTR_RESOURCE_FORK
	namespace {
		std::vector<uint8_t> read_rfork(int _fd, std::error_code &ec) {
			std::vector<uint8_t> rv;

			ec.clear();

			for(;;) {
				ssize_t size = 0;
				ssize_t tsize = 0;

				for(;;) {
					rv.clear();
					size = size_xattr(_fd, XATTR_RESOURCEFORK_NAME);
					if (size < 0) {
						if (errno == EINTR) continue;
						if (errno == ENOATTR) {
							return rv;
						}
						set_or_throw_error(ec, "size_xattr");
						return rv;
					}
					break;
				}

				if (size == 0) return rv;
				rv.resize(size);

				for(;;) {
					tsize = read_xattr(_fd, XATTR_RESOURCEFORK_NAME, tmp.data(), size);
					if (tsize < 0) {
						if (errno == EINTR) continue;
						if (errno == ERANGE) break;
						if (errno == ENOATTR) {
							rv.clear();
							return rv;
						}
						set_or_throw_error(ec, "read_xattr");
						rv.clear();
						return rv;
					}
					rv.resize(tsize);
					return rv;
				}
			}
		}
	}
	bool resource_fork::open(const std::string &s, open_mode mode, std::error_code &ec) {
		close();
		ec.clear();
		_fd = open(s.c_str(), O_RDONLY);
		if (_fd < 0) {
			set_or_throw_error(ec, "open");
			return false;
		}
		_mode = mode;
		_offset = 0;
		return true;
	}


	size_t resource_fork::size(std::error_code &ec) {
		ec.clear();
		auto rv = size_xattr(_fd, XATTR_RESOURCEFORK_NAME);
		if (rv < 0) {
			set_or_throw_error(ec, "size_xattr");
			return 0;
		}
		return rv;
	}

	size_t resource_fork::read(void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		if (_fd < 0) {
			set_or_throw_error(ec, EBADF, "read");
			return 0;
		}
		if (_mode == write_only) {
			set_or_throw_error(ec, EPERM, "read");
			return 0;
		}

		if (n == 0) return 0;

		auto tmp = read_rfork(_fd, ec);
		if (ec) return 0;


		if (_offset >= tmp.size()) return 0;
		size_t count = std::min(n, tmp.size() - _offset);

		std::memcpy(buffer, tmp.data() + _offset, count);
		_offset += count;
		return count;
	}

	size_t resource_fork::write(const void *buffer, size_t n, std::error_code &ec) {
		ec.clear();
		if (_fd < 0) {
			set_or_throw_error(ec, EBADF, "write");
			return 0;
		}
		if (_mode == read_only) {
			set_or_throw_error(ec, EPERM, "write");
			return 0;
		}

		if (n == 0) return 0;

		auto tmp = read_rfork(_fd, ec);
		if (ec) return ec;

		if (_offset > tmp.size()) {
			tmp.resize(_offset);
		}
		tmp.append((const uint8_t *)buffer, (const uint8_t *buffer) + n);

		auto rv = write_xattr(_fd, XATTR_RESOURCEFORK_NAME, tmp.data(), tmp.size());
		if (rv < 0) {
			set_or_throw_error(ec, "write_xattr");
			return 0;			
		}
		return n;
	}

	bool resource_fork::truncate(size_t pos, std::error_code &ec) {

		ec.clear();
		if (_fd < 0) {
			set_or_throw_error(ec, EBADF, "resource_fork::truncate");
			return 0;
		}
		if (_mode == read_only) {
			set_or_throw_error(ec, EPERM, "resource_fork::truncate");
			return 0;
		}

		// simple case..
		if (pos == 0) {
			auto rv = remove_xattr(_fd, XATTR_RESOURCEFORK_NAME);
			if (rv < 0) {
				set_or_throw_error(ec, "remove_xattr");
				return false;		
			}
			return true;
		}
		auto tmp = read_rfork(_fd, ec);
		if (ec) return false;
		if (tmp.size() == pos) return true;
		tmp.resize(pos);
		auto rv = write_xattr(_fd, XATTR_RESOURCEFORK_NAME, tmp.data(), pos);
		if (rv < 0) {
				set_or_throw_error(ec, "write_xattr");
				return false;
		}
		_offset = pos;
		return true;
	}
	bool resource_fork::seek(size_t pos, std::error_code &ec) {
		ec.clear();
		if (_fd < 0) {
			set_or_throw_error(ec, EBADF, "truncate");
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
