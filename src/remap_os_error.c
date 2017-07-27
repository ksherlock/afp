/*
 * libstdc++ (cygwin, msys, mingw, etc) use posix errno for std::system_category()
 * rather than create a new category, just remap to a posix errno.
 */
#include <errno.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int remap_os_error(unsigned long e) {
	
	switch(e) {
	case NO_ERROR: return 0;
	default:
	#ifdef ELAST
		if (e > ELAST) return e;
	#endif 
		return EIO;

    case ERROR_INVALID_HANDLE:
    	return EBADF;

	case ERROR_CANTOPEN:
    case ERROR_CANTREAD:
    case ERROR_CANTWRITE:
    case ERROR_OPEN_FAILED:
    case ERROR_READ_FAULT:
    case ERROR_SEEK:
    case ERROR_WRITE_FAULT:
    	return EIO;

    case ERROR_ACCESS_DENIED:
    case ERROR_CANNOT_MAKE:
    case ERROR_CURRENT_DIRECTORY:
    case ERROR_INVALID_ACCESS:
    case ERROR_NOACCESS:
    case ERROR_SHARING_VIOLATION:
    case ERROR_WRITE_PROTECT:
    	return EACCES;

    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
    	return EEXIST;

    case ERROR_BAD_UNIT:
    case ERROR_DEV_NOT_EXIST:
    case ERROR_INVALID_DRIVE:
    	return ENODEV;

    case ERROR_BUFFER_OVERFLOW:
    	return ENAMETOOLONG;

    case ERROR_BUSY:
    case ERROR_BUSY_DRIVE:
    case ERROR_DEVICE_IN_USE:
    case ERROR_OPEN_FILES:
    	return EBUSY;

    case ERROR_DIR_NOT_EMPTY:
    	return ENOTEMPTY;

    case ERROR_DIRECTORY:
    case ERROR_INVALID_NAME:
    case ERROR_NEGATIVE_SEEK:
    case ERROR_INVALID_PARAMETER:
    	return EINVAL;

    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
    	return ENOSPC;

    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
    	return ENOENT;

    case ERROR_INVALID_FUNCTION:
    	return ENOSYS;

    case ERROR_LOCK_VIOLATION:
    case ERROR_LOCKED:
    	return ENOLCK;

    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
    case ERROR_NOT_ENOUGH_QUOTA:
    	return ENOMEM;

    case ERROR_NOT_READY:
    case ERROR_RETRY:
    	return EAGAIN;

    case ERROR_NOT_SAME_DEVICE:
    	return EXDEV;

    case ERROR_OPERATION_ABORTED:
    	return ECANCELED;

    case ERROR_TOO_MANY_OPEN_FILES:
    	return EMFILE;
	}

}