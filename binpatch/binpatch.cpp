#include <stdio.h>
#include <stdlib.h>


int main(int parc, char ** pars)
{
	if (parc < 2)
	{
		printf("Simple binary patcher\n"
			"Usage: binpatch patchfile.txt\n"
			"Patchfile format; every line must have:\n"
			"filename offset value\n"
			"where each line changes one byte of the file.\n\n");
		return 0;
	}
	FILE * f;
	f = fopen(pars[1], "r");
	if (!f)
	{
		printf("'%s' not found\n", pars[1]);
		return 0;
	}
	while (!feof(f))
	{
		char temp[1024];
		int ofs = 0, val = 0;
			
		fscanf(f, "%s %d %d\n", temp, &ofs, &val);
		printf("'%s' offset %d value %d\n", temp, ofs, val);

		FILE * ff = fopen(temp, "r+b");
		if (!ff)
		{
			printf("'%s' not found\n", temp);
		}
		else
		{
			fseek(ff,ofs,SEEK_SET);
			fputc(val, ff);
			fclose(ff);
		}
	}	
	return 0;
}