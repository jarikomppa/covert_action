#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "stb/stb_image_write.h"

// Link with stb_image_write.c

// standard dos 16 color palette (except dos was 6 bit, this is 8 bit scaled from the 6 bit values)
unsigned int DOS_Palette32[16] =
{
	0xff000000, 0xff0000aa, 0xff00aa00, 0xff00aaaa, 0xffaa0000, 0xffaa00aa, 0xffaa5500, 0xffaaaaaa,
	0xff555555, 0xff5555ff, 0xff55ff55, 0xff55ffff, 0xffff5555, 0xffff55ff, 0xffffff55, 0xffffffff
};

// covert action palette is the same, except index 5 is set to black
unsigned int CA_Palette32[16] =
{
	0x00000000, 0xff0000aa, 0xff00aa00, 0xff00aaaa, 0xffaa0000, 0xff000000, 0xffaa5500, 0xffaaaaaa,
	0xff555555, 0xff5555ff, 0xff55ff55, 0xff55ffff, 0xffff5555, 0xffff55ff, 0xffffff55, 0xffffffff
};

// stb wants r/b swapped palette..
unsigned int CA_Palette32_rbswapped[16] =
{
	0x00000000, 0xffaa0000, 0xff00aa00, 0xffaaaa00, 0xff0000aa, 0xff000000, 0xff0055aa, 0xffaaaaaa,
	0xff555555, 0xffff5555, 0xff55ff55, 0xffffff55, 0xff5555ff, 0xffff55ff, 0xff55ffff, 0xffffffff
};

class BitStream
{
protected:
	int mState;
	int mOffset;
	unsigned char *mData;
public:
	void init(void *aData)
	{
		mState = 0;
		mOffset = 0;
		mData = (unsigned char*)aData;
	}

	int get(int aBits)
	{
		int out = 0;
		int bitsout = 0;
		while (bitsout != aBits)
		{
			out >>= 1;
			int data = mData[mOffset];

			if (data & (1 << mState))
			{
				out |= 1 << (aBits - 1);
			}

			mState++;

			if (mState == 8)
			{
				mState = 0;
				mOffset++;
			}

			bitsout++;
		}
		return out;
	}
};


class LZW
{
protected:
	struct LzwData
	{
		unsigned char mData;
		unsigned short mNext;
	};
	LzwData mDictionary[2048];
	int mMaxWordWidth;
	unsigned short mStack[1024];
	int mStackTop;
	int mWordWidth;
	int mWordMask;
	int mDictionaryTop;
	int mPrevIndex;
	int mPrevData;
	BitStream mBitStream;
public:
	void init(void *aData)
	{
		mBitStream.init(aData);
		mStackTop = 0;
		reset();
	}

	void reset()
	{
		mPrevIndex = 0;
		mPrevData = 0;
		mWordWidth = 9;
		mWordMask = (1 << mWordWidth) - 1;
		mDictionaryTop = 0x100;
		mMaxWordWidth = 0xb;
		int i;
		for (i = 0; i < 2048; i++)
		{
			mDictionary[i].mNext = 0xffff;
			mDictionary[i].mData = i & 0xff;
		}
	}

	unsigned char get()
	{
		int tempIndex;
		int index;

		// if pixel stack is empty, get some more data
		if (mStackTop == 0)
		{
			// Decode data from file buffer

			tempIndex = index = mBitStream.get(mWordWidth);

			// If value is outside known dictionary, invent it
			if (index >= mDictionaryTop)
			{
				tempIndex = mDictionaryTop;
				index = mPrevIndex;
				mStack[mStackTop++] = mPrevData;
			}

			// Folow the dictionary, adding each item's pixel to the stack until
			// the end of the list (0xFFFF)
			while (mDictionary[index].mNext != 0xFFFF)
			{
				mStack[mStackTop++] = (index & 0xff00) + mDictionary[index].mData;
				index = mDictionary[index].mNext;
			}

			// Push data in stack
			mPrevData = mDictionary[index].mData;
			mStack[mStackTop++] = mPrevData;

			// Store current position in dictionary
			mDictionary[mDictionaryTop].mNext = mPrevIndex;
			mDictionary[mDictionaryTop].mData = mPrevData;

			mPrevIndex = tempIndex;

			// Next dictionary entry; if this word size is full, grow it
			mDictionaryTop++;
			if (mDictionaryTop > mWordMask)
			{
				mWordWidth++;
				mWordMask <<= 1;
				mWordMask |= 1;
			}

			// If dictionary is full, reset
			if (mWordWidth > mMaxWordWidth)
			{
				reset();
			}
		}
		mStackTop--;
		return mStack[mStackTop];
	}
};


unsigned char * DecodePicData(void *aData, int aWidth, int aHeight)
{
	int len = aWidth * aHeight;

	LZW lzw;
	lzw.init(aData);

	unsigned char * image = new unsigned char[len];

	int rleCount = 0;
	int pixel = 0;

	int i;
	for (i = 0; i < len; i++)
	{
		// If in RLE, we'll just reuse the current 'pixel' value
		if (rleCount > 0)
		{
			rleCount--;
		}
		else
		{
			int data = lzw.get();

			// Is it the RLE opcode?
			if (data != 0x90)
			{
				// Nope, just a pixel.
				pixel = data;
			}
			else
			{
				// Yes, how many repeats?
				int repeat = lzw.get();

				if (repeat == 0)
				{
					// If repeat is 0, it's just encoding the repeat code (0x90)
					pixel = 0x90;
				}
				else
				{
					// Otherwise, set up RLE (note that code can't be 1)
					rleCount = repeat - 2;
				}
			}
		}

		// One byte has two 16-color pixels
		image[i] = pixel & 0x0f;
		i++;
		image[i] = (pixel >> 4) & 0x0f;
	}
	return image;
}

void work(char *aPICname, char *aPNGname)
{
	FILE * f;
	f = fopen(aPICname, "rb");
	if (f == NULL)
		exit(0);
	int len;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char * rawdata = new unsigned char[len];
	fread(rawdata, 1, len, f);
	fclose(f);

	unsigned short * p = (unsigned short*)rawdata;

	int format = p[0];
	int width = p[1];
	int height = p[2];

	unsigned char *image = NULL;

	switch (format)
	{
	case 0x07:
		image = DecodePicData(rawdata + 0x7, width, height);
		break;
	case 0x0f:
		image = DecodePicData(rawdata + 0x17, width, height);
		break;
	}
	delete[] rawdata;

	if (!image)
		exit(0);


	unsigned int *image32 = new unsigned int[width*height];

	int i;
	for (i = 0; i < width*height; i++)
	{
		image32[i] = CA_Palette32_rbswapped[image[i]&0xf];
	}
	delete[] image;

    stbi_write_png(aPNGname, width, height, 4, image32, width * 4);

    delete[] image32;
}




int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		printf("Sid Meier's Covert Action PIC 2 PNG converter\n\n"
			"Usage: pic2png infile.pic outfile.png\n"
			"Output is overwritten without warning if one exists.\n");
		return 0;
	}

	// work, baby
	work(argv[1], argv[2]);

	return 0;
}

