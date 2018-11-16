#pragma once
#define WIN32_LEAN_AND_MEAN
#define STRICT_GS_ENABLED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <windows.h>

namespace
{
	constexpr UINT32 byteswap32(_In_ UINT32 v) noexcept
	{
		return v >> 24 | (v >> 8 & 0xFFU << 8) | (v << 8 & 0xFFU << 16) | v << 24;
	}
	static_assert(0xCC335577 == byteswap32(0x775533CC));
	template <typename Ty1, typename Ty2>
	constexpr auto ROUNDUP(_In_ Ty1 number, _In_ Ty2 num_digits) noexcept
	{
		return (number + num_digits - 1) / num_digits * num_digits;
	}
	static_assert(ROUNDUP(42, 15) == 45);
	template <typename Ty1, typename Ty2>
	constexpr auto CEILING(_In_ Ty1 number, _In_ Ty2 significance) noexcept
	{
		return (number + significance - 1) / significance;
	}
	static_assert(CEILING(42, 15) == 3);
	constexpr bool IsPow2(_In_ UINT64 value) noexcept
	{
		return value && (value & (value - 1)) == 0;
	}
	static_assert(IsPow2(64));
	static_assert(!IsPow2(65));
	static_assert(!IsPow2(0));
	[[noreturn]]
	void die(_In_opt_z_ PCWSTR err_msg = nullptr)
	{
#ifdef _DEBUG
		if (IsDebuggerPresent())
		{
			_CrtDbgBreak();
		}
#endif
		if (!err_msg)
		{
			FormatMessageW(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr,
				GetLastError(),
				0,
				reinterpret_cast<PWSTR>(&err_msg),
				0,
				nullptr
			);
		}
		fputws(err_msg, stderr);
		fputws(L"\n", stderr);
		ExitProcess(EXIT_FAILURE);
	}
	template <typename Ty>
	Ty* ReadFileWithOffset(
		_In_ HANDLE hFile,
		_Out_writes_bytes_(nNumberOfBytesToRead) __out_data_source(FILE) Ty* lpBuffer,
		_In_ ULONG nNumberOfBytesToRead,
		_In_ ULONGLONG Offset
	)
	{
		static_assert(!std::is_pointer_v<Ty>);
		ULONG read;
		OVERLAPPED o = {};
		o.Offset = static_cast<ULONG>(Offset);
		o.OffsetHigh = static_cast<ULONG>(Offset >> 32);
		if (!ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, &read, &o) || read != nNumberOfBytesToRead)
		{
			die();
		}
		return lpBuffer;
	}
	template <typename Ty>
	Ty* ReadFileWithOffset(
		_In_ HANDLE hFile,
		_Out_writes_bytes_(sizeof(Ty)) __out_data_source(FILE) Ty* lpBuffer,
		_In_ ULONGLONG Offset
	)
	{
		return ReadFileWithOffset(hFile, lpBuffer, sizeof(Ty), Offset);
	}
	VOID WriteFileWithOffset(
		_In_ HANDLE hFile,
		_In_reads_bytes_(nNumberOfBytesToWrite) LPCVOID lpBuffer,
		_In_ ULONG nNumberOfBytesToWrite,
		_In_ ULONGLONG Offset
	)
	{
		_ASSERT(Offset % 512 == 0);
		ULONG write;
		OVERLAPPED o = {};
		o.Offset = static_cast<ULONG>(Offset);
		o.OffsetHigh = static_cast<ULONG>(Offset >> 32);
		if (!WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &write, &o) || write != nNumberOfBytesToWrite)
		{
			die();
		}
	}
	template <typename Ty>
	VOID WriteFileWithOffset(
		_In_ HANDLE hFile,
		_In_ const Ty& lpBuffer,
		_In_ ULONGLONG Offset
	)
	{
		static_assert(!std::is_pointer_v<Ty>);
		WriteFileWithOffset(hFile, &lpBuffer, sizeof(Ty), Offset);
	}
}