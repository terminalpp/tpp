#pragma once
#if (defined ARCH_WINDOWS)
#include <fileapi.h>
#include <ShlObj_core.h>
#else
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#endif

#include <filesystem>

#include "helpers.h"
#include "string.h"

/** Filesystem functions and helpful classes

    Adds extra filesystem routines such as creation of unique folders, temporary folder, etc. and wraps around the std::filesystem with UTF8 strings instead of the platform specific paths, which are used under the hood. 
 
    To include thsi header, the the `stdc++fs` library must be included, such as typing the following in CMakeLists:

    target_link_libraries(PROJECT stdc++fs)    

 */

namespace helpers {

    /** Returns the hostname of the current computer. 
     */
    inline std::string GetHostname() {
#if (defined ARCH_WINDOWS)
        TCHAR buffer[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD bufSize = MAX_COMPUTERNAME_LENGTH + 1;
        OSCHECK(GetComputerName(buffer, &bufSize));
        buffer[bufSize] = 0;
        return UTF16toUTF8(buffer);
#else
        char buffer[HOST_NAME_MAX];
        gethostname(buffer, HOST_NAME_MAX);
        return std::string(buffer);
#endif
    }

    inline std::string GetFilename(std::string const & path) {
#if (defined ARCH_WINDOWS)
        std::filesystem::path p(UTF8toUTF16(path));
        return p.filename().u8string();
#else
        std::filesystem::path p(path);
        return p.filename();
#endif
    }

    inline std::string JoinPath(std::string const & first, std::string const & second) {
#if (defined ARCH_WINDOWS)
        std::filesystem::path p(UTF8toUTF16(first));
        p.append(UTF8toUTF16(second));
        return p.u8string();
#else
        std::filesystem::path p(first);
        p.append(second);
        return p.u8string();
#endif
    }

    inline bool PathExists(std::string const path) {
#if (defined ARCH_WINDOWS)
        std::filesystem::path p(UTF8toUTF16(path));
#else
        std::filesystem::path p(path);
#endif
        return std::filesystem::exists(p);
    }

    /** Creates the given path. 
     
        Returns true if the path had to be created, false if it already exists.
     */
    inline bool CreatePath(std::string const & path) {
        return std::filesystem::create_directories(path);
    }

    /** Returns the directory in which local application settings should be stored on given platform. 
     */
    inline std::string LocalSettingsDir() {
#if (defined ARCH_WINDOWS)
		wchar_t * wpath;
		OSCHECK(SHGetKnownFolderPath(
			FOLDERID_LocalAppData,
			0, 
			nullptr,
			& wpath
		) == S_OK) << "Unable to determine stetings folder location";
		std::string path(helpers::UTF16toUTF8(wpath));
		CoTaskMemFree(wpath);
        return path;
#else
        std::string path(getpwuid(getuid())->pw_dir);
        return path + "/.config";
#endif
    }

    /** Returns the directory where temporary files should be stored. 
     
        This usually goes to `%APP_DATA%/local/Temp` on Windows and `/tmp` on Linux and friends. 
     */
    inline std::string TempDir() {
#if (defined ARCH_WINDOWS)
        return UTF16toUTF8(std::filesystem::temp_directory_path().c_str());
#else
        return std::filesystem::temp_directory_path().c_str();
#endif
    }

    /** Given an existing folder, creates a new path that is guaranteed not to exist in the folder. 
     
        The path is a string with given prefix followed by the specified number of alphanumeric characters. Once created, the path is checked for existence and if not found is returned.
     */
    inline std::string UniqueNameIn(std::string const & folder, std::string const & prefix, size_t length = 16) {
        while (true) {
            std::filesystem::path p{folder};
            std::string x{CreateRandomAlphanumericString(length)};
            p.append(prefix + x);
            if (! std::filesystem::exists(p))
#if (defined ARCH_WINDOWS)
                return UTF16toUTF8(p.c_str());
#else
                return p.c_str();
#endif
        }
    }

    /** Temporary folder with optional cleanup.

        Creates a temporary folder in the appropriate location for given platform. The folder may start with a selected prefix, which is not necessary but may help a human orient in the temp directories. The temporary folder and its contents are deleted when the object is destroyed, unless the `deleteWhenDestroyed` argument is set to false, in which case the folder and its files are never deleted by the object and their deletion is either up to the user, or more likely to the operating system. 
     */
    class TemporaryFolder {
    public:

        /** Creates the temporary folder of given prefix and persistence. 
         */
        TemporaryFolder(std::string prefix = "", bool deleteWhenDestroyed = true):
            path_(UniqueNameIn(TempDir(), prefix)),
            deleteWhenDestroyed_(deleteWhenDestroyed) {
            std::filesystem::create_directory(path_);
        }

        TemporaryFolder(TemporaryFolder && from):
            path_(from.path_),
            deleteWhenDestroyed_(from.deleteWhenDestroyed_) {
            from.deleteWhenDestroyed_ = false;
        }

        TemporaryFolder(TemporaryFolder const &) = delete;

        /** If not persistent, deletes the temporary folder and its contents. 
         */
        ~TemporaryFolder() {
            if (deleteWhenDestroyed_)
                std::filesystem::remove_all(path_);
        }

        TemporaryFolder & operator = (TemporaryFolder && from) {
            if (& from != this) {
                if (deleteWhenDestroyed_)
                    std::filesystem::remove_all(path_);
                path_ = from.path_;
                deleteWhenDestroyed_ = from.deleteWhenDestroyed_;
                from.deleteWhenDestroyed_ = false;
            }
            return *this;
        }

        TemporaryFolder & operator = (TemporaryFolder const &) = delete;

        /** Returns the path of the temporary object. 
         */
        std::string const & path() const {
            return path_;
        }

    private:
        std::string path_;
        bool deleteWhenDestroyed_;

    };

} // namespace helpers