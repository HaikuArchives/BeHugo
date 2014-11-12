/*
	PICTURE.CPP

	BeOS image file display routines, including the main function
	called from the engine:

		hugo_displaypicture

	for the Hugo Engine

	Copyright (c) 1995-2006 by Kent Tessman
*/

#define USE_BILINEARSTRETCHBLT

#if !defined (NO_GRAPHICS)

#include <DataIO.h>
#include <TranslationUtils.h>

#include "behugo.h"

extern "C"
{
#include "heheader.h"

int hugo_displaypicture(FILE *infile, long length);
}

BFile *TrytoOpenBFile(char *name, char *where);
int DisplayBBitmap(BBitmap *img);
int DisplayJPEG(BFile *file, long reslength);

#ifdef USE_BILINEARSTRETCHBLT
bool BilinearStretchBlt(BBitmap *dest_bmp, BBitmap *src_bmp);
#endif


int hugo_hasgraphics(void)
{
	return display_graphics;
}

/* hugo_displaypicture

	(This is the only routine that gets called from the engine proper.)

	Assumes that "filename" is hot--i.e., opened and positioned--but
	closes it before returning.

	Loads and displays a JPEG picture, sizing it to the screen area
	described by the current text window.  If smaller than the
	current text window, the image should be centered, not stretched.

	Ultimately, graphic formats other than JPEG may be supported,
	hence the identification of the type flag and the switch
	at the end.

	Returns false if it fails because of an ERROR.
*/

int hugo_displaypicture(FILE *infile, long reslength)
{
	long pos;
	BFile *file;
	
	/* So that graphics can be dynamically enabled/disabled */
	if (!hugo_hasgraphics())
	{
		fclose(infile);
		return true;		/* not an error */
	}
	
	// Reopen infile as a BFile at the same position
	pos = ftell(infile);
	fclose(infile);
	// If the resource is not blank, we're using a resource file,
	// so uppercase the name
	if (strcpy(loaded_resname, "")) strupr(loaded_filename);
	if (!(file = TrytoOpenBFile(loaded_filename, "games")))
	{
		if (!(file = TrytoOpenBFile(loaded_filename, "object")))
		{
			return false;
		}
	}
	// Seek BFile to the same position infile was at
	file->Seek((off_t)pos, SEEK_SET);	

	/* Before doing any drawing, mainly because we need to make sure there
	   is no scroll_offset, or anything like that:
	*/
	view->Update(false);

	switch (resource_type)
	{
		case JPEG_R:
			if (!DisplayJPEG(file, reslength)) goto Failed;
			break;

		default:        /* unrecognized */
#if defined (DEBUGGER)
			SwitchtoDebugger();
			DebugMessageBox("Picture Loading Error", "Unrecognized graphic format");
			SwitchtoGame();
#endif
			goto Failed;
	}

	delete file;
	return 1;	// success

Failed:
	delete file;
	return 0;
}


/* TrytoOpenBFile
 * 
 *   is similar to hemisc.c's TrytoOpen, but returns a BFile.
 */

extern char gamepath[];  // from hemisc.c

BFile *TrytoOpenBFile(char *name, char *where)
{
	char drive[MAXDRIVE], dir[MAXDIR], fname[MAXFILENAME], ext[MAXEXT];
	char envvar[32];
	BFile *tempfile;
	char temppath[MAXPATH];

	/* Try to open the given, vanilla filename */
	tempfile = new BFile(name, B_READ_ONLY);
	if (tempfile->InitCheck()==B_OK)
		return tempfile;
	delete tempfile;

	hugo_splitpath(name, drive, dir, fname, ext);  /* file to open */

	/* If the given filename doesn't already specify where to find it */
	if (!strcmp(drive, "") && !strcmp(dir, ""))
	{
		/* Check gamefile directory */
		hugo_makepath(temppath, "", gamepath, fname, ext);
		tempfile = new BFile(temppath, B_READ_ONLY);
		if (tempfile->InitCheck()==B_OK)
			return tempfile;
		delete tempfile;

		/* Check environment variables */
		strcpy(envvar, "hugo_");  /* make up the actual var. name */
		strcat(envvar, where);

		if (getenv(strupr(envvar)))
		{
			hugo_makepath(temppath, "", getenv(strupr(envvar)), fname, ext);

			tempfile = new BFile(temppath, B_READ_ONLY);
			if (tempfile->InitCheck()==B_OK)
				return tempfile;
			delete tempfile;
		}
	}

	/* return NULL if not openable */
	return NULL;
}


/* DisplayBBitmap
 * 
 *   is called by the completed image-loading routine (such as
 *   DisplayJPEG) in order to copy the loaded BBitmap to visible_view.
 */

int DisplayBBitmap(BBitmap *img)
{
	float width, height, window_width, window_height, ratio;
	window_width = physical_windowwidth-1.0;
	window_height = physical_windowheight-1.0;
	
	width = img->Bounds().Width();
	height = img->Bounds().Height();
	
	ratio = width/height;
	if (width > window_width)
	{
		width = window_width;
		height = width/ratio;
	}
	if (height > window_height)
	{
		height = window_height;
		width = height*ratio;
	}

	// floor() everything to match video bounds calculation
	float x = floor(physical_windowleft + window_width/2.0 - width/2.0);
	float y = floor(physical_windowtop + window_height/2.0 - height/2.0);
	width = floor(width);
	height = floor(height);

	BRect rect(x, y, x+width, y+height);

	if (bitmap->Lock())
	{
#ifdef USE_BILINEARSTRETCHBLT
		if (!graphics_smoothing ||
			((int)img->Bounds().Width()<=physical_windowwidth &&
			(int)img->Bounds().Height()<=physical_windowheight))
		{
			view->DrawBitmap(img, rect);
		}
		else
		{
			BBitmap scaled_image(rect, B_RGB32);
			BilinearStretchBlt(&scaled_image, img);
			view->DrawBitmap(&scaled_image, rect);
		}
#else
		view->DrawBitmap(img, rect);
#endif
		bitmap->Unlock();
	}
		
	return true;
}


/* DisplayJPEG */

int DisplayJPEG(BFile *file, long reslength)
{
	// Create a BMemoryIO object to hold the image data
	char *buffer;
	if (!(buffer = (char *)malloc(reslength*sizeof(char)))) return false;
	BMemoryIO memio(buffer, reslength);
	
	// Read the JPEG data into the buffer
	file->Read(buffer, reslength);
	
	// Call the translator to read the JPEG data into a BBitmap
	BBitmap *img = BTranslationUtils::GetBitmap(&memio);

	int result = false;
	if (img!=NULL)
	{
		result = DisplayBBitmap(img);
		delete img;
	}
	free(buffer);
	return result;
}

//---------------------------------------------------------------------------
// BilinearStretchBlt
//---------------------------------------------------------------------------

#ifdef USE_BILINEARSTRETCHBLT

inline rgb_color BitmapPixelAt(unsigned char *src, int bpr, int x, int y)
{
	rgb_color c;
	c.red   = *(src + y*bpr + x*4+2);
	c.green = *(src + y*bpr + x*4+1);
	c.blue  = *(src + y*bpr + x*4);
	return c;
}

inline void SetBitmapPixelAt(unsigned char *dest, int bpr, int x, int y, rgb_color c)
{
	*(dest + y*bpr + x*4+2) = c.red;
	*(dest + y*bpr + x*4+1) = c.green;
	*(dest + y*bpr + x*4)   = c.blue;
}

bool BilinearStretchBlt(BBitmap *dest_bmp, BBitmap *src_bmp)
{
	unsigned char *dest, *src;
	int dest_width, dest_height, src_width, src_height;
	int dest_bpr, src_bpr;
	double nXFactor, nYFactor;
	double fraction_x, fraction_y, one_minus_x, one_minus_y;
	int ceil_x, ceil_y, floor_x, floor_y;
	rgb_color c, c1, c2, c3, c4;
	unsigned char b1, b2;
	int x, y;

	dest = (unsigned char *)dest_bmp->Bits();
	dest_width = (int)dest_bmp->Bounds().Width();
	dest_height = (int)dest_bmp->Bounds().Height();
	dest_bpr = dest_bmp->BytesPerRow();
	src = (unsigned char *)src_bmp->Bits();
	src_width = (int)src_bmp->Bounds().Width();
	src_height = (int)src_bmp->Bounds().Height();
	src_bpr = src_bmp->BytesPerRow();
	
	nXFactor = (double)src_width/(double)dest_width;
	nYFactor = (double)src_height/(double)dest_height;

	for (x=0; x<=dest_width; x++)
	for (y=0; y<=dest_height; y++)
	{
		floor_x = (int)floor(x * nXFactor);
		floor_y = (int)floor(y * nYFactor);
		ceil_x = floor_x + 1;
		if (ceil_x >= src_width) ceil_x = floor_x;
		ceil_y = floor_y + 1;
		if (ceil_y >= src_height) ceil_y = floor_y;
		fraction_x = x * nXFactor - floor_x;
		fraction_y = y * nYFactor - floor_y;
		one_minus_x = 1.0 - fraction_x;
		one_minus_y = 1.0 - fraction_y;

		c1 = BitmapPixelAt(src, src_bpr, floor_x, floor_y);
		c2 = BitmapPixelAt(src, src_bpr, ceil_x,  floor_y);
		c3 = BitmapPixelAt(src, src_bpr, floor_x, ceil_y);
		c4 = BitmapPixelAt(src, src_bpr, ceil_x,  ceil_y);

		// Blue
		b1 = (unsigned char)(one_minus_x * c1.blue + fraction_x * c2.blue);
		b2 = (unsigned char)(one_minus_x * c3.blue + fraction_x * c4.blue);
		c.blue = (unsigned char)(one_minus_y * (double)(b1) + fraction_y * (double)(b2));

		// Green
		b1 = (unsigned char)(one_minus_x * c1.green + fraction_x * c2.green);
		b2 = (unsigned char)(one_minus_x * c3.green + fraction_x * c4.green);
		c.green = (unsigned char)(one_minus_y * (double)(b1) + fraction_y * (double)(b2));

		// Red
		b1 = (unsigned char)(one_minus_x * c1.red + fraction_x * c2.red);
		b2 = (unsigned char)(one_minus_x * c3.red + fraction_x * c4.red);
		c.red = (unsigned char)(one_minus_y * (double)(b1) + fraction_y * (double)(b2));

		SetBitmapPixelAt(dest, dest_bpr, x, y, c);
	}

	return true;
}

#endif	// USE_BILINEARSTRETCHBLT

#else	// NO_GRAPHICS

extern "C"
{

#include "heheader.h"

int hugo_hasgraphics(void)
{
	return 0;
}

int hugo_displaypicture(FILE *infile)
{
	fclose(infile);
	return 1;	// not an error
}

}  // extern "C"

#endif  // NO_GRAPHICS
