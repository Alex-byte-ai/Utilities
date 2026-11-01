#pragma once

#include <filesystem>

#include <windows.h>
#include <mmeapi.h>
#include <mmreg.h>

#include "Affine3D.h"

class SoundData
{
private:
    WAVEFORMATEX wf;
    BYTE *sound;
    unsigned length;
public:
    SoundData( const std::filesystem::path& path );
    ~SoundData();
    friend class Sound;
};

class Sound
{
private:
    HWAVEOUT hWaveOut;

    WAVEHDR wh[2];
    bool prepared[2];

    SoundData *data;

    unsigned shift;
    unsigned bufid;
    unsigned sbuflen;

    bool GetBuffer( unsigned cur );
    void ClearBuffer( unsigned cur );
    void ProcessBuffer( unsigned cur );
public:
    Sound();
    ~Sound();
    Sound( HWND hwnd, SoundData* _data, unsigned samples_in_buf = 5000 );

    void Play();
    void Continue();

    inline bool IsPlaying()const
    {
        return prepared[0] || prepared[1];
    };
};
