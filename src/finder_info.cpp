#include "finder_info.h"
#include "xattr.h"

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <string>

#if defined (_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define XATTR_FINDERINFO_NAME "AFP_AfpInfo"
#else
#include <unistd.h>
#include <fcntl.h>
#endif

#if defined(__APPLE__)
#include <sys/xattr.h>
#endif

#if defined(__linux__)
#define XATTR_FINDERINFO_NAME "user.com.apple.FinderInfo"
#endif


#ifndef XATTR_FINDERINFO_NAME
#define XATTR_FINDERINFO_NAME "com.apple.FinderInfo"
#endif


#if defined(_WIN32)
#define _prodos_file_type _afp.prodos_file_type
#define _prodos_aux_type _afp.prodos_aux_type
#define _finder_info _afp.finder_info
#endif

namespace {

// ENOATTR is not standard enough, so use ENODATA.
#if defined(ENOATTR) && ENOATTR != ENODATA
	int remap_errno(int e) {
		if (e == ENOATTR) return ENODATA;
		return e;
	}
#else
	int remap_errno(int e) { return e; }
#endif
	int remap_errno(void) { return remap_errno(errno); }

	/*

     tech note PT515
     ProDOS -> Macintosh conversion

     ProDOS             Macintosh
     Filetype    Auxtype    Creator    Filetype
     $00          $0000     'pdos'     'BINA'
     $B0 (SRC)    (any)     'pdos'     'TEXT'
     $04 (TXT)    $0000     'pdos'     'TEXT'
     $FF (SYS)    (any)     'pdos'     'PSYS'
     $B3 (S16)    (any)     'pdos'     'PS16'
     $uv          $wxyz     'pdos'     'p' $uv $wx $yz

     Programmer's Reference for System 6.0:

     ProDOS Macintosh
     File Type Auxiliary Type Creator Type File Type
     $00        $0000           'pdos'  'BINA'
     $04 (TXT)  $0000           'pdos'  'TEXT'
     $FF (SYS)  (any)           'pdos'  'PSYS'
     $B3 (S16)  $DByz           'pdos'  'p' $B3 $DB $yz
     $B3 (S16)  (any)           'pdos'  'PS16'
     $D7        $0000           'pdos'  'MIDI'
     $D8        $0000           'pdos'  'AIFF'
     $D8        $0001           'pdos'  'AIFC'
     $E0        $0005           'dCpy'  'dImg'
     $FF (SYS)  (any)           'pdos'  'PSYS'
     $uv        $wxyz           'pdos'  'p' $uv $wx $yz


	  mpw standard:
     $uv        (any)          "pdos"  printf("%02x  ",$uv)

     */



	int hex(uint8_t c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c + 10 - 'a';
		if (c >= 'A' && c <= 'F') return c + 10 - 'A';
		return 0;
	}


	bool finder_info_to_filetype(const uint8_t *buffer, uint16_t *file_type, uint32_t *aux_type) {

		if (!memcmp("pdos", buffer + 4, 4))
		{
			if (buffer[0] == 'p') {
				*file_type = buffer[1];
				*aux_type = (buffer[2] << 8) | buffer[3];
				return true;
			}
			if (!memcmp("PSYS", buffer, 4)) {
				*file_type = 0xff;
				*aux_type = 0x0000;
				return true;
			}
			if (!memcmp("PS16", buffer, 4)) {
				*file_type = 0xb3;
				*aux_type = 0x0000;
				return true;
			}

			// old mpw method for encoding.
			if (!isxdigit(buffer[0]) && isxdigit(buffer[1]) && buffer[2] == ' ' && buffer[3] == ' ')
			{
				*file_type = (hex(buffer[0]) << 8) | hex(buffer[1]);
				*aux_type = 0;
				return true;
			}
		}
		if (!memcmp("TEXT", buffer, 4)) {
			*file_type = 0x04;
			*aux_type = 0x0000;
			return true;
		}
		if (!memcmp("BINA", buffer, 4)) {
			*file_type = 0x00;
			*aux_type = 0x0000;
			return true;
		}
		if (!memcmp("dImgdCpy", buffer, 8)) {
			*file_type = 0xe0;
			*aux_type = 0x0005;
			return true;
		}

		if (!memcmp("MIDI", buffer, 4)) {
			*file_type = 0xd7;
			*aux_type = 0x0000;
			return true;
		}

		if (!memcmp("AIFF", buffer, 4)) {
			*file_type = 0xd8;
			*aux_type = 0x0000;
			return true;
		}

		if (!memcmp("AIFC", buffer, 4)) {
			*file_type = 0xd8;
			*aux_type = 0x0001;
			return true;
		}

		return false;
	}

	bool file_type_to_finder_info(uint8_t *buffer, uint16_t file_type, uint32_t aux_type) {
		if (file_type > 0xff || aux_type > 0xffff) return false;

		if (!file_type && aux_type == 0x0000) {
			memcpy(buffer, "BINApdos", 8);
			return true;
		}

		if (file_type == 0x04 && aux_type == 0x0000) {
			memcpy(buffer, "TEXTpdos", 8);
			return true;
		}

		if (file_type == 0xff && aux_type == 0x0000) {
			memcpy(buffer, "PSYSpdos", 8);
			return true;
		}

		if (file_type == 0xb3 && aux_type == 0x0000) {
			memcpy(buffer, "PS16pdos", 8);
			return true;
		}

		if (file_type == 0xd7 && aux_type == 0x0000) {
			memcpy(buffer, "MIDIpdos", 8);
			return true;
		}
		if (file_type == 0xd8 && aux_type == 0x0000) {
			memcpy(buffer, "AIFFpdos", 8);
			return true;
		}
		if (file_type == 0xd8 && aux_type == 0x0001) {
			memcpy(buffer, "AIFCpdos", 8);
			return true;
		}
		if (file_type == 0xe0 && aux_type == 0x0005) {
			memcpy(buffer, "dImgdCpy", 8);
			return true;
		}


		memcpy(buffer, "p   pdos", 8);
		buffer[1] = (file_type) & 0xff;
		buffer[2] = (aux_type >> 8) & 0xff;
		buffer[3] = (aux_type) & 0xff;
		return true;
	}

	void set_or_throw_error(std::error_code *ec, int error, const std::string &what) {
		if (ec) *ec = std::error_code(error, std::system_category());
		else throw std::system_error(error, std::system_category(), what);
	}




#if defined(_WIN32)

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
				access = GENERIC_READ | GENERIC_WRITE; // we always read existing info on file.
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

	void set_or_throw_error(std::error_code *ec, const std::string &what) {
		auto e = errno;
		set_or_throw_error(ec, e, what);
	}




#if defined(_WIN32)
void afp_init(struct AFP_Info *info) {
	//static_assert(sizeof(AFP_Info) == 60, "Incorrect AFP_Info size");
	memset(info, 0, sizeof(*info));
	info->magic = 0x00504641;
	info->version = 0x00010000;
	info->backup_date = 0x80000000;
}

int afp_verify(struct AFP_Info *info) {
	if (!info) return 0;

	if (info->magic != 0x00504641) return 0;
	if (info->version != 0x00010000) return 0;

	return 1;
}


int afp_to_filetype(struct AFP_Info *info, uint16_t *file_type, uint32_t *aux_type) {
	// check for prodos ftype/auxtype...
	if (info->prodos_file_type || info->prodos_aux_type) {
		*file_type = info->prodos_file_type;
		*aux_type = info->prodos_aux_type;
		return 0;
	}
	int ok = finder_info_to_filetype(info->finder_info, file_type, aux_type);
	if (ok == 0) {
		info->prodos_file_type = *file_type;
		info->prodos_aux_type = *aux_type;
	}
	return 0;
}

enum {
	trust_prodos,
	trust_hfs
};

void afp_synchronize(struct AFP_Info *info, int trust) {
	// if ftype/auxtype is inconsistent between prodos and finder info, use
	// prodos as source of truth.
	uint16_t f = 0;
	uint32_t a = 0;
	if (finder_info_to_filetype(info->finder_info, &f, &a)) {
		if (f == info->prodos_file_type && a == info->prodos_aux_type) return;
	}
	
	if (trust == trust_prodos)
		file_type_to_finder_info(info->finder_info, info->prodos_file_type, info->prodos_aux_type);
	else {
		info->prodos_file_type = f;
		info->prodos_aux_type = a;
	}
}



#endif

}

namespace afp {

#if defined(_WIN32)
finder_info::finder_info() {
	afp_init(&_afp);
}
void finder_info::close() {
	if (_fd != INVALID_HANDLE_VALUE) CloseHandle(_fd);
	_fd = INVALID_HANDLE_VALUE;
}
void _finder_info::clear() {
	std::memset(&_afp, sizeof(_afp), 0);
	afp_init(&_afp);
}

#else
finder_info::finder_info() {
	memset(&_finder_info, 0, sizeof(_finder_info));
}
void finder_info::close() {
	if (_fd >= 0) ::close(_fd);
	_fd = -1;
}
void _finder_info::clear() {
	std::memset(&_finder_info, sizeof(+_finder_info), 0);
	_prodos_file_type = 0;
	_prodos_aux_type = 0;
}


#endif


finder_info::~finder_info() {
	close();
}

#if defined(_WIN32)

bool finder_info::open(const std::string &path, open_mode mode, std::error_code &ec) {

	ec.clear();
	close();
	clear();

	std::string s(path);
	s += ":" XATTR_FINDERINFO_NAME;

	/* open the base file, then the finder info, so we can clarify the error */

	HANDLE h = CreateFileX(path, read_only, ec);
	if (ec) return false;
	_fd = CreateFileX(s, mode, ec);
	CloseHandle(h);
	if (ec) {
		if (ec.value() == ERROR_FILE_NOT_FOUND)
			ec = std::errc::no_message_available;
		return false;
	}

	// always read the data.
	DWORD transferred = 0;
	BOOL ok = ReadFile(_fd, _afp, sizeof(_afp), &transferred, nullptr);
	auto e = GetLastError();
	if (mode == read_only) close();

	if (!ok) {
		afp_init(&_afp);
		set_or_throw_error(&ec, e, "ReadFile");
		return false;
	}
	// warn if incorrect size or data.
	if (transferred != sizeof(_afp) || !afp_verify(&_afp)) {
		afp_init(&_afp);
		if (mode != write_only) {
			ec = std::errc::illegal_byte_sequence;
			return false;
		}
	}
	return true;
}

bool finder_info::write(std::error_code &ec) {
	LARGE_INTEGER ll;
	BOOL ok;

	ll.QuadPart = 0;

	ok = SetFilePointerEx(_fd, ll, nullptr, FILE_BEGIN);
	if (!ok) {
		set_or_throw_error(&ec, "SetFilePointerEx");
		return false;
	}

	ok = WriteFile(_fd, &_afp, sizeof(_afp), nullptr, nullptr);
	if (!ok) {
		set_or_throw_error(&ec, "WriteFile");
		return false;
	}
	return true;
}

bool finder_info::write(const std::string &path, std::error_code &ec) {
	BOOL ok;

	std::string s(path);
	s += ":" XATTR_FINDERINFO_NAME;

	HANDLE h = CreateFileX(s, 
		GENERIC_WRITE, 
		FILE_SHARE_READ, 
		nullptr, 
		CREATE_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL, 
		nullptr);

	if (h == INVALID_HANDLE_VALUE) {
		set_or_throw_error(&ec, "CreateFile");
		return false;
	}

	ok = WriteFile(h, &_afp, sizeof(_afp), nullptr, nullptr);
	int e = GetLastError();
	CloseHandle(h);
	if (!ok) {
		set_or_throw_error(&ec, e, "WriteFile");
		return false;
	}
	return true;
}

#elif defined(__sun__)
bool finder_info::open(const std::string &path, open_mode mode, std::error_code &ec) {
	ec.clear();
	close();
	clear();

	int umode = 0;
	switch(perm) {
		case read_only: umode = O_RDONLY; break;
		case read_write: umode = O_RDWR | O_CREAT; break;
		case write_only: umode = O_WRONLY | O_CREAT | O_TRUNC; break;
	}

	// attropen is a front end for open / openat.
	// do it ourselves so we can distinguish file doesn't exist vs attr doesn't exist.

	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		set_or_throw_error(&ec, "open");
		return false;
	}

	_fd = openat(fd, XATTR_FINDERINFO_NAME, umode | O_XATTR, 0666);
	int e = remap_errno();
	::close(fd);

	if (_fd < 0) {
		if (e == ENOENT) e = ENODATA;
		set_or_throw_error(&ec, e, "openat");
		return false;
	}

	if (mode == read_only || mode == read_write) {
		// read it...
		auto ok = ::pread(_fd, &_finder_info, 32, 0);
		e = remap_errno();
		if (mode == read_only) close();
		if (ok < 0) {
			set_or_throw_error(&ec, e, "pread");
			return false;
		}
	}
	return true;
}

bool finder_info::write(std::error_code &ec) {
	ec.clear();
	auto ok = ::pwrite(_fd, &_finder_info, 32, 0);
	if (ok < 0) {
		set_or_throw_error(&ec, remap_errno(), "pwrite");
		return false;
	}
	return true;
}

bool finder_info::write(const std::string &path, std::error_code &ec) {
	ec.clear();

	int e;

	// attropen safe to use here.
	int fd = attropen(path.c_str, XATTR_FINDERINFO_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	e = remap_errno();
	if (fd < 0) {
		set_or_throw_error(&ec, e, "attropen");
		return false;
	}
	auto ok = ::pwrite(fd, &_finder_info, 32, 0);
	e = remap_errno();
	::close(fd);

	if (ok < 0) {
		set_or_throw_error(&ec, e, "pwrite");
		return false;
	}
	return true;
}
#else
bool finder_info::open(const std::string &path, open_mode mode, std::error_code &ec) {
	ec.clear();
	close();
	clear();

	_fd = open(path.c_str(), O_RDONLY);
	if (_fd < 0) {
		set_or_throw_error(&ec, "open");
		return false;
	}

	if (mode == read_only || mode == read_write) {
		auto ok = read_xattr(_fd, XATTR_FINDERINFO_NAME, 32);
		e = remap_errno();
		if (mode == read_only) close();
		if (ok < 0) {
			set_or_throw_error(&ec, e, "read_xattr");
			return false;
		}
	}
	return true;
}

bool finder_info::write(std::error_code &ec) {
	ec.clear();
	// n.b. no way to differentiate closed vs opened read-only.
	auto ok = write_xattr(_fd, XATTR_FINDERINFO_NAME, _finder_info, 32);
	if (ok < 0) {
		set_or_throw_error(&ec, remap_errno(), "write_xattr");
		return false;
	}
	return true;
}

bool finder_info::write(const std::string &path, std::error_code &ec) {
	ec.clear();

	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		set_or_throw_error(&ec, "open");
		return false;
	}

	auto ok = write_xattr(fd, XATTR_FINDERINFO_NAME, _finder_info, 32);
	int e = remap_errno();
	::close(fd);
	if (ok < 0) {
		set_or_throw_error(&ec, e, "write_xattr");
		return false;	
	}
	return true;
}

#endif



void finder_info::set_prodos_file_type(uint16_t ftype, uint32_t atype) {
	_prodos_file_type = ftype;
	_prodos_aux_type = atype;
	file_type_to_finder_info(_finder_info, ftype, atype);
}


void finder_info::set_prodos_file_type(uint16_t ftype) {
	set_prodos_file_type(ftype, _prodos_aux_type);
}

bool finder_info::is_text() const {
	if (memcmp(_finder_info, "TEXT", 4) == 0) return true;
	if (_prodos_file_type == 0x04) return true;
	if (_prodos_file_type == 0xb0) return true;

	return false;
}

bool finder_info::is_binary() const {
	if (is_text()) return false;
	if (_prodos_file_type || _prodos_aux_type) return true;

	if (memcmp(_finder_info, "\x00\x00\x00\x00\x00\x00\x00\x00", 8) == 0) return false;
	return true;
}



uint32_t finder_info::file_type() const {
	uint32_t rv = 0;
	for (unsigned i = 0; i < 4; ++i) {
		rv <<= 8;
		rv |= _finder_info[i];
	}
	return rv;
}

uint32_t finder_info::creator_type() const {
	uint32_t rv = 0;
	for (unsigned i = 4; i < 8; ++i) {
		rv <<= 8;
		rv |= _finder_info[i];
	}
	return rv;
}

void finder_info::set_file_type(uint32_t x) {
	_finder_info[0] = x >> 24;
	_finder_info[1] = x >> 16;
	_finder_info[2] = x >> 8;
	_finder_info[3] = x >> 0;
}


void finder_info::set_creator_type(uint32_t x) {
	_finder_info[4] = x >> 24;
	_finder_info[5] = x >> 16;
	_finder_info[6] = x >> 8;
	_finder_info[7] = x >> 0;
}

}
