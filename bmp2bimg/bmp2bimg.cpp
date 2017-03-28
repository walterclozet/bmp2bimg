// bmp2bimg.cpp : 定义控制台应用程序的入口点。
//

#include "windows.h"
#include <gdiplus.h>
#include <shlwapi.h>
#include <tchar.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

int TileOrder[] =
{
	0,  1,   4,  5,
	2,  3,   6,  7,

	8,  9,  12, 13,
	10, 11,  14, 15
};

void WriteU16LE(BYTE Data[], int Offset, USHORT Value)
{
	Data[Offset] = (BYTE)(Value & 0xFF);
	Data[Offset + 1] = (BYTE)((Value >> 8) & 0xFF);
}

UINT ToRGB565(UINT A, UINT R, UINT G, UINT B)
{
	UINT result = 0;
	UINT mask;

	mask = ~(0xFFFFFFFFu << 5);
	result |= ((R * mask + 127u) / 255u) << 11;
	mask = ~(0xFFFFFFFFu << 6);
	result |= ((G * mask + 127u) / 255u) << 5;
	mask = ~(0xFFFFFFFFu << 5);
	result |= ((B * mask + 127u) / 255u) << 0;
	return result;
}

UINT ConvertColorFormatARGB2RGB565(UINT InColor)
{
	//From color format to components:
	UINT A, R, G, B;
	UINT mask;
	{
		mask = ~(0xFFFFFFFFu << 8);
		A = ((((InColor >> 24) & mask) * 255u) + mask / 2) / mask;
	}
	mask = ~(0xFFFFFFFFu << 8);
	R = ((((InColor >> 16) & mask) * 255u) + mask / 2) / mask;
	mask = ~(0xFFFFFFFFu << 8);
	G = ((((InColor >> 8) & mask) * 255u) + mask / 2) / mask;
	mask = ~(0xFFFFFFFFu << 8);
	B = ((((InColor >> 0) & mask) * 255u) + mask / 2) / mask;
	return ToRGB565(A, R, G, B);
}

BYTE * ConvertFromBitmapARGB(Bitmap * pDest)
{
	BitmapData oData;
	Rect rcRect(0, 0, 256, 128);

	Status status = pDest->LockBits(&rcRect, ImageLockModeRead, PixelFormat32bppARGB, &oData);
	UINT * res = (UINT*)oData.Scan0;
	BYTE * result = new BYTE[256 * 128 * 2]; // 2 byte per rgb data
	int offs = 0;
	for (int y = 0; y < 128; y += 8)
	{
		for (int x = 0; x < 256; x += 8)
		{
			for (int i = 0; i < 64; i++)
			{
				int x2 = i % 8;
				if (x + x2 >= 256) continue;
				int y2 = i / 8;
				if (y + y2 >= 128) continue;
				int pos = TileOrder[x2 % 4 + y2 % 4 * 4] + 16 * (x2 / 4) + 32 * (y2 / 4);
				WriteU16LE(result, offs + pos * 2, (USHORT)ConvertColorFormatARGB2RGB565(res[(y + y2) * oData.Stride / 4 + x + x2]));
			}
			offs += 64 * 2;
		}
	}

	pDest->UnlockBits(&oData);

	return result;
}


int _tmain(int argc, TCHAR *argv[])
{
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	Bitmap * pSource = NULL;
	if (PathFileExists(argv[1]))
	{
		pSource = new Bitmap(argv[1]);
	}
	else
	{
		TCHAR bufFile[MAX_PATH];
		GetModuleFileName(NULL, bufFile, MAX_PATH);
		TCHAR *p = _tcsrchr(bufFile, '//');
		*p = 0x00;

		_tcscat_s(bufFile, argv[1]);
		pSource = new Bitmap(bufFile);
	}

	UINT nSrcWidth = pSource->GetWidth();
	UINT nSrcHeight = pSource->GetHeight();
	UINT nNewWidth = nSrcWidth;
	UINT nNewHeight = nSrcHeight;
	UINT nNewLeft = 0;
	UINT nNewTop = 0;

	double dRate = 200.0 / 120.0;
	if (double(nSrcWidth) / double(nSrcHeight) > dRate)
	{
		nNewWidth = nSrcHeight * dRate;
		nNewLeft = (nSrcWidth - nNewWidth) / 2;
	}
	else
	{
		nNewHeight = nSrcWidth / dRate;
		nNewTop = (nSrcHeight - nNewHeight) / 2;
	}
	Bitmap * pStreched = new Bitmap(nNewWidth, nNewHeight, PixelFormat32bppARGB);
	Graphics * g1 = Graphics::FromImage(pStreched);
	g1->SetInterpolationMode(InterpolationModeHighQualityBicubic);
	g1->DrawImage(pSource, 0, 0, nNewLeft, nNewTop, nSrcWidth, nSrcHeight, UnitPixel);
	
	Bitmap * pDest = new Bitmap(256, 128, PixelFormat32bppARGB);
	Graphics * g = Graphics::FromImage(pDest);
	Color cl;
	cl.SetFromCOLORREF(0);
	SolidBrush * b = new SolidBrush(cl);
	
	g->FillRectangle(b, Rect(0, 0, 256, 128));
	g->SetInterpolationMode(InterpolationModeHighQualityBicubic);
	g->DrawImage(pStreched, 0, 0, 200, 120);

	//BYTE * pBImgData = ConvertFromBitmapARGB(pDest);
	BYTE * pBImgData = ConvertFromBitmapARGB(pDest);

	BYTE bimgHeader[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
						  0x00, 0x01, 0x80, 0x00, 0x01, 0x02, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0xe0, 0x2e, 0x06, 0x51 };

	TCHAR szDestFile[MAX_PATH];
	_tcscpy_s(szDestFile, argv[1]);
	size_t i = _tcslen(szDestFile);
	for (; i >= 0; --i)
	{
		if (szDestFile[i] == _T('.'))
		{
			szDestFile[i] = 0;
			_tcscat_s(szDestFile, _T(".bimg"));
			break;
		}
	}
	if (PathFileExists(szDestFile))
	{
		DeleteFile(szDestFile);
	}
	HANDLE hFile = CreateFile(szDestFile, FILE_ALL_ACCESS, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		DWORD dwErr = GetLastError();
		TCHAR buf[255];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr, 0, buf, 255, NULL);
		_tprintf_s(buf);
	}
	else
	{
		DWORD dwWritten;
		WriteFile(hFile, bimgHeader, 0x20, &dwWritten, NULL);
		WriteFile(hFile, pBImgData, 256 * 128 * 2, &dwWritten, NULL);
		FlushFileBuffers(hFile);
		CloseHandle(hFile);
	}

	delete[] pBImgData;
	
	delete g1;
	delete g;
	delete pStreched;
	delete pSource;
	delete pDest;

	GdiplusShutdown(gdiplusToken);

    return 0;
}

