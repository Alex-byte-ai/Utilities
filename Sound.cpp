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

void MovementRecorder::Construct( unsigned _maxn, bool _iof )
{
    iof = _iof;
    maxn = _maxn;
    if( maxn < 2 )
    {
        maxn = 2;
    }
    n = maxn - 1;
    total = 0;
    pos = new Affine3D[maxn];
    time = new double[maxn];
    sp = new Vector3D[maxn];
}

MovementRecorder::MovementRecorder()
{
}

MovementRecorder::MovementRecorder( unsigned _maxn, bool _iof )
{
    Construct( _maxn, _iof );
}

MovementRecorder::~MovementRecorder()
{
    delete[] pos;
    delete[] time;
    delete[] sp;
}

void MovementRecorder::addPoint( const Affine3D& p, double t )
{
    ++total;
    n = ( n + 1 ) % maxn;
    pos[n] = p;
    time[n] = t;
    if( n > 0 )
    {
        sp[n] = sp[n   - 1] = ( pos[n].s - pos[n   - 1].s ) / ( time[n] - time[n   - 1] );
    }
    else
    {
        sp[0] = sp[maxn - 1] = ( pos[0].s - pos[maxn - 1].s ) / ( time[0] - time[maxn - 1] );
    }
}

void MovementRecorder::removePoint()
{
    if( total > 0 )
    {
        if( n == 0 )
        {
            n = maxn;
        }
        --n;
        --total;
    }
}

Vector3D MovementRecorder::getPos( double t0 ) const
{
    double ti;
    unsigned i = n, j = total;
    if( j > maxn )
    {
        j = maxn;
    }
    if( iof )
    {
        ti = 1.0 / 0.0;
        goto check_interval;
    }
    while( j > 0 )
    {
        ti = time[i];
        if( i == 0 )
        {
            i = maxn;
        }
        --i;
        --j;
check_interval:
        if( ( time[i] <= t0 ) && ( t0 <= ti ) )
        {
            return pos[i].s + sp[i] * ( t0 - time[i] );
        }
    }
    // Record was not found
    makeException( false );
    return Vector3D();
}

double MovementRecorder::getTime( MovementRecorder* e, double t0, double u )const
{
    Vector3D dr;
    double dt, ti;
    unsigned i = n, j = total;
    if( j > maxn )
    {
        j = maxn;
    }
    if( iof )
    {
        ti = 1.0 / 0.0;
        goto check_interval;
    }
    while( j > 0 )
    {
        ti = time[i];
        if( i == 0 )
        {
            i = maxn;
        }
        --i;
        --j;
check_interval:
        dr = e->getPos( t0 ) - pos[i].s - sp[i] * ( t0 - time[i] );
        dt = ( -dr * sp[i] + Sqrt( u * u * dr.Sqr() - sp[i].M( dr ).Sqr() ) ) / ( u * u - sp[i].Sqr() );
        if( ( time[i] <= t0 + dt ) && ( t0 + dt <= ti ) )
        {
            return dt;
        }
    }
    // Record was not found
    makeException( false );
    return -1;
}

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


double Sound::ssp = 330;
double Sound::recd2 = 25;
double Sound::mind2 = recd2;
double Sound::maxd2 = Sound::ssp * Sound::ssp * 31 * 31;

bool Sound::GetBuffer( unsigned cur )
{
    unsigned len;
    ClearBuffer( cur );
    if( shift < data->length )
    {
        wh[cur].dwBufferLength = len = Min( sbuflen, data->length - shift );
        wh[cur].dwFlags = 0;
        wh[cur].dwLoops = 0;
        wh[cur].dwUser = reinterpret_cast<long long unsigned>( this );

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
    if( ( processing_type == 'l' ) || ( processing_type == 'r' ) )
    {
        unsigned csz = data->wf.nBlockAlign / 2;
        short value0, value1;
        if( sizeof( value0 ) == csz )
        {
            BYTE *in = data->sound + shift, *out;
            memset( wh[cur].lpData, 0, tbuflen );

            double t0, t1, dt0, dt1;
            double i0, i1;
            double len;
            unsigned j, j0, j1;
            unsigned ssh;

            if( processing_type == 'l' )
            {
                ssh = 0;
            }
            if( processing_type == 'r' )
            {
                ssh = csz;
            }

            double lt1 = -1, ldt1, ptime;
            unsigned rsn = 0;
            unsigned i = 0, n = wh[cur].dwBufferLength / data->wf.nBlockAlign;
            double d0d;
            while( i < n )
            {
                t0 = i + shift / data->wf.nBlockAlign;
                t1 = t0 + 1;
                t0 /= data->wf.nSamplesPerSec;
                t1 /= data->wf.nSamplesPerSec;
                t0 += stime;
                t1 += stime;

                if( lt1 < 0 )
                {
                    dt0 = ptrg->getTime( psnd, t0, ssp );
                    t0 += dt0;
                    ptime = t0;
                }
                else
                {
                    t0 = lt1;
                    dt0 = ldt1;
                }

                dt1 = ptrg->getTime( psnd, t1, ssp );
                t1 += dt1;
                lt1 = t1;
                ldt1 = dt1;

                i0 = ( t0 - ptime ) * data->wf.nSamplesPerSec;
                i1 = ( t1 - ptime ) * data->wf.nSamplesPerSec;
                j0 = floor( i0 );
                j1 = ceil( i1 );
                if( j1 > rsn )
                {
                    rsn = j1;
                }
                d0d = 0.5 * recd2 / Max( Sqr( ssp * ( dt0 + dt1 ) * 0.5 ), mind2 );
                for( j = j0; j < j1; ++j )
                {
                    len = Min( ( double )( j + 1 ), i1 ) - Max( ( double )j, i0 );
                    out = ( BYTE* )wh[cur].lpData + j * data->wf.nBlockAlign + ssh;
                    memcpy( &value0, in + ssh, csz );
                    memcpy( &value1, out, csz );

                    value0 = value0 * d0d * len + value1;
                    memcpy( out, &value0, csz );
                }
                in += data->wf.nBlockAlign;
                ++i;
            }
            lt1 = -1;
            wh[cur].dwBufferLength = rsn * data->wf.nBlockAlign;
        }
        else
        {
            // Sound sample has wrong size
            makeException( false );
        }
    }
    else if( processing_type == '3' )
    {
        unsigned csz = data->wf.nBlockAlign / 2;
        short value;
        double tmp;
        if( sizeof( value ) == csz )
        {
            BYTE *in = data->sound + shift, *out = ( BYTE* )wh[cur].lpData;
            memset( out, 0, tbuflen );

            unsigned i = 0, n = wh[cur].dwBufferLength / data->wf.nBlockAlign;
            double d0d = 0.5 * recd2 / Max( sml.Sqr(), mind2 ), sinfi = sml.y / sml.Abs();
            while( i < n )
            {
                memcpy( &value, in, csz );
                tmp = value * d0d * ( 1 + sinfi );
                value = tmp;
                memcpy( out, &value, csz );

                memcpy( &value, in + csz, csz );
                tmp = value * d0d * ( 1 - sinfi );
                value = tmp;
                memcpy( out + csz, &value, csz );

                out += data->wf.nBlockAlign;
                in += data->wf.nBlockAlign;
                ++i;
            }
        }
        else
        {
            // Sound sample has wrong size
            makeException( false );
        }
    }
    else
    {
        wh[cur].lpData = ( char* )data->sound + shift;
    }
}

Sound::Sound()
{
    hWaveOut = nullptr;
    data = nullptr;
    psnd = nullptr;
    ptrg = nullptr;
}

Sound::Sound( HWND hwnd, SoundData* _data, unsigned _processing_type, unsigned samples_in_buf )
{
    data = _data;
    psnd = nullptr;
    ptrg = nullptr;

    sbuflen = data->wf.nBlockAlign * samples_in_buf;

    if( data->wf.nChannels == 2 )
    {
        processing_type = _processing_type;
    }
    else
    {
        processing_type = 0;
    }

    if( ( processing_type == 'r' ) || ( processing_type == 'l' ) )
    {
        tbuflen = ceil( sqrt( maxd2 ) / ssp ) + 1;
        psnd = new MovementRecorder( tbuflen, true );
        ptrg = new MovementRecorder( tbuflen, true );
        tbuflen = sbuflen * 2;
    }
    else if( processing_type == '3' )
    {
        tbuflen = sbuflen;
    }
    else
    {
        tbuflen = 0;
        processing_type = 0;
    }

    if( tbuflen > 0 )
    {
        wh[0].lpData = new char[tbuflen];
        wh[1].lpData = new char[tbuflen];
    }
    else
    {
        wh[0].lpData = nullptr;
        wh[1].lpData = nullptr;
    }

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

    if( tbuflen > 0 )
    {
        delete[] wh[0].lpData;
        delete[] wh[1].lpData;
    }

    if( psnd )
        delete psnd;

    if( ptrg )
        delete ptrg;
}

void Sound::Play( double _stime )
{
    // Trying to play sound, that is already playing
    makeException( !IsPlaying() );

    shift = 0;
    bufid = 0;
    stime = _stime;
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
