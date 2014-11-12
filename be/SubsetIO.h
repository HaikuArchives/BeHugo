/*
	SubsetIO.h
*/

#ifndef _SUBSETIO_H
#define _SUBSETIO_H

#include <DataIO.h>
#include <File.h>

class subset_io_data
{
public:
	subset_io_data(BFile *_file, off_t _start, off_t _length);
	~subset_io_data();
	
	BFile *file;
	off_t start;
	off_t length;
};

class SubsetIO : public BPositionIO
{
public:
//	SubsetIO(BPositionIO* io, off_t from);
	SubsetIO(BPositionIO* io, off_t from, off_t to);
	virtual ~SubsetIO();
  
	virtual ssize_t Read(void *buffer, size_t size);
	virtual ssize_t ReadAt(off_t pos, void* buffer, size_t size);
	virtual ssize_t WriteAt(off_t pos, const void* buffer, size_t size);
	virtual off_t Seek(off_t position, uint32 seek_mode);
	virtual off_t Position() const;
	virtual status_t SetSize(off_t size);

private:
	BPositionIO* m_io;
	off_t m_beginOffset;
	off_t m_endOffset;
};

#endif	// ifndef _SUBSETIO_H
