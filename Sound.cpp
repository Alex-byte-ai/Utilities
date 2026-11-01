#include "Sound.h"

#include <fstream>

#include "Exception.h"
#include "Basic.h"

// Add "winmm" to linker

#pragma pack(push, 1)
struct RIFFHEADER
{
    uint32_t index;
    uint32_t size;
};

struct FMTDATA
{
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t avgBytesPerSec;
    uint16_t blockAlign;
    uint16_t significantBitsPerSample;
};
#pragma pack(pop)

SoundData::SoundData( const std::filesystem::path& path )
{
    std::ifstream file( path, std::ios::binary );
    FMTDATA format_data;
    RIFFHEADER header;
    uint32_t type;

    sound = nullptr;
    length = 0;

    // File does not exist
    makeException( file );

    makeException( file.read( ( char* )&header, sizeof( header ) ) );
    makeException( file.read( ( char* )&type, sizeof( type ) ) );

    // This is not a correct WAV file
    makeException( header.index == 0x46464952 && type == 0x45564157 );

    while( file.read( ( char* )&header, sizeof( header ) ) )
    {
        if( header.index == 0x20746d66 )
        {
            makeException( sizeof( format_data ) <= header.size );
            makeException( file.read( ( char* )&format_data, sizeof( format_data ) ) );
            file.seekg( header.size - sizeof( format_data ), std::ios::cur );

            wf.wFormatTag = format_data.audioFormat;
            wf.nChannels = format_data.numChannels;
            wf.nSamplesPerSec = format_data.sampleRate;
            wf.nAvgBytesPerSec = format_data.avgBytesPerSec;
            wf.nBlockAlign = format_data.blockAlign;
            wf.wBitsPerSample = format_data.significantBitsPerSample;
            wf.cbSize = 0;

            // Compressed WAV files are not supported
            makeException( wf.wFormatTag == WAVE_FORMAT_PCM );
        }
        else if( header.index == 0x61746164 )
        {
            length = header.size;
            sound = new BYTE[length];
            makeException( file.read( ( char* )sound, length ) );
            return;
        }
        else
        {
            file.seekg( header.size, std::ios::cur );
        }
    }
}

SoundData::~SoundData()
{
    delete[] sound;
}

bool Sound::GetBuffer( unsigned cur )
{
    unsigned len;
    ClearBuffer( cur );
    if( shift < data->length )
    {
        wh[cur].dwBufferLength = len = Min( sbuflen, data->length - shift );
        wh[cur].dwFlags = 0;
        wh[cur].dwLoops = 0;
        wh[cur].dwUser = ( long long unsigned )this;

        ProcessBuffer( cur );

        waveOutPrepareHeader( hWaveOut, &wh[cur], sizeof( WAVEHDR ) );
        shift += len;
        prepared[cur] = true;
        waveOutWrite( hWaveOut, &wh[cur], sizeof( WAVEHDR ) );
        return true;
    }
    return false;
}

void Sound::ClearBuffer( unsigned cur )
{
    if( prepared[cur] )
    {
        waveOutUnprepareHeader( hWaveOut, &wh[cur], sizeof( WAVEHDR ) );
        prepared[cur] = false;
    }
}

void Sound::ProcessBuffer( unsigned cur )
{
    wh[cur].lpData = ( char* )data->sound + shift;
}

Sound::Sound()
{
    hWaveOut = nullptr;
    data = nullptr;
}

Sound::Sound( HWND hwnd, SoundData* _data, unsigned samples_in_buf )
{
    data = _data;

    sbuflen = data->wf.nBlockAlign * samples_in_buf;
    wh[0].lpData = nullptr;
    wh[1].lpData = nullptr;

    waveOutOpen( &hWaveOut, WAVE_MAPPER, &data->wf, reinterpret_cast<DWORD_PTR>( hwnd ), 0, CALLBACK_WINDOW );

    prepared[0] = prepared[1] = false;

    shift = 0;
    bufid = 0;
}

Sound::~Sound()
{
    // Sound deleted while playing
    makeException( !IsPlaying() );

    ClearBuffer( 0 );
    ClearBuffer( 1 );
    waveOutClose( hWaveOut );
}

void Sound::Play()
{
    // Trying to play sound, that is already playing
    makeException( !IsPlaying() );

    shift = 0;
    bufid = 0;
    if( GetBuffer( 0 ) )
    {
        GetBuffer( 1 );
    }
}

void Sound::Continue()
{
    // Trying to continue sound, that is not playing
    makeException( IsPlaying() );

    GetBuffer( bufid );
    bufid = ( bufid + 1 ) % 2;
}
