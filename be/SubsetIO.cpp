/*
	SubsetIO.cpp
	
	To get around the problem that Hugo needs to find resources
	at offsets in resource files, while Be's Media Kit expects
	them to be standalone files.
*/

#include <stdio.h>
#include "SubsetIO.h"

subset_io_data::subset_io_data(BFile *_file, off_t _start, off_t _length)
{
	file = _file;
	start = _start;
	length = _length;
}

subset_io_data::~subset_io_data()
{
	// Important:  we delete the BFile here
	delete file;
}

SubsetIO::SubsetIO(BPositionIO* io, off_t from, off_t to) :
		m_io(io),
		m_beginOffset(from),
		m_endOffset(-m_beginOffset)
	{
		off_t end = m_io->Seek(0, SEEK_END);
		if (end >= to)
			m_endOffset += (to - end);
	}

SubsetIO::~SubsetIO()
{
}
  
ssize_t SubsetIO::Read(void *buffer, size_t size)
{
	ssize_t ret = ReadAt(Position(), buffer, size);
	Seek(size, SEEK_CUR);
	return ret;
}

ssize_t SubsetIO::ReadAt(off_t pos, void* buffer, size_t size)
{
	return m_io->ReadAt(pos + m_beginOffset, buffer, size);
}

ssize_t SubsetIO::WriteAt(off_t pos, const void* buffer, size_t size)
{
	return m_io->WriteAt(pos + m_beginOffset, buffer, size);
}

off_t SubsetIO::Seek(off_t position, uint32 seek_mode)
{
	off_t ret = m_io->Seek(position, seek_mode);
	return (seek_mode == SEEK_END) ? ret + m_endOffset : ret;
}

off_t SubsetIO::Position() const
{
	return m_io->Position();
}

status_t SubsetIO::SetSize(off_t size)
{
	return m_io->SetSize(size);
}
