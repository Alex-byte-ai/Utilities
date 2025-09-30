#include "Connect.h"

#include <windows.h>

#include "Exception.h"
#include "Lambda.h"
#include "Basic.h"

class Connect::Data
{
public:
    // Buffers for reading, writing and positions to read and write from are stored in shared memory
    uint8_t *inputPos, *outputPos;    // Positions of current message for input and output in buffers for THIS data instance
    uint8_t *complimentary;           // Position of current message to write for buffer it inputs from
    uint8_t *inputBegin, *inputEnd;   // Pointers to begin and end of the input buffer
    uint8_t *outputBegin, *outputEnd; // Pointers to begin and end of the output buffer
    void *sharedMemory;               // Pointer to the mapped memory
    HANDLE mapFile;                   // Handle to the shared memory
    HANDLE mutex;                     // Handle to the named mutex
    bool server;                      // Indicates if this instance is the creator of common memory
    size_t size;                      // Size of sharedMemory

    static void calculate( size_t bufferSize, size_t &half )
    {
        if( bufferSize < 1024 )
            bufferSize = 1024;

        auto mod = sizeof( size_t );
        auto remainder = bufferSize % mod;
        if( remainder != 0 )
            bufferSize += mod - remainder;

        half = bufferSize + 2 * sizeof( size_t );
    }

    static bool calculate( size_t size, size_t &half, size_t &bufferSize )
    {
        if( size % 2 != 0 )
            return false;
        half = size / 2;
        if( half < 1024 + 2 * sizeof( size_t ) )
            return false;
        bufferSize = half - 2 * sizeof( size_t );
        return true;
    }

    Data( const std::wstring &id, size_t bufferSize )
    {
        size_t half;

        // Create or open a named mutex for synchronization
        mutex = CreateMutexW( nullptr, FALSE, ( id + L"_mutex" ).c_str() );
        makeException( mutex );
        // Mutex might already exist, but point of CreateMutexW was to open existing one in that case
        SetLastError( ERROR_SUCCESS );

        auto name = id + L"_file";

        // Check if the shared memory already exists
        mapFile = OpenFileMappingW( FILE_MAP_ALL_ACCESS, FALSE, name.c_str() );
        if( mapFile )
        {
            server = false;
        }
        else if( GetLastError() == ERROR_FILE_NOT_FOUND )
        {
            calculate( bufferSize, half );
            size = 2 * half;

            // Create the shared memory as the server
            mapFile = CreateFileMappingW( INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, name.c_str() );
            makeException( mapFile );
            server = true;
            SetLastError( ERROR_SUCCESS ); // Error, that happened in OpenFileMappingW was properly processed here
        }
        else
        {
            makeException( false );
        }

        // Map the shared memory
        sharedMemory = MapViewOfFile( mapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0 );
        makeException( sharedMemory );

        // Query the memory region information
        MEMORY_BASIC_INFORMATION memoryInfo;
        makeException( VirtualQuery( sharedMemory, &memoryInfo, sizeof( memoryInfo ) ) );
        size = memoryInfo.RegionSize;
        makeException( calculate( size, half, bufferSize ) );

        if( server )
            memset( sharedMemory, 0, size );

        auto begin = ( uint8_t * )sharedMemory;
        auto end = begin + half;

        auto pos0 = begin;
        begin += sizeof( size_t );
        auto pos1 = begin;
        begin += sizeof( size_t );

        if( server )
        {
            inputPos = pos0;
            complimentary = pos1;
            inputBegin = begin;
            inputEnd = end;
        }
        else
        {
            outputPos = pos1;
            outputBegin = begin;
            outputEnd = end;
        }

        begin += half;
        end += half;
        pos0 += half;
        pos1 += half;

        if( server )
        {
            outputPos = pos1;
            outputBegin = begin;
            outputEnd = end;
        }
        else
        {
            inputPos = pos0;
            complimentary = pos1;
            inputBegin = begin;
            inputEnd = end;
        }
    }

    ~Data()
    {
        if( sharedMemory )
            UnmapViewOfFile( sharedMemory );
        if( mapFile )
            CloseHandle( mapFile );
        if( mutex )
            CloseHandle( mutex );
    }
};

Connect::Connect( const std::wstring &name, size_t bufferSize )
{
    data = new Data( name, bufferSize );
}

Connect::~Connect()
{
    delete data;
}

bool Connect::input( Message &message ) const
{
    // Lock the shared memory with the mutex, unlock it when finished.
    WaitForSingleObject( data->mutex, INFINITE );
    Finalizer _;
    _.push( [this]()
    {
        ReleaseMutex( data->mutex );
    } );

    size_t pos;
    auto begin = data->inputBegin;
    auto end = data->inputEnd;
    copy( &pos, data->inputPos, sizeof( pos ) );

    uint8_t *current = begin;
    current += pos;

    size_t size;
    if( current + sizeof( size ) > end )
        return false; // Check for buffer overflow

    copy( &size, current, sizeof( size ) );
    current += sizeof( size );

    if( size == 0 )
        return false; // No data available
    --size; // Always storing one more than size, to differentiate between zero size objects and empty areas of the buffer

    if( current + size > end )
        return false; // Check for buffer overflow

    std::vector<uint8_t> buffer( current, current + size );
    current += size;

    if( !message.input( buffer ) )
        return false;

    // Moving forward in buffer
    pos = current - begin;

    // Everything, that was written was read, going to the beginning of the buffer and cleaning stuff, that was written
    // It contains nothing meaningful, let's set it up to initial state to avoid overflow
    if( compare( &pos, data->complimentary, sizeof( pos ) ) )
    {
        pos = 0;
        memset( data->complimentary, 0, sizeof( size_t ) );
        memset( begin, 0, current - begin );
    }

    copy( data->inputPos, &pos, sizeof( pos ) );
    return true;
}

bool Connect::output( const Message &message ) const
{
    // Lock the shared memory with the mutex, unlock it when finished.
    WaitForSingleObject( data->mutex, INFINITE );
    Finalizer _;
    _.push( [this]()
    {
        ReleaseMutex( data->mutex );
    } );

    size_t pos;
    auto begin = data->outputBegin;
    auto end = data->outputEnd;
    copy( &pos, data->outputPos, sizeof( pos ) );

    uint8_t *current = begin;
    current += pos;

    size_t size = 0;
    std::vector<uint8_t> buffer;
    if( !message.output( buffer ) )
        return false;

    size = buffer.size();
    if( current + sizeof( size ) + size > end )
        return false; // Check for buffer overflow

    size_t encodedSize = size + 1;
    copy( current, &encodedSize, sizeof( encodedSize ) );
    current += sizeof( encodedSize );

    copy( current, buffer.data(), size );
    current += size;

    // Moving forward in buffer
    pos = current - begin;

    copy( data->outputPos, &pos, sizeof( pos ) );
    return true;
}

bool Connect::isServer() const
{
    return data->server;
}
