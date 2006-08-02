// Copyright (C) 2002-2006 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CImageLoaderBMP.h"
#include <string.h>
#include "SColor.h"
#include "CColorConverter.h"
#include "CImage.h"
#include "os.h"

namespace irr
{
namespace video
{


//! constructor
CImageLoaderBMP::CImageLoaderBMP()
: BmpData(0), PaletteData(0)
{
	#ifdef _DEBUG
	setDebugName("CImageLoaderBMP");
	#endif
}



//! destructor
CImageLoaderBMP::~CImageLoaderBMP()
{
	if (PaletteData)
		delete [] PaletteData;

	if (BmpData)
		delete [] BmpData;
}



//! returns true if the file maybe is able to be loaded by this class
//! based on the file extension (e.g. ".tga")
bool CImageLoaderBMP::isALoadableFileExtension(const c8* fileName)
{
	return strstr(fileName, ".bmp") != 0;
}



//! returns true if the file maybe is able to be loaded by this class
bool CImageLoaderBMP::isALoadableFileFormat(irr::io::IReadFile* file)
{
	u16 headerID;
	file->read(&headerID, sizeof(u16));
#ifdef __BIG_ENDIAN__
	headerID = OSReadSwapInt16(&headerID,0);
#endif
	return headerID == 0x4d42;
}


void CImageLoaderBMP::decompress8BitRLE(c8*& BmpData, s32 size, s32 width, s32 height, s32 pitch)
{
	c8* p = BmpData;
	c8* newBmp = new c8[(width+pitch)*height];
	c8* d = newBmp;
	c8* destEnd = newBmp + (width+pitch)*height;
	s32 line = 0;

	while (BmpData - p < size && d < destEnd)
	{
		if (*p == 0)
		{
			++p;

			switch(*p)
			{
			case 0: // end of line
				++p; 
				++line;
				d = newBmp + (line*(width+pitch));
				break; 
			case 1: // end of bmp
				delete [] BmpData;
				BmpData = newBmp;
				return; 
			case 2:
				++p; d +=(u8)*p;  // delta
				++p; d += ((u8)*p)*(width+pitch); 
				++p;
				break;
			default:
				{
					// absolute mode
					s32 count = (u8)*p; ++p;
					s32 readAdditional = ((2-(count%2))%2);
					s32 i;

					for (i=0; i<count; ++i)
					{
						*d = *p;
						++p;
						++d;
					}

					for (i=0; i<readAdditional; ++i)
						++p;
				}
			}
		}
		else
		{
			s32 count = (u8)*p; ++p;			
			s32 color = (u8)*p; ++p;
			for (s32 i=0; i<count; ++i)
			{
				*d = color;
				++d;
			}
		}
	}

	delete [] BmpData;
	BmpData = newBmp;
}


void CImageLoaderBMP::decompress4BitRLE(c8*& BmpData, s32 size, s32 width, s32 height, s32 pitch)
{
	s32 lineWidth = (width+1)/2+pitch;
	c8* p = BmpData;
	c8* newBmp = new c8[lineWidth*height];
	c8* d = newBmp;
	c8* destEnd = newBmp + lineWidth*height;
	s32 line = 0;
	s32 shift = 4;

	while (BmpData - p < size && d < destEnd)
	{
		if (*p == 0)
		{
			++p;

			switch(*p)
			{
			case 0: // end of line
				++p; 
				++line;
				d = newBmp + (line*lineWidth);
				shift = 4;
				break; 
			case 1: // end of bmp
				delete [] BmpData;
				BmpData = newBmp;
				return; 
			case 2:
				{
					++p; 
					s32 x = (u8)*p; ++p;
					s32 y = (u8)*p; ++p;
					d += x/2 + y*lineWidth;
					shift = x%2==0 ? 4 : 0;
				}
				break;
			default:
				{
					// absolute mode
					s32 count = (u8)*p; ++p;
					s32 readAdditional = ((2-((count)%2))%2);
					s32 readShift = 4;
					s32 i;

					for (i=0; i<count; ++i)
					{
						s32 color = (((u8)*p) >> readShift) & 0x0f;
						readShift -= 4;
						if (readShift < 0)
						{
							++*p;
							readShift = 4;
						}

						c8 mask = 0x0f << shift;
						*d = (*d & (~mask)) | ((color << shift) & mask);

						shift -= 4;
						if (shift < 0)
						{
							shift = 4;
							++d;
						}

					}

					for (i=0; i<readAdditional; ++i)
						++p;
				}
			}
		}
		else
		{
			s32 count = (u8)*p; ++p;			
			s32 color1 = (u8)*p; color1 = color1 & 0x0f;
			s32 color2 = (u8)*p; color2 = (color2 >> 4) & 0x0f;
			++p;

			for (s32 i=0; i<count; ++i)
			{
				c8 mask = 0x0f << shift;
				c8 toSet = (shift==0 ? color1 : color2) << shift;
				*d = (*d & (~mask)) | (toSet & mask);

				shift -= 4;
				if (shift < 0)
				{
					shift = 4;
					++d;
				}
			}
		}
	}

	delete [] BmpData;
	BmpData = newBmp;
}



//! creates a surface from the file
IImage* CImageLoaderBMP::loadImage(irr::io::IReadFile* file)
{
	SBMPHeader header;

	file->read(&header, sizeof(header));

#ifdef __BIG_ENDIAN__
	header.Id = OSReadSwapInt16(&header.Id,0);
	header.FileSize = OSReadSwapInt32(&header.FileSize,0);
	header.BitmapDataOffset = OSReadSwapInt32(&header.BitmapDataOffset,0);
	header.BitmapHeaderSize = OSReadSwapInt32(&header.BitmapHeaderSize,0);
	header.Width = OSReadSwapInt32(&header.Width,0);
	header.Height = OSReadSwapInt32(&header.Height,0);
	header.Planes = OSReadSwapInt16(&header.Planes,0);
	header.BPP = OSReadSwapInt16(&header.BPP,0);
	header.Compression = OSReadSwapInt32(&header.Compression,0);
	header.BitmapDataSize = OSReadSwapInt32(&header.BitmapDataSize,0);
	header.PixelPerMeterX = OSReadSwapInt32(&header.PixelPerMeterX,0);
	header.PixelPerMeterY = OSReadSwapInt32(&header.PixelPerMeterY,0);
	header.Colors = OSReadSwapInt32(&header.Colors,0);
	header.ImportantColors = OSReadSwapInt32(&header.ImportantColors,0);
#endif

	s32 pitch = 0;

	//! return if the header is false

	if (header.Id != 0x4d42 && header.Id != 0x424d)
		return 0;

	if (header.Compression > 2) // we'll only handle RLE-Compression
	{
		os::Printer::log("Compression mode not supported.", ELL_ERROR);
		return 0;
	}

	// adjust bitmap data size to dword boundary
	header.BitmapDataSize += (4-(header.BitmapDataSize%4))%4;

	// read palette

	s32 pos = file->getPos();
	s32 paletteSize = (header.BitmapDataOffset - pos) / 4;

	if (paletteSize)
	{
		PaletteData = new s32[paletteSize];
		file->read(PaletteData, paletteSize * sizeof(s32));
#ifdef __BIG_ENDIAN__
		for (int i=0; i<paletteSize; ++i)
			PaletteData[i] = OSReadSwapInt32(&PaletteData[i],0);
#endif
	}

	// read image data

	if (!header.BitmapDataSize)
	{
		// okay, lets guess the size
		// some tools simply don't set it
		header.BitmapDataSize = file->getSize() - header.BitmapDataOffset;
	}

	file->seek(header.BitmapDataOffset);

	f32 t = (header.Width) * (header.BPP / 8.0f);
	s32 widthInBytes = (s32)t;
	t -= widthInBytes;
	if (t!=0.0f)
		++widthInBytes;

	s32 lineData = widthInBytes + ((4-(widthInBytes%4)))%4;
	pitch = lineData - widthInBytes;
	
	BmpData = new c8[header.BitmapDataSize];
	file->read(BmpData, header.BitmapDataSize);

	// decompress data if needed
	switch(header.Compression)
	{
	case 1: // 8 bit rle
		decompress8BitRLE(BmpData, header.BitmapDataSize, header.Width, header.Height, pitch);
		break;
	case 2: // 4 bit rle
		decompress4BitRLE(BmpData, header.BitmapDataSize, header.Width, header.Height, pitch);
		break;
	}

	// create surface

	IImage* image = 0;

	switch(header.BPP)
	{
	case 1:
		image = new CImage(ECF_A1R5G5B5, core::dimension2d<s32>(header.Width, header.Height));
		CColorConverter::convert1BitTo16BitFlipMirror(BmpData, (s16*)image->lock(), header.Width, header.Height, pitch);
		image->unlock();
		break;
	case 4:
		image = new CImage(ECF_A1R5G5B5, core::dimension2d<s32>(header.Width, header.Height));
		CColorConverter::convert4BitTo16BitFlipMirror(BmpData, (s16*)image->lock(), header.Width, header.Height, pitch, PaletteData);
		image->unlock();
		break;
	case 8:
		image = new CImage(ECF_A1R5G5B5, core::dimension2d<s32>(header.Width, header.Height));
		CColorConverter::convert8BitTo16BitFlipMirror(BmpData, (s16*)image->lock(), header.Width, header.Height, pitch, PaletteData);
		image->unlock();
		break;
	case 16:
		image = new CImage(ECF_A1R5G5B5, core::dimension2d<s32>(header.Width, header.Height));
		CColorConverter::convert16BitTo16BitFlipMirror((s16*)BmpData, (s16*)image->lock(), header.Width, header.Height, pitch);
		image->unlock();
		break;
	case 24:
		image = new CImage(ECF_R8G8B8, core::dimension2d<s32>(header.Width, header.Height));
		CColorConverter::convert24BitTo24BitFlipMirrorColorShuffle(BmpData, (c8*)image->lock(), header.Width, header.Height, pitch);
		image->unlock();
		break;
	case 32: // thx to Reinhard Ostermeier
		image = new CImage(ECF_A8R8G8B8, core::dimension2d<s32>(header.Width, header.Height));
		CColorConverter::convert32BitTo32BitFlipMirror((s32*)BmpData, (s32*)image->lock(), header.Width, header.Height, pitch);
		image->unlock();
		break;
	};

	// clean up

	delete [] PaletteData;
	PaletteData = 0;

	delete [] BmpData;
	BmpData = 0;

	return image;
}


//! creates a loader which is able to load windows bitmaps
IImageLoader* createImageLoaderBMP()
{
	return new CImageLoaderBMP;
}



} // end namespace video
} // end namespace irr
