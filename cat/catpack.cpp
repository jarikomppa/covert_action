#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct CatRecord
{
	char mFilename[13];
	unsigned int mMagic;
	unsigned int mLength;
	unsigned int mOffset;
};

int main(int parc, char ** pars)
{
	if (parc < 2)
	{
		printf("Sid Meier's Covert Action CAT file packer\n\n"
			   "Usage: catpack filename.log\n"
			   "Will pack all files listed in filename.log into filename.cat\n");
		return 0;
	}

	FILE *f = fopen(pars[1], "rb");
	if (!f)
	{
		printf("'%s' not found\n", pars[1]);
		return 0;
	}
	char * tempbuf = new char[1024*1024]; // one meg should be enough..
	char temp[100];

	fseek(f,0,SEEK_END);
	int loglen = ftell(f);
	fseek(f,0,SEEK_SET);
	fread(tempbuf,1,loglen,f);
	tempbuf[loglen] = 0;
	fclose(f);

	short records = 0;
	
	// let's say we have a cap of 1024 records and do this in a lazy way..
	CatRecord *record = new CatRecord[1024];
	memset(record, 0, sizeof(CatRecord) * 1024);

	int ofs = 0;
	char *p = tempbuf;
	int i = 0;
	while (*p)
	{
		temp[i] = *p;
		p++;
		i++;
		if (*p == '\n' || *p == 0)
		{
			if (*p == '\n') p++;
			temp[i] = 0;
			i = 0;
			FILE * f = fopen(temp,"rb");
			if (f) 
			{
				memcpy(record[records].mFilename, temp, strlen(temp) + 1);
				record[records].mMagic = 0x41434D53;
				record[records].mOffset = ofs;
				fseek(f, 0, SEEK_END);
				record[records].mLength = ftell(f);
				ofs += record[records].mLength;
				records++;
				fclose(f);
			}
			else
			{
				printf("'%s' not found\n", temp);
			}
		}
	}


	printf("Will write %d records\n", records);

	// fix offsets
	for (i = 0; i < records; i++)
		record[i].mOffset += 2 + records * 24;
		
	int l = sprintf(temp, "%s", pars[1]);
	temp[l-3] = 'c';
	temp[l-2] = 'a';
	temp[l-1] = 't';

	FILE *catfile = fopen(temp, "wb");
	fwrite(&records, 1, 2, catfile);	

	for (i = 0; i < records; i++)
	{
		fwrite(record[i].mFilename, 1, 12, catfile);
		fwrite(&record[i].mMagic, 1, 4, catfile);
		fwrite(&record[i].mLength, 1, 4, catfile);
		fwrite(&record[i].mOffset, 1, 4, catfile);
	}

	for (i = 0; i < records; i++)
	{
		FILE * f = fopen(record[i].mFilename, "rb");
		fread(tempbuf, 1, record[i].mLength, f);
		fwrite(tempbuf, 1, record[i].mLength, catfile);
		fclose(f);
	}
	fclose(catfile);
	
	delete[] tempbuf;

	return 0;
}