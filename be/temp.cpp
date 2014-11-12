/*
	temp.cpp
*/

#include <stdio.h>
#include <stdlib.h>
#include <Alert.h>
#include <File.h>

#define TRANSFER_BLOCK_SIZE 4096

/* CreateResourceCache

	The supplied FILE structure describes a file that is opened
	and positioned; we just have to read length bytes from it
	and write the resource to newfile.  (For clarification,
	this function used to be called CreateTemporaryFile.)

	In any case, it returns with the original file pointer CLOSED.
*/

int CreateResourceCache(char **tempfilename, FILE *file, long length)
{
	unsigned char buf[TRANSFER_BLOCK_SIZE];
	size_t bytesread;
	long totalbytes = 0;

	// tempfilename should be NULL before calling this the first time
	if (*tempfilename) free(*tempfilename);

	// Create a temporary filename
	*tempfilename = tempnam("/tmp", "he");
	
	BFile newfile(*tempfilename, B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
	if (newfile.InitCheck()!=B_OK) goto Error;

	do 
	{
		bytesread = fread(buf, 1, TRANSFER_BLOCK_SIZE, file);
		if (ferror(file))
		{
			goto Error;
		}

		newfile.Write(buf, bytesread);
		totalbytes += bytesread;
	}
	while (bytesread==TRANSFER_BLOCK_SIZE && totalbytes < length);

	fclose(file);
	return true;

Error:
	fclose(file);
	BAlert *alert = new BAlert("Hugo Engine",
		"Unable to create resource cache file.",
		"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->Go();
	return false;
}
