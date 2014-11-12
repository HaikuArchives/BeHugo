/*

Name:
drv_be.cpp

Original author:  John Robinson (Soco)
Maintained by:  Kent Tessman

Description:
Mikmod driver for BeOS R5

*/

#include <SoundPlayer.h>

extern "C"
{
#include <mikmod_internals.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

sem_id _mm_mutex_lists;
sem_id _mm_mutex_vars;
}

//int16 *tmp_buffer;		// for B_AUDIO_FLOAT format


void Be_Mix( void *thecookie, void *buffer, size_t size,
		const media_raw_audio_format &format )
{
	if(!Player_Active()) Player_Stop();
 	
	switch( format.format )
	{
		case media_raw_audio_format::B_AUDIO_SHORT:
		{
			uint16 *buf = (uint16 *)buffer;
			VC_WriteBytes( (SBYTE *)buffer, size  );
			for( size_t x = 0; x < size >> 1;x++)
				buf[x]= (buf[x]);
		}
		break;
/*
		case media_raw_audio_format::B_AUDIO_FLOAT:
		{
			float *buf = (float *)buffer;
			if( (size >> 1) < 32768)
			{
				VC_WriteBytes( (SBYTE *)tmp_buffer, size >> 1 );
				float val =1.0/32800.0;
				for( unsigned int x = 0; x < size >> 2; x ++ )
				{
					buf[x] = ((float)tmp_buffer[x]) * val;
   				}
			}
			break;
		}
*/
		default:
			break;
	}		
}

/****************************************************************************
** Actual Driver
*****************************************************************************/
BSoundPlayer			*soco_bsbp;
media_raw_audio_format		soco_mf;


static BOOL Be_IsThere(void)
{
	return 1;
}

static void Be_Update(void)
{}

static BOOL Be_Init(void)
{
	if(VC_Init()) return 1;

	soco_mf.format 		= media_raw_audio_format::B_AUDIO_SHORT;
	soco_mf.channel_count	= 2; 
	soco_mf.frame_rate	= 44100;
	soco_mf.byte_order	= B_MEDIA_LITTLE_ENDIAN;
	soco_mf.buffer_size	= 8192; //4096;

	soco_bsbp = new BSoundPlayer( &soco_mf, "MikMod", Be_Mix );

//	tmp_buffer = (int16 *) malloc( 65536 );
	
	return 0;
}

static void Be_Exit( void )
{
	delete soco_bsbp;
//	free( tmp_buffer );
}


static BOOL Be_Reset(void)
{
	Be_Exit();
	return Be_Init();
}

static BOOL Be_PlayStart(void)
{
	VC_PlayStart();
	soco_bsbp->Start();
	soco_bsbp->SetVolume( 1.0 ); 
	soco_bsbp->SetHasData( true );
	
	return 0;
}

static void Be_PlayStop(void)
{
	soco_bsbp->Stop();
	VC_PlayStop();
}

MDRIVER drv_be = 
{
	NULL,
	"BeOS Audio Server",
	"BeOS Audio Server v0.01 - by John Robinson",
	0,255,
	"be",
	NULL,
	Be_IsThere,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	Be_Init,
	Be_Exit,
	Be_Reset,
	VC_SetNumVoices,
	Be_PlayStart,
	Be_PlayStop,
	Be_Update,
	NULL,
	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};
