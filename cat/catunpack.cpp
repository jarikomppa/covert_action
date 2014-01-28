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
		printf("Sid Meier's Covert Action CAT file unpacker\n\n"
			   "Usage: catunpack filename.cat\n"
			   "Will save all of the files from inside the .cat as separate files,\n"
			   "and generate filename.log containing the filenames (for repacking)\n");
		return 0;
	}

	FILE *f = fopen(pars[1], "rb");
	if (!f)
	{
		printf("'%s' not found\n", pars[1]);
		return 0;
	}

	short records = 0;
	fread(&records, 1, 2, f);	
	printf("%s contains %d records\n", pars[1], records);
	
	CatRecord *record = new CatRecord[records];
	memset(record, 0, sizeof(CatRecord) * records);

	char temp[100];
	int l = sprintf(temp, "%s", pars[1]);
	temp[l-3] = 'l';
	temp[l-2] = 'o';
	temp[l-1] = 'g';

	FILE *logfile = fopen(temp, "wb");

	int i;
	for (i = 0; i < records; i++)
	{
		fread(record[i].mFilename, 1, 12, f);
		fread(&record[i].mMagic, 1, 4, f);
		fread(&record[i].mLength, 1, 4, f);
		fread(&record[i].mOffset, 1, 4, f);
		printf("'%s', %08X, ofs %d, len %d\n", record[i].mFilename, record[i].mMagic, record[i].mOffset, record[i].mLength);
		fprintf(logfile, "%s\n", record[i].mFilename);
	}
	fclose(logfile);

	char *tempbuf = new char[1024*1024]; // one meg ought to be enough

	for (i = 0; i < records; i++)
	{
		FILE * outfile = fopen(record[i].mFilename, "wb");
		fseek(f, record[i].mOffset, SEEK_SET);
		fread(tempbuf, 1, record[i].mLength, f);
		fwrite(tempbuf, 1, record[i].mLength, outfile);
		fclose(outfile);
	}
	
	delete[] tempbuf;

	fclose(f);

	return 0;
}