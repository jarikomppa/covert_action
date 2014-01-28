#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "stb/stb_image.h"

// Link with stb_image.c

// standard dos 16 color palette (except dos was 6 bit, this is 8 bit scaled from the 6 bit values)
unsigned int DOS_Palette32[16] =
{
	0xff000000, 0xff0000aa, 0xff00aa00, 0xff00aaaa, 0xffaa0000, 0xffaa00aa, 0xffaa5500, 0xffaaaaaa,
	0xff555555, 0xff5555ff, 0xff55ff55, 0xff55ffff, 0xffff5555, 0xffff55ff, 0xffffff55, 0xffffffff
};

// covert action palette is the same, except index 5 is set to black
unsigned int CA_Palette32[16] =
{
	0xff000000, 0xff0000aa, 0xff00aa00, 0xff00aaaa, 0xffaa0000, 0xff000000, 0xffaa5500, 0xffaaaaaa,
	0xff555555, 0xff5555ff, 0xff55ff55, 0xff55ffff, 0xffff5555, 0xffff55ff, 0xffffff55, 0xffffffff
};

class BitStreamWriter
{
protected:
	int mState;
	int mOffset;
	int mPartial;
	unsigned char *mData;
public:
	void init(void *aData)
	{
		mPartial = 0;
		mState = 0;
		mOffset = 0;
		mData = (unsigned char*)aData;
	}

	void put(int aData, int aBits)
	{
		mPartial |= aData << mState;
		mState += aBits;
		while (mState >= 8)
		{
			mData[mOffset] = mPartial & 0xff;
			mPartial >>= 8;
			mState -= 8;
			mOffset++;
		}
		mData[mOffset] = mPartial;
	}

	int getLength()
	{
		if (mState) return mOffset + 1;
		return mOffset;
	}
};

class LZWWriter
{
protected:
	struct LzwData
	{
		unsigned char mData;
		unsigned short mNext;
	};
	LzwData mDictionary[2048];
	int mMaxWordWidth;
	int mWordMask;
	int mWordWidth;
	int mDictionaryTop;
	BitStreamWriter mBitStream;
public:
	void init(void *aData)
	{
		mBitStream.init(aData);
		reset();
	}

	void reset()
	{
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

	int getLength()
	{
		return mBitStream.getLength();
	}

	void finish()
	{
		// flush the last open code word (none with the trivial implementation)
	}

	void put(unsigned char aData)
	{
	    // Trivial (and pretty pessimal) lzw "comrpessor" implementation:
	    // - Always just store 1 symbol codes.
	    // - Keep track of dictionary size so we reset the dictionary
	    //   (and word size) at correct places.
		mBitStream.put(aData, mWordWidth);
		mDictionaryTop++;
		if (mDictionaryTop > mWordMask)
		{
			mWordWidth++;
			mWordMask <<= 1;
			mWordMask |= 1;
			if (mWordWidth > mMaxWordWidth)
			{
				reset();
			}
		}
	}
};


int find_color(unsigned int c)
{
	int r1 = (c >> 16) & 0xff;
	int g1 = (c >> 8) & 0xff;
	int b1 = c & 0xff;

    // If alpha is zero (or nearly so), use color 0
    if (((c >> 24) & 0xff) < 0x3f)
    {
        return 0;
    }

	int distance = 0xffffff;
	int col = 0;

    // Go through all of the colors in palette and pick
    // the one with the least geometrical distance to
    // the desired color.
    // Note that we skip color 0. This makes sure that
    // any black pixels (with alpha) will pick color 5.
	int i;
	for (i = 1; i < 16; i++)
	{
		int r2 = CA_Palette32[i] & 0xff;
		int g2 = (CA_Palette32[i] >> 8) & 0xff;
		int b2 = (CA_Palette32[i] >> 16) & 0xff;

		int rd = r2 - r1;
		int gd = g2 - g1;
		int bd = b2 - b1;

		int dist = (int)floor(sqrt((float)rd*rd+(float)gd*gd+(float)bd*bd));

		if (dist < distance)
		{
			distance = dist;
			col = i;
		}
	}
	return col;
}


unsigned char *encodePicData(unsigned char *data, int width, int height, int &datalen)
{
    // Allocate "big enough" buffer for output buffer.
    // Even at the worst case, the data should take less than
    // 320*200*0.5*2*1.5 bytes..
    // - 1 byte = 2 pixels
    // - max. encoding is 2 bytes for 2 pixels (0x90 code)
    // - max. word length is 12 bits (1.5 bytes) per byte
	unsigned char *d = new unsigned char[width*height*2];
	LZWWriter lzw;
	lzw.init(d);
	int i;
	int repeats = 0;
	int last_pixel = -1;
	for (i = 0; i < width * height; i++)
	{
		int pix = data[i];
		i++;
		pix |= data[i] << 4;

        // Cut repeat at 250 just in case, and break repeat at the end of image
		if (pix == last_pixel && repeats < 250 && i < width * height - 1)
		{
			repeats++;
		}
		else
		{
			if (repeats == 0)
			{
                // If repeat is zero, just send out the pixel.
				lzw.put(pix);
				// If pix is 0x90, we need to send the zero repeat count too..
				if (pix == 0x90) lzw.put(0);
			}
			else
			if (repeats == 1)
			{
			    // If repeat is 1, send out the old pixel and the new
			    // (since repeat of 1 is illegal), again making sure we're not
			    // breaking the 0x90 codes..
				lzw.put(last_pixel);
				if (last_pixel == 0x90) lzw.put(0);
				lzw.put(pix);
				if (pix == 0x90) lzw.put(0);
			}
			else
			{
			    // Otherwise, set up the repeat..
				lzw.put(0x90);
				lzw.put(repeats+1);
				// ..and make sure we send the current pixel too.
				lzw.put(pix);
				if (pix == 0x90) lzw.put(0);
			}
			repeats = 0;
		}
		last_pixel = pix;
	}

    // call lzw.finish to make sure it's finished the current
    // code point properly.
	lzw.finish();
	datalen = lzw.getLength();
	return d;
}


void work(char *aPNGname, char *aPICname)
{
	int x,y,n;
	unsigned int *data = (unsigned int*)stbi_load(aPNGname, &x, &y, &n, 4);

	if (!data)
    {
        printf("'%s' load failed (not found?)\n", aPNGname);
		return;
    }

    // Allocate our 8-bit buffer
	unsigned char *image = new unsigned char[x*y];

	int i;

	// Map colors to closest ones in the palette..
	for (i = 0; i < x*y; i++)
	{
		image[i] = find_color(data[i]);
	}

	int len;
	unsigned char * picdata = encodePicData(image, x, y, len);
	delete[] image;

	if (picdata)
	{
		FILE * f = fopen(aPICname, "wb");
		unsigned short tag = 0x7;
		unsigned short w = x;
		unsigned short h = y;
		unsigned char b = 0xb;
		// Output header.
		fwrite(&tag, 1, 2, f); // 0x7 means no CGA lookup
		fwrite(&w, 1, 2, f);   // width
		fwrite(&h, 1, 2, f);   // height
		fwrite(&b, 1, 1, f);   // 0xb byte (max. dictionary word width)
		fwrite(picdata, 1, len, f); // ..and the LZW data
		fclose(f);
	}

    delete[] picdata;
}


int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		printf("Sid Meier's Covert Action PNG 2 PIC converter\n\n"
			"Usage: png2pic infile.png outfile.pic\n"
			"Output is overwritten without warning if one exists.\n"
			"Colors are mapped to the closest SMCA palette entry\n"
			"Colors with zero alpha are mapped to color index 0\n");
		return 0;
	}

	// work, baby
	work(argv[1], argv[2]);

	return 0;
}

