/*
 * Released into the public domain by Tré Dudman - 2024
 * For licensing and more info see https://github.com/Tremus/xhl
 *
 * stdio.h FILE* replacement for Windows & macOS
 * Contains a few extra nice to have features not offered by libc
 * All strings are assumed to be NULL terminated with UTF8 encoding
 *
 * WHY NOT JUST USE LIBC?
 * Windows C/C++ runtime can break when using Cyrillic characters for example in your file paths.
 * Presumably Microsofts implementation of libc interprets strings as ANSI rather than UTF8, which is a bummer for
 * anyone using non-latin characters in their Windows username.
 *
 * This problem alone was enough of a reason to create this library. Under the hood, your UTF8 paths are converted to
 * UTF16 on Windows which eliminates the above problem. Enjoy!
 *
 * LIMITATIONS:
 * Windows support file paths with names up to 32,767 wide characters. This library lazily supports a maximum of
 * MAX_PATH (260) wide characters using stack memory to avoid using malloc or similar. If you need longer paths, you
 * will have to add this yourself!
 * From what I've read around the internet, Windows File Explorer still doesn't support long paths. Users of your
 * software likely won't have or need any long paths. This may change in future.
 * https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file#maximum-path-length-limitation
 *
 * FEATURES:
 * - file read/write/append
 * - file/folder exists Y/N
 * - file/folder rename/move
 * - folder create
 * - file/folder move to bin
 * - file/folder delete
 * - open folders in Finder/Explorer (opt. with preselected file/folder)
 * - folder scan. Useful for custom search algorithms
 * - get platform specific paths eg. \AppData\, /Application Support/, {user}/Desktop, etc.
 * - file watching. Useful for devtools like recompiling code on change, or maintaining an accurate file tree
 *
 * BUILDING:
 * Simply #define XHL_FILES_IMPL in one of your build targets before including this header
 * MacOS: Requires Objective-C/Objective-C++ for some functions. Supports both ARC and no ARC
 *
 * LINKING:
 * Windows: Kernel32 Shell32 Shlwapi
 * macOS: -framework AppKit
 *
 * CREDITS:
 * Randy Gaul. 'xfiles_list' is a modified version of 'cf_scan' from the cute_files library
 * https://github.com/RandyGaul/cute_headers_deprecated/blob/master/cute_files.h
 */

// Example program 1: File watching
#if 0
#define XHL_FILES_IMPL

#include <signal.h>
#include <stdio.h>
#include <xhl/files.h>

int  g_running = 1;
void ctrl_c_callback(int code)
{
    fprintf(stderr, "Terminating\n");
    g_running = 0;
}

void cb_onfilechange(enum XFILES_WATCH_TYPE type, const char* path, void* udata)
{
    switch (type)
    {
    case XFILES_WATCH_CREATED:
        fprintf(stderr, "Created %s\n", path);
        break;
    case XFILES_WATCH_DELETED:
        fprintf(stderr, "Deleted %s\n", path);
        break;
    case XFILES_WATCH_MODIFIED:
        fprintf(stderr, "Modified %s\n", path);
        // Recompile program?
        // Recompile shader?
        // Note that if you are modifying files in an IDE with a linter for formatter, you will likely get multiple
        // 'modified' callbacks. If you're hoping to recompile code, you may want to write your own throttle for
        // whatever actions you make in response
        break;
    }
}

int main()
{
    fprintf(stderr, "Press Crtl+C to exit\n");
    g_running = 1;
    signal(SIGINT, ctrl_c_callback);
    // setup event queue
    xfiles_watch_context_t ctx = xfiles_watch_create("/path/to/directory", NULL, cb_onfilechange);
    while (g_running)
    {
        xfiles_watch_flush(ctx); // poll for items in queue, trigger cb_onfilechange()
        Sleep(50); // Windows, sleep 50ms
        usleep(50000) // Unix, sleep 50ms
    }
    xfiles_watch_destroy(ctx); // cleanup

    return 0;
}
#endif // Example program 1: File watching

// Example program 2: Recursive file searching
#if 0
#define XHL_FILES_IMPL
#include <stdio.h>
#include <xhl/files.h>

void cb_onfile(void* data, const xfiles_list_item_t* item)
{
    const char* name = item->path + item->name_idx;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return;

    fprintf(stderr, "Entry: %s\n", item->path);
    if (item->is_dir)
    {
        xfiles_list(item->path, data, cb_onfile);
    }
    else
    {
        int* p_file_counter = data;
        (*p_file_counter)++;
    }
}

int main()
{
    int file_counter = 0;
    xfiles_list("/path/to/directory", &file_counter, cb_onfile);
    fprintf(stderr, "Found %d files\n", file_counter);
    return 0;
}
#endif // Example program 2: Recursive file searching

#ifndef XHL_FILES_H
#define XHL_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define XFILES_DIR_STR      "\\"
#define XFILES_DIR_CHAR     '\\'
#define XFILES_BROWSER_NAME "File Explorer"
#define XFILES_TRASH_NAME   "Recycle Bin"
#else
#define XFILES_DIR_STR      "/"
#define XFILES_DIR_CHAR     '/'
#define XFILES_BROWSER_NAME "Finder"
#define XFILES_TRASH_NAME   "Trash"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// On success, returns file or directory name, else returns NULL
const char* xfiles_get_name(const char* path);
// On success, returns file extension, else returns NULL
const char* xfiles_get_extension(const char* name);

// Returns true if directory or file exists
bool xfiles_exists(const char* path);
// Returns true if directory, false in any other case
bool xfiles_is_directory(const char* path);
// Returns true if directory was created, or if it already exists
bool xfiles_create_directory(const char* path);
// Creates directory and any required parent directories, then returns true if the exists or was craeted.
bool xfiles_create_directory_recursive(const char* path);

// Returns only true on success and sets 'out' and 'outlen' with file contents and size
// Must release 'out' with xfiles_read_free()
bool xfiles_read(const char* path, void** out, size_t* outlen);
// Frees a file's contents allocated with xfiles_read()
void xfiles_read_free(void *data);
// Creates file if it doesn't exist with default access permissions.
// If file already exists, it overwrites all contents.
bool xfiles_write(const char* path, const void* in, size_t inlen);
// Creates file if it doesn't exist with default access permissions.
// Writes data starting from the end of a file, leaving old contents intact
bool xfiles_append(const char* path, const char* in, size_t inlen);
// Renames / moves file or folder
// You are advised to check for path collisions using xfiles_exists() beforehand
bool xfiles_move(const char* from, const char* to);

// Moves the file to:
// Win: Recycle Bin /
// OSX: Trash
bool xfiles_trash(const char* path);
// No bins. File is deleted and is likely unrecoverable
bool xfiles_delete(const char* path);
// Opens OS file or folder as if it was opened with
// Win: File Explorer /
// OSX: Finder
bool xfiles_open_file_explorer(const char* path);
// Opens OS file browsing app with path selected
// Win: File Explorer /
// OSX: Finder
bool xfiles_select_in_file_explorer(const char* path);

typedef enum XFilesUserDirectory
{
    // Windows: {letter}:\\Users\\{username}
    // macOS: /Users/{username}
    XFILES_USER_DIRECTORY_HOME,
    // Windows: [HOME]\\AppData\\Roaming
    // macOS: [HOME]/Library/Application\ Support
    XFILES_USER_DIRECTORY_APPDATA,
    XFILES_USER_DIRECTORY_DESKTOP,
    XFILES_USER_DIRECTORY_DOCUMENTS,
    XFILES_USER_DIRECTORY_DOWNLOADS,
    XFILES_USER_DIRECTORY_MUSIC,
    XFILES_USER_DIRECTORY_PICTURES,
    XFILES_USER_DIRECTORY_VIDEOS,
    XFILES_USER_DIRECTORY_COUNT,
} XFilesUserDirectory;

// Returns number of bytes written to 'char* out', excluding the null terminating byte
int xfiles_get_user_directory(char* out, size_t outlen, XFilesUserDirectory loc);

typedef enum XFilesMetadataType
{
    XFILES_METADATA_TYPE_UNKNOWN = 0,
    XFILES_METADATA_TYPE_FILE,
    XFILES_METADATA_TYPE_DIRECTORY,
    XFILES_METADATA_TYPE_SYMLINK,
    XFILES_METADATA_TYPE_BLOCK_DEVICE,
    XFILES_METADATA_TYPE_CHAR_DEVICE,
    XFILES_METADATA_TYPE_FIFO,
    XFILES_METADATA_TYPE_SOCKET
} XFilesMetadataType;

typedef struct XFilesMetadata
{
    int                last_error; // errno on macOS, GetLastError() on Windows
    XFilesMetadataType type;

    uint64_t size_bytes;

    // Convenience
    bool is_readonly;
    bool is_hidden;
    bool is_system;
    bool is_archive;    // Windows only, its for backing up files
    bool is_compressed; // (UF_COMPRESSED, FILE_ATTRIBUTE_COMPRESSED). Not to be confused with zip/7z/tar/gz/rar
    bool is_encrypted;
    bool is_executable; // +x user permissions on posix. Lazy extension checking on Windwos
    bool is_symlink;

    // Unix epoch times in nanoseconds
    uint64_t creation_time_ns;      // File creation time (birthtime / CreationTime)
    uint64_t modification_time_ns;  // Last write time (mtime / LastWriteTime)
    uint64_t access_time_ns;        // Last access time (atime / LastAccessTime)
    uint64_t status_change_time_ns; // POSIX ctime (macOS only, 0 on Windows)

    // Stuff you probably dont want

    // Might get rid of this. Can't imagine a use for it
    uint64_t file_index;           // (st_ino / (nFileIndexHigh << 32 | nFileSizeLow))
    uint32_t num_links;            // (st_nlink / nNumberOfLinks)
    uint32_t volume_serial_number; // (st_dev / dwVolumeSerialNumber)

    // Windows fields you might want
    uint32_t _dwFileAttributes; // BY_HANDLE_FILE_INFORMATION.dwFileAttributes
    uint32_t _dwReparseTag;     // FILE_ATTRIBUTE_TAG_INFO.ReparseTag

    // POSIX fields (stat64) you might want
    uint32_t _st_mode;    // File mode bits (POSIX)
    uint32_t _st_blksize; // Preferred I/O block size
    uint64_t _st_blocks;  // Number of 512-byte blocks allocated
    uint32_t _st_flags;   // User-defined flags (macOS BSD flags)
    uint32_t _st_gen;     // File generation number (macOS)
} XFilesMetadata;

XFilesMetadata xfiles_get_metadata(const char* path);

typedef struct xfiles_list_item_t
{
    char path[1024];

    unsigned path_len;
    unsigned name_idx;
    unsigned ext_idx;
    bool     is_dir;
} xfiles_list_item_t;

typedef void(xfiles_list_callback_t)(void* data, const xfiles_list_item_t* item);
// Calls the above callback for each item in a directory
void xfiles_list(const char* path, void* data, xfiles_list_callback_t* cb);

enum XFILES_WATCH_TYPE
{
    XFILES_WATCH_CREATED,
    XFILES_WATCH_DELETED,
    XFILES_WATCH_MODIFIED,
};
typedef void (*xfiles_watch_callback_t)(enum XFILES_WATCH_TYPE type, const char* path, void* udata);
typedef void* xfiles_watch_context_t;

// Sets up an event queue for file change notifications
xfiles_watch_context_t xfiles_watch_create(const char* path, void* udata, xfiles_watch_callback_t cb);
// Process all events in the queue
void xfiles_watch_flush(xfiles_watch_context_t ctx);
void xfiles_watch_destroy(xfiles_watch_context_t ctx);

#ifdef __cplusplus
}
#endif

#ifdef XHL_FILES_IMPL

#define XFILES_ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#if !defined(XFILES_MALLOC) || !defined(XFILES_REALLOC) || !defined(XFILES_FREE)
#include <stdlib.h>
#define XFILES_MALLOC(size)       malloc(size)
#define XFILES_REALLOC(ptr, size) realloc(ptr, size)
#define XFILES_FREE(ptr)          free(ptr)
#endif

#ifndef XFILES_ASSERT
#ifdef NDEBUG
// clang-format off
#define XFILES_ASSERT(cond) do { (void)(cond); } while (0)
// clang-format on
#else
#ifdef _WIN32
#define XFILES_ASSERT(cond) (cond) ? (void)0 : __debugbreak()
#else // #if __APPLE__
#define XFILES_ASSERT(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif // _WIN32
#endif // NDEBUG
#endif // XFILES_ASSERT

#ifdef _WIN32
#include <Windows.h>

#include <Shlobj.h>
#include <Shlwapi.h>
#include <assert.h>
#include <shellapi.h>
#include <stdio.h>

#pragma comment(lib, "Shlwapi.lib")

bool xfiles_exists(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-pathfileexistsw
    // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar

    WCHAR pathunicode[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, pathunicode, XFILES_ARRLEN(pathunicode));
    XFILES_ASSERT(num);
    if (num)
        return PathFileExistsW(pathunicode);
    return false;
}

bool xfiles_is_directory(const char* path)
{
    bool  is_directory = false;
    DWORD attr         = 0;
    WCHAR WPath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, WPath, ARRAYSIZE(WPath));
    if (num)
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileattributesw
        attr = GetFileAttributesW(WPath);
    }
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) && attr != INVALID_FILE_ATTRIBUTES)
        is_directory = true;
    return is_directory;
}

bool xfiles_create_directory(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectoryw
    WCHAR DirPath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, DirPath, XFILES_ARRLEN(DirPath));
    XFILES_ASSERT(num);
    if (num)
    {
        BOOL ret = CreateDirectoryW(DirPath, 0);

        if (ret) // any non-zero is a success
        {
            return true;
        }
        else
        {
            DWORD err = GetLastError();
            if (err == ERROR_ALREADY_EXISTS)
                return true; // close enough to "created a directory"
            if (err == ERROR_PATH_NOT_FOUND)
                return false;
        }
    }
    return false;
}

bool xfiles_read(const char* path, void** out, size_t* outlen)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfilesizeex
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile

    void*         data     = NULL;
    HANDLE        hFile    = NULL;
    LARGE_INTEGER FileSize = {0};
    BOOL          ok       = FALSE;

    WCHAR FileName[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FileName, XFILES_ARRLEN(FileName));
    XFILES_ASSERT(num);
    if (num)
    {
    retry:
        hFile = CreateFileW(
            FileName,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            int err = GetLastError();
            if (err == ERROR_SHARING_VIOLATION)
            {
                // A process may currently have exclusive access to the file.
                // Wait until they close their handle.
                Sleep(0);
                goto retry;
            }
            if (err == ERROR_ACCESS_DENIED)
            {
                // NOTE: you may get this error when trying to "read" a directory. Check if a directory already exists
                // at this path
            }
            XFILES_ASSERT(hFile != INVALID_HANDLE_VALUE);
        }
        if (hFile != INVALID_HANDLE_VALUE)
        {
            ok = GetFileSizeEx(hFile, &FileSize);
            XFILES_ASSERT(ok);
            if (ok)
            {
                data = XFILES_MALLOC(FileSize.QuadPart);
                ok   = ReadFile(hFile, data, FileSize.QuadPart, NULL, NULL);
                XFILES_ASSERT(ok);
                if (ok)
                {
                    *out    = data;
                    *outlen = FileSize.QuadPart;
                }
                else
                {
                    XFILES_FREE(data);
                    data              = NULL;
                    FileSize.QuadPart = 0;
                }
            }
            ok &= CloseHandle(hFile);
            XFILES_ASSERT(ok);
        }
    }

    return ok;
}

bool xfiles_write(const char* path, const void* in, size_t inlen)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile

    WCHAR  FilePath[MAX_PATH];
    HANDLE hFile         = NULL;
    BOOL   ok            = FALSE;
    DWORD  nBytesWritten = 0;
    int    num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        hFile = CreateFileW(
            FilePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            int err = GetLastError();
            if (err == ERROR_ACCESS_DENIED)
            {
                // NOTE: you may get this error when trying to "write" to a directory. Check if a directory already
                // exists at this path
            }
            XFILES_ASSERT(hFile != INVALID_HANDLE_VALUE);
        }
        if (hFile != INVALID_HANDLE_VALUE)
        {
            ok = WriteFile(hFile, in, inlen, &nBytesWritten, NULL);
            XFILES_ASSERT(ok);
            ok &= CloseHandle(hFile);
            XFILES_ASSERT(ok);
        }
    }

    return ok;
}

bool xfiles_append(const char* path, const char* in, size_t inlen)
{
    WCHAR  FilePath[MAX_PATH];
    HANDLE hFile         = NULL;
    BOOL   ok            = FALSE;
    DWORD  nBytesWritten = 0;
    int    num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        hFile =
            CreateFileW(FilePath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        XFILES_ASSERT(hFile != INVALID_HANDLE_VALUE);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            ok = WriteFile(hFile, in, inlen, &nBytesWritten, NULL);
            XFILES_ASSERT(ok);
            ok &= CloseHandle(hFile);
            XFILES_ASSERT(ok);
        }
    }

    return ok;
}

bool xfiles_move(const char* from, const char* to)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefilew
    WCHAR FromPath[MAX_PATH];
    WCHAR ToPath[MAX_PATH];
    int   num1 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, from, -1, FromPath, XFILES_ARRLEN(FromPath));
    int   num2 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, to, -1, ToPath, XFILES_ARRLEN(ToPath));
    if (num1 && num2)
        return MoveFileW(FromPath, ToPath);
    return false;
}

bool xfiles_trash(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shfileoperationw
    // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shfileopstructw
    WCHAR PathList[MAX_PATH + 8] = {0};

    int num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, PathList, XFILES_ARRLEN(PathList));
    XFILES_ASSERT(num);
    if (num)
    {
        SHFILEOPSTRUCTW FileOp = {0};

        FileOp.wFunc  = FO_DELETE;
        FileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_NORECURSION |
                        FOF_RENAMEONCOLLISION | FOF_SILENT;
        FileOp.pFrom = PathList;

        int ret = SHFileOperationW(&FileOp);
        XFILES_ASSERT(ret == 0);
        return ret == 0;
    }
    return false;
}

bool xfiles_delete(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefilew
    WCHAR FilePath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        BOOL ok = DeleteFileW(FilePath);
        XFILES_ASSERT(ok);
        return ok;
    }
    return false;
}

bool xfiles_open_file_explorer(const char* path)
{
    WCHAR FilePath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        // ShellExecuteW depends on Windows registry to find what application to use to with the verb "open"
        // It's possible for the registry to be poisened by 3rd party software or manual tweaking
        // This happened to me after downloading FilePilot, setting it as the default file browser, then uninstalling
        // it. After doing this ShellExecuteW would always return 'SE_ERR_NOASSOC'
        // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew
        INT_PTR ret = (INT_PTR)ShellExecuteW(NULL, L"open", FilePath, NULL, NULL, SW_SHOWDEFAULT);

        if (ret == SE_ERR_NOASSOC) // Oh dear, Windows can't figure out what application to use!?
        {
            // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecuteexw
            // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfow
            // https://learn.microsoft.com/en-us/windows/win32/shell/launch
            SHELLEXECUTEINFOW sei = {0};
            sei.cbSize            = sizeof(SHELLEXECUTEINFOW);
            sei.fMask             = 0;
            sei.lpVerb            = L"open";
            sei.lpFile            = L"explorer.exe"; // Explicitly use File Explorer
            sei.lpParameters      = FilePath;
            sei.nShow             = SW_SHOW;

            BOOL ok = ShellExecuteExW(&sei);

            ret = (INT_PTR)sei.hInstApp;
        }
        XFILES_ASSERT(ret > 32);

        return ret > 32;
    }
    return false;
}

bool xfiles_select_in_file_explorer(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shopenfolderandselectitems
    // https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-ilcreatefrompathw
    WCHAR FilePath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        LPITEMIDLIST pItemList = ILCreateFromPathW(FilePath);
        if (pItemList)
        {
            CoInitialize(NULL);
            HRESULT hres = SHOpenFolderAndSelectItems(pItemList, 0, 0, 0);
            XFILES_ASSERT(hres == S_OK);
            ILFree(pItemList);
            CoUninitialize();

            return hres = S_OK;
        }
    }
    return false;
}

int xfiles_get_user_directory(char* out, size_t outlen, XFilesUserDirectory loc)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
    // https://learn.microsoft.com/en-us/windows/win32/shell/knownfolderid

    static const KNOWNFOLDERID* FOLDER_IDS[] = {
        &FOLDERID_Profile,        // XFILES_USER_DIRECTORY_HOME
        &FOLDERID_RoamingAppData, // XFILES_USER_DIRECTORY_APPDATA
        &FOLDERID_Desktop,        // XFILES_USER_DIRECTORY_DESKTOP
        &FOLDERID_Documents,      // XFILES_USER_DIRECTORY_DOCUMENTS
        &FOLDERID_Downloads,      // XFILES_USER_DIRECTORY_DOWNLOADS
        &FOLDERID_Music,          // XFILES_USER_DIRECTORY_MUSIC
        &FOLDERID_Pictures,       // XFILES_USER_DIRECTORY_PICTURES
        &FOLDERID_Videos,         // XFILES_USER_DIRECTORY_VIDEOS
    };
#ifdef _MSC_VER
    static_assert(XFILES_ARRLEN(FOLDER_IDS) == XFILES_USER_DIRECTORY_COUNT, "");
#else
    _Static_assert(XFILES_ARRLEN(FOLDER_IDS) == XFILES_USER_DIRECTORY_COUNT, "");
#endif

#ifdef __cplusplus
#define XFILES_REF(ptr) *ptr
#else
#define XFILES_REF(ptr) ptr
#endif

    int   num  = 0;
    PWSTR Path = NULL;
    if (loc < 0)
        loc = (XFilesUserDirectory)0;
    if (loc >= XFILES_USER_DIRECTORY_COUNT)
        loc = (XFilesUserDirectory)(ARRAYSIZE(FOLDER_IDS) - 1);
    if (S_OK == SHGetKnownFolderPath(XFILES_REF(FOLDER_IDS[loc]), 0, NULL, &Path))
    {
        num = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Path, -1, out, outlen, NULL, NULL);
        if (num)
            num--; // remove count of null byte
        XFILES_ASSERT(num == strlen(out));
        CoTaskMemFree(Path);
    }

    return num;
}

// Mostly LLM generated. GLWTS
XFilesMetadata xfiles_get_metadata(const char* path)
{
    XFilesMetadata meta = {0};

    if (path == NULL || path[0] == '\0')
    {
        meta.last_error = -1;
        return meta;
    }

    // Open file
    HANDLE hFile = INVALID_HANDLE_VALUE;
    {
        WCHAR WPath[MAX_PATH] = {0};
        int   wlen            = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, WPath, ARRAYSIZE(WPath));
        if (wlen == 0)
        {
            meta.last_error = (int)GetLastError();
            return meta;
        }
        // Open with FILE_FLAG_BACKUP_SEMANTICS so directories also work,
        // and FILE_FLAG_OPEN_REPARSE_POINT so we stat the symlink itself.
        // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
        hFile = CreateFileW(
            WPath,
            FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
            NULL);
    }

    if (hFile == INVALID_HANDLE_VALUE)
    {
        meta.last_error = (int32_t)GetLastError();
        return meta;
    }

    BY_HANDLE_FILE_INFORMATION info;
    BOOL                       ok = GetFileInformationByHandle(hFile, &info);
    if (!ok)
    {
        meta.last_error = (int32_t)GetLastError();
        CloseHandle(hFile);
        return meta;
    }

    meta.size_bytes = ((uint64_t)info.nFileSizeHigh << 32) | (uint64_t)info.nFileSizeLow;

    // Convert FILETIME (100-ns intervals since 1601-01-01 UTC) to ns since 1970-01-01 UTC
    {
        // 116444736000000000 = number of 100ns ticks between 1601-01-01 and 1970-01-01
        const uint64_t EPOCH_DIFF_100NS = 116444736000000000ULL;

        ULARGE_INTEGER c, m, a;
        c.LowPart  = info.ftCreationTime.dwLowDateTime;
        c.HighPart = info.ftCreationTime.dwHighDateTime;
        m.LowPart  = info.ftLastWriteTime.dwLowDateTime;
        m.HighPart = info.ftLastWriteTime.dwHighDateTime;
        a.LowPart  = info.ftLastAccessTime.dwLowDateTime;
        a.HighPart = info.ftLastAccessTime.dwHighDateTime;

        meta.creation_time_ns      = (uint64_t)((c.QuadPart - EPOCH_DIFF_100NS) * 100ULL);
        meta.modification_time_ns  = (uint64_t)((m.QuadPart - EPOCH_DIFF_100NS) * 100ULL);
        meta.access_time_ns        = (uint64_t)((a.QuadPart - EPOCH_DIFF_100NS) * 100ULL);
        meta.status_change_time_ns = 0; // no direct POSIX ctime equivalent on Windows
    }

    meta.file_index           = ((uint64_t)info.nFileIndexHigh << 32) | (uint64_t)info.nFileIndexLow;
    meta.num_links            = info.nNumberOfLinks;
    meta.volume_serial_number = info.dwVolumeSerialNumber;
    meta._dwFileAttributes    = info.dwFileAttributes;

    // https://learn.microsoft.com/en-us/windows/win32/fileio/reparse-points
    if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
    {
        // Get the reparse tag to decide if it is really a symlink
        // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex
        // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ne-minwinbase-file_info_by_handle_class
        FILE_ATTRIBUTE_TAG_INFO tag_info;
        ok = GetFileInformationByHandleEx(hFile, FileAttributeTagInfo, &tag_info, sizeof(tag_info));
        if (ok)
        {
            meta._dwReparseTag = tag_info.ReparseTag;
            if (tag_info.ReparseTag == IO_REPARSE_TAG_SYMLINK || tag_info.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
            {
                meta.type       = XFILES_METADATA_TYPE_SYMLINK;
                meta.is_symlink = true;
            }
            else
            {
                meta.type = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? XFILES_METADATA_TYPE_DIRECTORY
                                                                               : XFILES_METADATA_TYPE_FILE;
            }
        }
    }
    else if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        meta.type = XFILES_METADATA_TYPE_DIRECTORY;
    }
    else
    {
        meta.type = XFILES_METADATA_TYPE_FILE;
    }

    meta.is_readonly   = (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
    meta.is_hidden     = (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    meta.is_system     = (info.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
    meta.is_archive    = (info.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0;
    meta.is_compressed = (info.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0;
    meta.is_encrypted  = (info.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) != 0;
    meta.is_executable = false; // Windows uses ACLs, not a simple bit; best-effort below
    {
        // Treat .exe/.bat/.cmd/.com as executable as a convenience heuristic
        const char* ext = xfiles_get_extension(path);
        int         c   = ext != NULL ? ext[1] : 0;
        if (c == 'e' || c == 'E' || c == 'b' || c == 'B' || c == 'c' || c == 'C')
        {
            ext++; // increment past "."
            if (_stricmp(ext, "exe") == 0 || _stricmp(ext, "bat") == 0 || _stricmp(ext, "cmd") == 0 ||
                _stricmp(ext, "com") == 0)
            {
                meta.is_executable = true;
            }
        }
    }

    CloseHandle(hFile);
    return meta;
}

void xfiles_list(const char* path, void* data, xfiles_list_callback_t* callback)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findnextfilew
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findclose

    HANDLE             hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW   FindData;
    xfiles_list_item_t Item;
    bool               HasNext = 0;

    {
        WCHAR PathUnicode[MAX_PATH] = {0};
        int   n =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, PathUnicode, XFILES_ARRLEN(PathUnicode) - 3);
        if (n)
        {
            PathUnicode[n] = 0;
            wcsncat(PathUnicode, L"\\*", XFILES_ARRLEN(PathUnicode) - n - 1);
            hFind = FindFirstFileW(PathUnicode, &FindData);
        }
    }

    HasNext = hFind != INVALID_HANDLE_VALUE;
    XFILES_ASSERT(hFind != INVALID_HANDLE_VALUE);

    Item.name_idx = snprintf(Item.path, sizeof(Item.path), "%s\\", path);

    while (HasNext)
    {
        int NameLen = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            FindData.cFileName,
            -1,
            Item.path + Item.name_idx,
            sizeof(Item.path) - Item.name_idx,
            NULL,
            NULL);
        XFILES_ASSERT(NameLen);
        if (NameLen == 0) // Failed conversion
            goto iterate;

        Item.path_len = Item.name_idx + NameLen - 1;

        Item.ext_idx = Item.name_idx;
        for (unsigned i = Item.name_idx; i < Item.path_len; i++)
            if (Item.path[i] == '.')
                Item.ext_idx = i;
        if (Item.ext_idx == Item.name_idx) // Failed to find extension
            Item.ext_idx = Item.path_len;

        XFILES_ASSERT(strlen(Item.path) == Item.path_len);
        Item.is_dir = FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        callback(data, &Item);

    iterate:
        HasNext = FindNextFileW(hFind, &FindData);
    }

    if (hFind != INVALID_HANDLE_VALUE)
        FindClose(hFind);
}

typedef struct XFWatchContext
{
    HANDLE hDirectory;
    // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped
    OVERLAPPED overlapped;

    BYTE buffer[1024 * 4];

    int  pathlen;
    char path[MAX_PATH];

    void*                   udata;
    xfiles_watch_callback_t callback;
} XFWatchContext;

void _xfiles_watch_init_overlapped(XFWatchContext* ctx)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw
    BOOL  bWatchSubtree  = TRUE;
    DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                           // FILE_NOTIFY_CHANGE_ATTRIBUTES |
                           FILE_NOTIFY_CHANGE_LAST_WRITE;
    BOOL ok = ReadDirectoryChangesW(
        ctx->hDirectory,
        ctx->buffer,
        sizeof(ctx->buffer),
        bWatchSubtree,
        dwNotifyFilter,
        NULL,
        &ctx->overlapped,
        NULL);
    XFILES_ASSERT(ok);
}

xfiles_watch_context_t xfiles_watch_create(const char* path, void* udata, xfiles_watch_callback_t cb)
{
    XFWatchContext* ctx = (XFWatchContext*)XFILES_MALLOC(sizeof(*ctx));

    WCHAR wPath[MAX_PATH] = {0};
    int   num             = 0;

    memset(ctx, 0, sizeof(*ctx));

    ctx->pathlen  = strlen(path);
    ctx->udata    = udata;
    ctx->callback = cb;

    // Remove backslash
    if (ctx->pathlen > 0 && (path[ctx->pathlen - 1] == '\\' || path[ctx->pathlen - 1] == '/'))
        ctx->pathlen--;
    snprintf(ctx->path, sizeof(ctx->path), "%.*s", ctx->pathlen, path);

    num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ctx->path, ctx->pathlen, wPath, XFILES_ARRLEN(wPath));
    XFILES_ASSERT(num);

    ctx->hDirectory = CreateFileW(
        wPath,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);
    XFILES_ASSERT(ctx->hDirectory != INVALID_HANDLE_VALUE);

    if (ctx->hDirectory != INVALID_HANDLE_VALUE)
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createeventw
        ctx->overlapped.hEvent = CreateEventW(NULL, FALSE, 0, NULL);
        XFILES_ASSERT(ctx->overlapped.hEvent);

        _xfiles_watch_init_overlapped(ctx);
    }

    // Handle failure
    if (ctx->hDirectory == INVALID_HANDLE_VALUE || ctx->overlapped.hEvent == NULL)
    {
        xfiles_watch_destroy(ctx);
        ctx = NULL;
    }

    return ctx;
}

void xfiles_watch_flush(xfiles_watch_context_t _ctx)
{
    XFWatchContext* ctx = (XFWatchContext*)_ctx;

    if (ctx->overlapped.Internal == STATUS_PENDING)
        return;

    // https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
    DWORD result = WaitForSingleObject(ctx->overlapped.hEvent, 0);

    if (result == WAIT_OBJECT_0)
    {
        DWORD dwNumberOfBytesTransferred = 0;
        DWORD dwOffset                   = 0;

        // https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult
        BOOL bWait   = FALSE;
        BOOL success = GetOverlappedResult(ctx->hDirectory, &ctx->overlapped, &dwNumberOfBytesTransferred, bWait);

        while (success && dwOffset < dwNumberOfBytesTransferred)
        {
            FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)(ctx->buffer + dwOffset);

            enum XFILES_WATCH_TYPE type = ((enum XFILES_WATCH_TYPE)0xffffffff); // invalid
            if (pNotify->Action == FILE_ACTION_ADDED)
                type = XFILES_WATCH_CREATED;
            if (pNotify->Action == FILE_ACTION_REMOVED)
                type = XFILES_WATCH_DELETED;
            if (pNotify->Action == FILE_ACTION_MODIFIED)
                type = XFILES_WATCH_MODIFIED;

            if (type >= 0)
            {
                char fp[MAX_PATH] = {0};
                int  NameLength   = pNotify->FileNameLength / sizeof(wchar_t);

                int fplen  = snprintf(fp, sizeof(fp), "%.*s" XFILES_DIR_STR, ctx->pathlen, ctx->path);
                fplen     += WideCharToMultiByte(
                    CP_UTF8,
                    WC_ERR_INVALID_CHARS,
                    pNotify->FileName,
                    NameLength,
                    fp + fplen,
                    sizeof(fp) - fplen,
                    NULL,
                    NULL);

                ctx->callback(type, fp, ctx->udata);
            }

            dwOffset += pNotify->NextEntryOffset;
            if (pNotify->NextEntryOffset == 0)
                break;
        }

        // After events are flushed we need to call ReadDirectoryChangesW() to prepare our event queue again
        _xfiles_watch_init_overlapped(ctx);
    }
}

void xfiles_watch_destroy(xfiles_watch_context_t _ctx)
{
    XFWatchContext* ctx = (XFWatchContext*)_ctx;
    if (ctx->overlapped.hEvent)
        CloseHandle(ctx->overlapped.hEvent);
    if (ctx->hDirectory)
        CloseHandle(ctx->hDirectory);
}

#endif // _WIN32

#ifdef __APPLE__
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <unistd.h>

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/access.2.html
bool xfiles_exists(const char* path) { return access(path, F_OK) == 0; }

bool xfiles_is_directory(const char* path)
{
    bool is_directory = false;

    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html
    struct stat st = {0};
    lstat(path, &st);
    if (S_ISDIR(st.st_mode))
        is_directory = true;
    return is_directory;
}

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/mkdir.2.html
bool xfiles_create_directory(const char* path)
{
    int ret = mkdir(path, 0777);
    if (ret != 0)
    {
        int err = errno;
        XFILES_ASSERT(err == EEXIST);
        if (err == EEXIST)
            return true;
    }
    return ret == 0;
}

bool xfiles_read(const char* path, void** out, size_t* outlen)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/open.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fstat.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/read.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/close.2.html

    int         fd    = -1;
    int         ret   = -1;
    struct stat info  = {0};
    void*       data  = NULL;
    ssize_t     nread = -1;

    fd = open(path, O_RDONLY);
    XFILES_ASSERT(fd != -1);
    if (fd != -1)
    {
        ret = fstat(fd, &info);
        XFILES_ASSERT(ret != -1);
        if (ret != -1)
        {
            data  = XFILES_MALLOC(info.st_size);
            nread = read(fd, data, info.st_size);
            XFILES_ASSERT(nread != -1);
            if (nread == -1)
            {
                XFILES_FREE(data);
                data = NULL;
            }
            else
            {
                *out    = data;
                *outlen = info.st_size;
            }
        }
        close(fd);
    }

    return nread != -1;
}

bool xfiles_write(const char* path, const void* in, size_t inlen)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/write.2.html#//apple_ref/doc/man/2/write
    int     fd;
    ssize_t nwritten = -1;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    XFILES_ASSERT(fd != -1);
    if (fd != -1)
    {
        nwritten = write(fd, in, inlen);
        XFILES_ASSERT(nwritten != -1);
        close(fd);
    }
    return nwritten != -1;
}

bool xfiles_append(const char* path, const char* in, size_t inlen)
{
    int     fd;
    ssize_t nwritten = -1;

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0777);
    XFILES_ASSERT(fd != -1);
    if (fd != -1)
    {
        nwritten = write(fd, in, inlen);
        XFILES_ASSERT(nwritten != -1);
        close(fd);
    }
    return nwritten != -1;
}

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/rename.2.html
bool xfiles_move(const char* from, const char* to) { return 0 == rename(from, to); }

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/unlink.2.html
bool xfiles_delete(const char* path) { return unlink(path) == 0; }

XFilesMetadata xfiles_get_metadata(const char* path)
{
    XFilesMetadata meta = {0};

    if (path == NULL || path[0] == '\0')
    {
        meta.last_error = -1;
        return meta;
    }

    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html
    struct stat st;
    // lstat so symlinks are reported as symlinks
    if (lstat(path, &st) != 0)
    {
        meta.last_error = (int32_t)errno;
        return meta;
    }

    // Unified fields
    meta.size_bytes       = (uint64_t)st.st_size;
    meta.creation_time_ns = (uint64_t)st.st_birthtimespec.tv_sec * 1000000000LL + (uint64_t)st.st_birthtimespec.tv_nsec;
    meta.modification_time_ns  = (uint64_t)st.st_mtimespec.tv_sec * 1000000000LL + (uint64_t)st.st_mtimespec.tv_nsec;
    meta.access_time_ns        = (uint64_t)st.st_atimespec.tv_sec * 1000000000LL + (uint64_t)st.st_atimespec.tv_nsec;
    meta.status_change_time_ns = (uint64_t)st.st_ctimespec.tv_sec * 1000000000LL + (uint64_t)st.st_ctimespec.tv_nsec;
    meta.file_index            = st.st_ino;
    meta.num_links             = st.st_nlink;
    meta.volume_serial_number  = st.st_dev;

    meta._st_mode    = st.st_mode;
    meta._st_blksize = st.st_blksize;
    meta._st_blocks  = st.st_blocks;
    meta._st_flags   = st.st_flags;
    meta._st_gen     = st.st_gen;

    // Determine type
    if (S_ISREG(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_FILE;
    }
    else if (S_ISDIR(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_DIRECTORY;
    }
    else if (S_ISLNK(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_SYMLINK;
    }
    else if (S_ISBLK(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_BLOCK_DEVICE;
    }
    else if (S_ISCHR(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_CHAR_DEVICE;
    }
    else if (S_ISFIFO(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_FIFO;
    }
    else if (S_ISSOCK(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_SOCKET;
    }

    // Convenience boolean flags (BSD-style)
    // st_flags on macOS: UF_HIDDEN, UF_IMMUTABLE, UF_COMPRESSED, etc.
    meta.is_readonly   = ((st.st_mode & 0222) == 0);       // no write bits for anyone
    meta.is_hidden     = (st.st_flags & 0x00008000u) != 0; // UF_HIDDEN
    meta.is_system     = (st.st_flags & 0x00080000u) != 0; // SF_RESTRICTED, rough analogue
    meta.is_archive    = false;                            // No direct analogue on macOS
    meta.is_compressed = (st.st_flags & 0x00000020u) != 0; // UF_COMPRESSED
    meta.is_encrypted  = false;                            // FileVault is volume-level; per-file is not stable here
    meta.is_executable = (st.st_mode & 0111) != 0;
    meta.is_symlink    = S_ISLNK(st.st_mode) ? true : false;

    return meta;
}

void xfiles_list(const char* path, void* data, xfiles_list_callback_t* callback)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/opendir.3.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/readdir.3.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/closedir.3.html

    DIR*               dir   = NULL;
    struct dirent*     entry = NULL;
    xfiles_list_item_t item;

    dir = opendir(path);
    if (dir)
        entry = readdir(dir);

    item.name_idx = snprintf(item.path, sizeof(item.path), "%s/", path);

    while (entry)
    {
        item.path_len = item.name_idx + entry->d_namlen;

        XFILES_ASSERT(item.path_len < sizeof(item.path));
        if (item.path_len >= sizeof(item.path)) // Guard overflow
            goto iterate;

        memcpy(&item.path[item.name_idx], entry->d_name, entry->d_namlen);
        item.path[item.path_len] = 0;

        item.ext_idx = item.name_idx;
        for (uint32_t i = item.name_idx; i < item.path_len; i++)
            if (item.path[i] == '.')
                item.ext_idx = i;
        if (item.ext_idx == item.name_idx) // Failed to find extension
            item.ext_idx = item.path_len;
        item.is_dir = DT_DIR & entry->d_type;

        XFILES_ASSERT(strlen(item.path) == item.path_len);
        callback(data, &item);

    iterate:
        // readdir returns NULL if no more files, breaking the loop
        entry = readdir(dir);
    }

    if (dir)
        closedir(dir);
}

struct XFWatchHandles
{
    int  fd;
    bool is_dir;
    // path is stored in stringpool
    size_t stringpool_offset;
};

struct XFWatchContext
{
    int kq;

    struct kevent*         changelist;
    size_t                 changelist_len;
    size_t                 changelist_cap;
    struct XFWatchHandles* handles;
    size_t                 handles_len;
    size_t                 handles_cap;
    char*                  stringpool;
    size_t                 stringpool_len;
    size_t                 stringpool_cap;

    void*                   udata;
    xfiles_watch_callback_t callback;
};

static size_t _xfiles_watch_push_string(struct XFWatchContext* ctx, const char* str, size_t len)
{
    size_t offset = ctx->stringpool_len;
    XFILES_ASSERT((offset & 15) == 0);

    size_t nextlen = offset + len + 1;       // +1 for '\0' byte
    nextlen        = (nextlen + 0xf) & ~0xf; // Round to 16 byte boundary
    XFILES_ASSERT((nextlen & 15) == 0);

    if (nextlen > ctx->stringpool_cap)
    {
        size_t nextcap = nextlen * 2;
        if (nextcap < 4096) // min cap
            nextcap = 4096;
        ctx->stringpool     = XFILES_REALLOC(ctx->stringpool, nextcap);
        ctx->stringpool_cap = nextcap;
    }
    ctx->stringpool_len = nextlen;

    memcpy(ctx->stringpool + offset, str, len);
    ctx->stringpool[offset + len] = '\0';

    return offset;
}

void _xfiles_watch_add_listener(struct XFWatchContext* ctx, const char* path_, bool is_dir)
{
    struct kevent         event;
    struct XFWatchHandles wh;

    wh.fd = open(path_, O_EVTONLY);
    XFILES_ASSERT(wh.fd > 0);
    if (wh.fd <= 0)
        return;

    wh.is_dir            = is_dir;
    wh.stringpool_offset = _xfiles_watch_push_string(ctx, path_, strlen(path_));
    char* path           = ctx->stringpool + wh.stringpool_offset;
    XFILES_ASSERT(path >= ctx->stringpool);

    unsigned short flags = EV_ADD | EV_CLEAR;
    unsigned int fflags  = NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK | NOTE_RENAME | NOTE_REVOKE;
    event.ident          = wh.fd;
    event.filter         = EVFILT_VNODE;
    event.flags          = flags;
    event.fflags         = fflags;
    event.data           = 0;
    // Supposedly you're able to set a ptr here to whatever you want. It appears to be occasionally overwritten by
    // something in kevent(), so we can't trust it... Unless I'm doing a buffer overrun elsewhere, this is a bug
    event.udata = path;

    XFILES_ASSERT(ctx->changelist_len == ctx->handles_len);
    XFILES_ASSERT(ctx->changelist_cap == ctx->handles_cap);
    if (ctx->changelist_len + 1 > ctx->changelist_cap)
    {
        size_t nextcap = 2 * (ctx->changelist_len + 1);
        if (nextcap < 64) // min cap
            nextcap = 64;
        ctx->changelist     = XFILES_REALLOC(ctx->changelist, sizeof(*ctx->changelist) * nextcap);
        ctx->handles        = XFILES_REALLOC(ctx->handles, sizeof(*ctx->handles) * nextcap);
        ctx->changelist_cap = nextcap;
        ctx->handles_cap    = nextcap;
    }
    ctx->changelist[ctx->changelist_len++] = event;
    ctx->handles[ctx->handles_len++]       = wh;

    size_t nevents  = ctx->changelist_len;
    size_t nfolders = ctx->handles_len;
    XFILES_ASSERT(nevents == nfolders);
}

int _xfiles_watch_find_listener_by_string(struct XFWatchContext* ctx, const char* path)
{
    int    i;
    size_t nevents  = ctx->changelist_len;
    size_t nfolders = ctx->handles_len;
    XFILES_ASSERT(nevents == nfolders);
    for (i = 0; i < nfolders; i++)
    {
        struct XFWatchHandles* wh          = ctx->handles + i;
        const char*            cached_path = ctx->stringpool + wh->stringpool_offset;
        int                    match       = strcmp(path, cached_path);
        if (match == 0)
            return i;
    }
    return -1;
}

int _xfiles_watch_find_listener_by_fd(struct XFWatchContext* ctx, int fd)
{
    int    i;
    size_t nevents  = ctx->changelist_len;
    size_t nfolders = ctx->handles_len;
    XFILES_ASSERT(nevents == nfolders);
    for (i = 0; i < nfolders; i++)
    {
        struct kevent* e = ctx->changelist + i;
        if (e->ident == fd)
            return i;
    }
    return -1;
}

void _xfiles_watch_remove_listener_at_index(struct XFWatchContext* ctx, int i)
{
    XFILES_ASSERT(ctx->changelist_len > 0 && ctx->handles_len > 0);
    if (i != -1)
    {
        struct XFWatchHandles wh   = ctx->handles[i]; // copy data onto stack before deleting
        const char*           path = ctx->stringpool + wh.stringpool_offset;
        close(wh.fd);

        XFILES_ASSERT(ctx->changelist_len == ctx->handles_len);
        size_t remaining = ctx->changelist_len - i - 1;
        if (remaining)
        {
            memmove(ctx->changelist + i, ctx->changelist + i + 1, sizeof(*ctx->changelist) * remaining);
            memmove(ctx->handles + i, ctx->handles + i + 1, sizeof(*ctx->handles) * remaining);
        }
        ctx->changelist_len--;
        ctx->handles_len--;
    }
}

void _xfiles_watch_cb_on_direntry(void* data, const xfiles_list_item_t* item)
{
    const char* name    = item->path + item->name_idx;
    int         match_1 = strcmp(name, ".");
    int         match_2 = strcmp(name, "..");
    if (match_1 == 0 || match_2 == 0)
        return;

    struct XFWatchContext* ctx = (struct XFWatchContext*)data;
    _xfiles_watch_add_listener(ctx, item->path, item->is_dir);

    if (item->is_dir)
    {
        xfiles_list(item->path, ctx, _xfiles_watch_cb_on_direntry);
    }
}

void _xfiles_watch_cb_poll_for_new_entries(void* data, const xfiles_list_item_t* item)
{
    const char* name    = item->path + item->name_idx;
    int         match_1 = strcmp(name, ".");
    int         match_2 = strcmp(name, "..");
    if (match_1 == 0 || match_2 == 0)
        return;

    struct XFWatchContext* ctx = data;
    int                    idx = _xfiles_watch_find_listener_by_string(ctx, item->path);
    if (idx == -1)
    {
        _xfiles_watch_add_listener(ctx, item->path, item->is_dir);
        ctx->callback(XFILES_WATCH_CREATED, item->path, ctx->udata);

        if (item->is_dir)
        {
            xfiles_list(item->path, ctx, _xfiles_watch_cb_poll_for_new_entries);
        }
    }
}

xfiles_watch_context_t xfiles_watch_create(const char* path, void* udata, xfiles_watch_callback_t cb)
{
    XFILES_ASSERT(path != NULL); // What path should be watched?
    XFILES_ASSERT(cb != NULL);   // Did you forget to write a callback?

    struct XFWatchContext* ctx = XFILES_MALLOC(sizeof(*ctx));

    ctx->udata    = udata;
    ctx->callback = cb;

    _xfiles_watch_add_listener(ctx, path, true);
    xfiles_list(path, ctx, _xfiles_watch_cb_on_direntry);

    // https://man.freebsd.org/cgi/man.cgi?kqueue
    // https://www.ipnom.com/FreeBSD-Man-Pages/kqueue.2.html
    ctx->kq = kqueue();

    return ctx;
}

void xfiles_watch_flush(xfiles_watch_context_t _ctx)
{
    XFILES_ASSERT(_ctx != NULL);

    struct XFWatchContext* ctx     = _ctx;
    struct timespec        timeout = {0};
    // timeout.tv_sec  = 0;
    // timeout.tv_nsec = timeout_ms * 1000000;

    int loop = 1;
    do
    {
        enum
        {
            XFILES_WATCH_MAX_EVENTS = 16,
        };

        struct kevent events[XFILES_WATCH_MAX_EVENTS] = {0};
        // https://www.ipnom.com/FreeBSD-Man-Pages/kevent.2.html
        int nevents = kevent(ctx->kq, ctx->changelist, ctx->changelist_len, events, XFILES_ARRLEN(events), &timeout);

        for (int i = 0; i < nevents; i++)
        {
            if (events[0].flags == EV_ERROR)
                continue;

            // Helpful table pulled from here:
            // https://github.com/segmentio/fs/blob/main/notify_darwin.go
            // | Condition                               | Events                   |
            // | --------------------------------------- | ------------------------ |
            // | creating a file in a directory          | NOTE_WRITE               |
            // | creating a directory in a directory     | NOTE_WRITE NOTE_LINK     |
            // | creating a link in a directory          | NOTE_WRITE               |
            // | creating a symlink in a directory       | NOTE_WRITE               |
            // | removing a file from a directory        | NOTE_WRITE               |
            // | removing a directory from a directory   | NOTE_WRITE NOTE_LINK     |
            // | renaming a file within a directory      | NOTE_WRITE               |
            // | renaming a directory within a directory | NOTE_WRITE               |
            // | moving a file out of a directory        | NOTE_WRITE               |
            // | moving a directory out of a directory   | NOTE_WRITE NOTE_LINK     |
            // | writing to a file                       | NOTE_WRITE NOTE_EXTEND   |
            // | truncating a file                       | NOTE_ATTRIB              |
            // | overwriting a symlink                   | NOTE_DELETE, NOTE_RENAME |

            // NOTE: The control over the event loop with kevent is nice, but the info returned absolutely sucks.
            // There is no simple "file created" or "file deleted" event to respond to
            // If you create a folder in Finder, you get a NOTE_WRITE|NOTE_LINK event
            // If you move a folder to the Trash, you get a NOTE_WRITE|NOTE_LINK event...?
            // If you `rm -R` a folder, you get a NOTE_WRITE|NOTE_LINK event, not a NOTE_DELETE event???
            // kevents are effectively triggers for doing your own polling
            struct kevent* e = events + i;
            XFILES_ASSERT(e->filter == EVFILT_VNODE);

            int cached_path_idx = _xfiles_watch_find_listener_by_fd(ctx, e->ident);
            XFILES_ASSERT(cached_path_idx != -1);

            const struct XFWatchHandles* wh      = ctx->handles + cached_path_idx;
            const char*                  ev_path = ctx->stringpool + wh->stringpool_offset;

            bool previously_existed = cached_path_idx != -1;
            bool currently_exists   = xfiles_exists(ev_path);

            // struct XFWatchHandles(*view_folders)[512] = (void*)ctx.handles;

            if (previously_existed && !currently_exists)
            {
                XFILES_ASSERT(_xfiles_watch_find_listener_by_fd(ctx, e->ident) != -1);

                // Remove watch listeners for every item in directory & subdirs
                if (wh->is_dir)
                {
                    // We don't get "rename" events for deleted/renamed directories contents, so we have to remove
                    // them from our data structure ourselves
                    int N = ctx->handles_len;
                    for (int j = N; j-- > 0;)
                    {
                        // if a (directory) is substring of b (subdir or file), remove b
                        const char* candidate_path = ctx->stringpool + ctx->handles[j].stringpool_offset;
                        const char* a              = ev_path;
                        const char* b              = candidate_path;
                        while (*a == *b && *a != 0 && *b != 0)
                        {
                            a++;
                            b++;
                        }

                        bool is_substring = *a == 0 && *b != 0;
                        if (is_substring) // must be child directory or file
                        {
                            XFILES_ASSERT(!xfiles_exists(candidate_path));
                            _xfiles_watch_remove_listener_at_index(ctx, j);
                            ctx->callback(XFILES_WATCH_DELETED, candidate_path, ctx->udata);
                        }
                    }
                } // !is_dir
                _xfiles_watch_remove_listener_at_index(ctx, cached_path_idx);
                ctx->callback(XFILES_WATCH_DELETED, ev_path, ctx->udata);
            }

            if (previously_existed && currently_exists)
            {
                if (e->fflags & NOTE_WRITE)
                {
                    ctx->callback(XFILES_WATCH_MODIFIED, ev_path, ctx->udata);

                    // Dir was modified. A new file may have been added
                    if (wh->is_dir)
                        xfiles_list(ev_path, ctx, _xfiles_watch_cb_poll_for_new_entries);
                }
                // if (e->fflags & NOTE_ATTRIB) // do we need an attrib event?
                //     ctx.callback(XFILES_WATCH_MODIFIED, ev_path, ctx.udata);
            }
        }

        loop = nevents == XFILES_WATCH_MAX_EVENTS;
    }
    while (loop);
}

void xfiles_watch_destroy(void* _ctx_nb)
{
    XFILES_ASSERT(_ctx_nb != NULL);
    struct XFWatchContext* ctx = _ctx_nb;

    if (ctx->kq != -1)
        close(ctx->kq);

    size_t nevents  = ctx->changelist_len;
    size_t nfolders = ctx->handles_len;
    XFILES_ASSERT(nevents == nfolders);
    for (int i = nevents; i-- > 0;)
        _xfiles_watch_remove_listener_at_index(ctx, i);
    XFILES_FREE(ctx->changelist);
    XFILES_FREE(ctx->handles);
    XFILES_FREE(ctx->stringpool);

    XFILES_FREE(ctx);
}

#ifdef __OBJC__
#import <AppKit/NSWorkspace.h>
#import <Foundation/Foundation.h>

bool xfiles_trash(const char* path)
{
    NSString* str     = [[NSString alloc] initWithUTF8String:path];
    NSURL*    itemUrl = [NSURL fileURLWithPath:str isDirectory:FALSE];

    const NSFileManager* fm           = [NSFileManager defaultManager]; // strong
    NSURL*               resultingURL = NULL;
    NSError*             error        = NULL;

    BOOL ok = NO;

    // Apparently a long standing bug in macOS is that this function will not give you the give 'Put Back' feature
    // inside Trash that you would normally see if you deleted the file using the Finder app. Such a shame!
    // https://openradar.appspot.com/radar?id=5063396789583872
    ok = [fm trashItemAtURL:itemUrl resultingItemURL:&resultingURL error:&error];

#if !__has_feature(objc_arc)
    [fm release];
    [itemUrl release];
    [str release];
#endif
    return ok;
}

bool xfiles_open_file_explorer(const char* path)
{
    BOOL ok = [[NSWorkspace sharedWorkspace] openFile:@(path) withApplication:@"Finder"];
    XFILES_ASSERT(ok);
    return ok;
}

bool xfiles_select_in_file_explorer(const char* path)
{
    // https://developer.apple.com/documentation/appkit/nsworkspace/1524399-selectfile
    [[NSWorkspace sharedWorkspace] selectFile:@(path) inFileViewerRootedAtPath:@("")];
    return true;
}

int xfiles_get_user_directory(char* out, size_t outlen, XFilesUserDirectory loc)
{
    static const char* PATHS[] = {
        "",                             // XFILES_USER_DIRECTORY_HOME,
        "/Library/Application Support", // XFILES_USER_DIRECTORY_APPDATA
        "/Desktop",                     // XFILES_USER_DIRECTORY_DESKTOP
        "/Documents",                   // XFILES_USER_DIRECTORY_DOCUMENTS
        "/Downloads",                   // XFILES_USER_DIRECTORY_DOWNLOADS
        "/Music",                       // XFILES_USER_DIRECTORY_MUSIC
        "/Pictures",                    // XFILES_USER_DIRECTORY_PICTURES
        "/Movies",                      // XFILES_USER_DIRECTORY_VIDEOS
    };
    _Static_assert(XFILES_ARRLEN(PATHS) == XFILES_USER_DIRECTORY_COUNT);

    // I don't think calling this function touches the reference count, but then I haven't looked at the binary
    // https://developer.apple.com/documentation/foundation/1413045-nshomedirectory
    NSString*   nsstr   = NSHomeDirectory();
    const char* homebuf = [nsstr UTF8String];
    size_t      homelen = strlen(homebuf);
    const char* subdir;
    size_t      subdirlen;

    if (loc < 0)
        loc = 0;
    if (loc >= XFILES_USER_DIRECTORY_COUNT)
        loc = XFILES_USER_DIRECTORY_COUNT - 1;

    subdir    = PATHS[loc];
    subdirlen = strlen(subdir);

    XFILES_ASSERT((homelen + subdirlen + 1) <= outlen);
    if ((homelen + subdirlen + 1) > outlen)
        return 0;

    memcpy(out, homebuf, homelen);
    memcpy(out + homelen, subdir, subdirlen);
    out[homelen + subdirlen] = '\0';

    return homelen + subdirlen;
}

#endif // __OBJC__
#endif // __APPLE__

void xfiles_read_free(void *data)
{
    XFILES_FREE(data);
}

const char* xfiles_get_name(const char* path)
{
    const char* name = path;
    for (const char* c = path; *c != '\0'; c++)
        if (*c == XFILES_DIR_CHAR)
            name = c + 1;
    return name == path ? NULL : name;
}

const char* xfiles_get_extension(const char* name)
{
    const char* extension = name;
    for (const char* c = name; *c != '\0'; c++)
        if (*c == '.')
            extension = c;
    return extension == name ? NULL : extension;
}

bool xfiles_create_directory_recursive(const char* path)
{
    if (xfiles_exists(path))
        return true;

    char nextpath[1024];
    int  i;
    nextpath[0] = path[0];
#ifdef _WIN32
    // eg: "C:\\Users\\username", start at "U"
    nextpath[1] = path[1];
    nextpath[2] = path[2];
    i           = 3;
#else
    // eg "/Users/username", start at "U"
    i = 1;
#endif
    for (; path[i] != '\0'; i++)
    {
        nextpath[i] = path[i];
        if (nextpath[i] == XFILES_DIR_CHAR)
        {
            nextpath[i] = '\0';
            if (!xfiles_exists(nextpath))
                xfiles_create_directory(nextpath);
            XFILES_ASSERT(xfiles_exists(nextpath));
            nextpath[i] = XFILES_DIR_CHAR;
        }
    }
    bool ok = xfiles_create_directory(path);
    XFILES_ASSERT(ok);
    return ok;
}

#endif // XHL_FILES_IMPL
#endif // XHL_FILES_H
