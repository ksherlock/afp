#ifndef __afp_finder_info_h__
#define __afp_finder_info_h__

#include <stdint.h>
#include <string>

#include <system_error>

#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__)
#define AFP_WIN32
#endif

#if defined(AFP_WIN32)
#pragma pack(push, 2)
struct AFP_Info {
	uint32_t magic;
	uint32_t version;
	uint32_t file_id;
	uint32_t backup_date;
	uint8_t finder_info[32];
	uint16_t prodos_file_type;
	uint32_t prodos_aux_type;
	uint8_t reserved[6];
};
#pragma pack(pop)

#endif

namespace afp {
	class finder_info {

	public:

		enum open_mode {
			read_only = 1,
			write_only = 2,
			read_write = 3,
		};

		finder_info();
		~finder_info();

		finder_info(const finder_info &) = delete;
		finder_info(finder_info &&);

		finder_info& operator=(const finder_info &) = delete;
		finder_info& operator=(finder_info &&);


		bool read(const std::string &path, std::error_code &ec) {
			return open(path, read_only, ec);
		}

		bool write(const std::string &path, std::error_code &ec);

		bool open(const std::string &path, open_mode perm, std::error_code &ec);
		bool open(const std::string &path, std::error_code &ec) {
			return open(path, read_only, ec);
		}


	#if defined(AFP_WIN32)
		bool read(const std::wstring &path, std::error_code &ec) {
			return open(path, read_only, ec);
		}

		bool write(const std::wstring &path, std::error_code &ec);

		bool open(const std::wstring &path, open_mode perm, std::error_code &ec);
		bool open(const std::wstring &path, std::error_code &ec) {
			return open(path, read_only, ec);
		}

	#endif

		bool write(std::error_code &ec);

		uint32_t creator_type() const;
		uint32_t file_type() const;

#if defined(AFP_WIN32)
		const uint8_t *data() const {
			return _afp.finder_info;
		}
		uint8_t *data() {
			return _afp.finder_info;
		}
		uint16_t prodos_file_type() const {
			return _afp.prodos_file_type;
		}
		uint32_t prodos_aux_type() const {
			return _afp.prodos_aux_type;
		}
#else
		const uint8_t *data() const {
			return _finder_info;
		}
		uint8_t *data() {
			return _finder_info;
		}
		uint16_t prodos_file_type() const {
			return _prodos_file_type;
		}
		uint32_t prodos_aux_type() const {
			return _prodos_aux_type;
		}
#endif

		void set_prodos_file_type(uint16_t);
		void set_prodos_file_type(uint16_t, uint32_t);

		void set_file_type(uint32_t);
		void set_creator_type(uint32_t);

		bool is_text() const;
		bool is_binary() const;

		void clear();

		void close();

	private:


		#if defined(AFP_WIN32)
		void *_fd = (void *)-1;
		AFP_Info _afp;
		#else
		int _fd = -1;

		uint16_t _prodos_file_type = 0;
		uint32_t _prodos_aux_type = 0;
		uint8_t _finder_info[32] = {};
		#endif
	};


}
#undef AFP_WIN32

#endif
