#include <windows.h>
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <wil/filesystem.h>
#include <wil/resource.h>
#include <wil/result.h>
#pragma comment(lib, "bcrypt")
#pragma comment(lib, "crypt32")

int wmain(int argc, PWSTR argv[]) try
{
	if (argc < 2)
	{
		puts(R"(\\.\PhysicalDrive%u [...])");
		ExitProcess(EXIT_FAILURE);
	}
	setlocale(LC_CTYPE, "");
	for (int j = 1; j < argc; j++)
	{
		auto h = wil::open_file(argv[j], GENERIC_READ, 0, FILE_FLAG_OVERLAPPED);
		GET_LENGTH_INFORMATION length;
		ULONG _;
		THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(h.get(), IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof length, &_, nullptr));
		const ULONG buffer_size = 16 * 1024 * 1024;
		PUCHAR buffer[2] = {
			reinterpret_cast<PUCHAR>(VirtualAlloc(nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)),
			reinterpret_cast<PUCHAR>(VirtualAlloc(nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)),
		};
		THROW_IF_NULL_ALLOC(buffer[0]);
		THROW_IF_NULL_ALLOC(buffer[1]);
		BCRYPT_HASH_HANDLE hash_handle;
		THROW_IF_NTSTATUS_FAILED(BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &hash_handle, nullptr, 0, nullptr, 0, 0));
		OVERLAPPED o = {};
		THROW_LAST_ERROR_IF(!ReadFile(h.get(), buffer[0], static_cast<ULONG>(std::min<LONGLONG>(length.Length.QuadPart, buffer_size)), nullptr, &o) && GetLastError() != ERROR_IO_PENDING);
		length.Length.QuadPart -= std::min<LONGLONG>(length.Length.QuadPart, buffer_size);
		for (UINT64 i = 1, current = 0, previous = 0;; i++, previous = current)
		{
			ULONG read;
			THROW_IF_WIN32_BOOL_FALSE(GetOverlappedResult(h.get(), &o, &read, TRUE));
			if (length.Length.QuadPart == 0)
			{
				THROW_IF_NTSTATUS_FAILED(BCryptHashData(hash_handle, buffer[current], read, 0));
				break;
			}
			current = (current + 1) % _countof(buffer);
			o.Offset = static_cast<ULONG>(buffer_size * i);
			o.OffsetHigh = static_cast<ULONG>(buffer_size * i >> 32);
			THROW_LAST_ERROR_IF(!ReadFile(h.get(), buffer[current], static_cast<ULONG>(std::min<LONGLONG>(length.Length.QuadPart, buffer_size)), nullptr, &o) && GetLastError() != ERROR_IO_PENDING);
			length.Length.QuadPart -= std::min<LONGLONG>(length.Length.QuadPart, buffer_size);
			THROW_IF_NTSTATUS_FAILED(BCryptHashData(hash_handle, buffer[previous], read, 0));
		}
		const size_t sha256_length = 32;
		UCHAR hash[sha256_length];
		THROW_IF_NTSTATUS_FAILED(BCryptFinishHash(hash_handle, hash, sizeof hash, 0));
		ULONG stringize_size = sha256_length * 2 + 1;
		char hash_string[sha256_length * 2 + 1];
		THROW_IF_WIN32_BOOL_FALSE(CryptBinaryToStringA(hash, sizeof hash, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, hash_string, &stringize_size));
		fputs("SHA256=", stdout);
		puts(hash_string);

		o.Offset = 0;
		o.OffsetHigh = 0;
		THROW_LAST_ERROR_IF(!ReadFile(h.get(), buffer[0], 1024 * 1024, nullptr, &o) && GetLastError() != ERROR_IO_PENDING);
		ULONG wrote;
		THROW_IF_WIN32_BOOL_FALSE(GetOverlappedResult(h.get(), &o, &wrote, TRUE));
		stringize_size = sha256_length * 2 + 1;
		THROW_IF_WIN32_BOOL_FALSE(CryptBinaryToStringA(buffer[0], sizeof hash, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, hash_string, &stringize_size));
		fputs("HEAD  =", stdout);
		puts(hash_string);
	}
	ExitProcess(EXIT_SUCCESS);
}
catch (wil::ResultException e)
{
	puts(e.what());
	ExitProcess(EXIT_FAILURE);
}