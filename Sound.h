#pragma once

#include <filesystem>

#include <windows.h>
#include <mmeapi.h>
#include <mmreg.h>

#include "Affine3D.h"

class Sound;

class MovementRecorder
{
private:
    unsigned maxn, total, n;
    Affine3D *pos;
    double *time;
    Vector3D *sp;
    void Construct( unsigned _maxn, bool _iof );
public:
    bool iof;
    MovementRecorder();
    MovementRecorder( unsigned _maxn, bool _iof = false );
    ~MovementRecorder();
    void addPoint( const Affine3D& p, double t );
    void removePoint();
    Vector3D getPos( double t0 ) const;
    double getTime( MovementRecorder* e, double t0, double u ) const;
};

class SoundData
{
private:
    WAVEFORMATEX wf;
    BYTE *sound;
    unsigned length;
public:
    SoundData( const std::filesystem::path& path );
    ~SoundData();
    friend Sound;
};

class Sound
{
private:
    HWAVEOUT hWaveOut;

    WAVEHDR wh[2];
    bool prepared[2];

    SoundData *data;

    unsigned processing_type;

    unsigned shift;
    unsigned bufid;
    unsigned sbuflen;
    unsigned tbuflen;

    double stime;

    bool GetBuffer( unsigned cur );
    void ClearBuffer( unsigned cur );
    void ProcessBuffer( unsigned cur );
public:
    static double ssp, recd2, mind2, maxd2;
    MovementRecorder *psnd, *ptrg;
    Vector3D sml;
    Sound();
    ~Sound();
    Sound( HWND hwnd, SoundData* _data, unsigned _processing_type = 0, unsigned samples_in_buf = 5000 );
    //void Delete();
    void Play( double _stime = 0 );
    void Continue();
    inline bool IsPlaying()const
    {
        return prepared[0] || prepared[1];
    };
};
