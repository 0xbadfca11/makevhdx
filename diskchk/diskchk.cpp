#define WIN32_LEAN_AND_MEAN
#define STRICT_GS_ENABLED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <winioctl.h>
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <crtdbg.h>
#include "../miscutil.hpp"
#pragma comment(lib, "bcrypt")
#pragma comment(lib, "crypt32")

int __cdecl main(int argc, char* argv[])
{
	setlocale(LC_ALL, "");
	if (argc < 2)
	{
		puts(R"(\\.\PhysicalDrive%u)");
		ExitProcess(EXIT_FAILURE);
	}
	HANDLE h = CreateFileA(argv[1], GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, nullptr);
	if (h == INVALID_HANDLE_VALUE)
	{
		die();
	}
	GET_LENGTH_INFORMATION length;
	ULONG junk;
	DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof length, &junk, nullptr);
	const ULONG buffer_size = 16 * 1024 * 1024;
	PUCHAR buffer[2] = {
		reinterpret_cast<PUCHAR>(VirtualAlloc(nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)),
		reinterpret_cast<PUCHAR>(VirtualAlloc(nullptr, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)),
	};
	BCRYPT_HASH_HANDLE hash_handle;
	ATLENSURE(BCRYPT_SUCCESS(BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &hash_handle, nullptr, 0, nullptr, 0, 0)));
	OVERLAPPED o = {};
	ATLENSURE(ReadFile(h, buffer[0], static_cast<ULONG>(std::min<LONGLONG>(length.Length.QuadPart, buffer_size)), nullptr, &o) || GetLastError() == ERROR_IO_PENDING);
	length.Length.QuadPart -= std::min<LONGLONG>(length.Length.QuadPart, buffer_size);
	UINT64 total = 0;
	for (UINT64 i = 1, current = 0, previous = 0;; i++, previous = current)
	{
		ULONG read;
		ATLENSURE(GetOverlappedResult(h, &o, &read, TRUE));
		total += read;
		if (length.Length.QuadPart == 0)
		{
			ATLENSURE(BCRYPT_SUCCESS(BCryptHashData(hash_handle, buffer[current], read, 0)));
			break;
		}
		current = (current + 1) % _countof(buffer);
		o.Offset = static_cast<ULONG>(buffer_size * i);
		o.OffsetHigh = static_cast<ULONG>(buffer_size * i >> 32);
		ATLENSURE(ReadFile(h, buffer[current], static_cast<ULONG>(std::min<LONGLONG>(length.Length.QuadPart, buffer_size)), nullptr, &o) || GetLastError() == ERROR_IO_PENDING);
		length.Length.QuadPart -= std::min<LONGLONG>(length.Length.QuadPart, buffer_size);
		ATLENSURE(BCRYPT_SUCCESS(BCryptHashData(hash_handle, buffer[previous], read, 0)));
	}
	const size_t sha256_length = 32;
	UCHAR hash[sha256_length];
	ATLENSURE(BCRYPT_SUCCESS(BCryptFinishHash(hash_handle, hash, sizeof hash, 0)));
	ULONG stringize_size = sha256_length * 2 + 1;
	char hash_string[sha256_length * 2 + 1];
	ATLENSURE(CryptBinaryToStringA(hash, sizeof hash, CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, hash_string, &stringize_size));
	printf("%llu:", total);
	puts(hash_string);
	_RPTN(_CRT_WARN, "%llu:%s\n", total, hash_string);
	ExitProcess(EXIT_SUCCESS);
}