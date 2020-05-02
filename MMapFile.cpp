#include "MMapFile.h"
#include <sys/mman.h>
#include <cstdint>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

MMapFile::MMapFile()
    : _fid(-1), _ptr(nullptr), _mapsize(0)
{}

MMapFile::~MMapFile() {
    if (_fid>=0) {
        // Close and remove
        ::munmap( _ptr, _mapsize );
        ::close(_fid);
    }
}

bool MMapFile::init( const std::string& name, uint32_t size )
{
    int fid = ::shm_open( name.c_str(), O_CREAT|O_RDWR, S_IRWXU|S_IRWXG );
    if ( fid<0 ) {
        int err_no = errno;
        fprintf( stderr, "Cannot open file [%s]: %s\n",
                 name.c_str(), strerror(err_no) );
        return false;
    }

    const uint32_t pagesize = getpagesize();
    const uint32_t mapsize = size==0 ? pagesize :
        ((size-1)/pagesize+1)*pagesize;
    int res = ::ftruncate( fid, mapsize );
    if ( res<0 ) {
        int err_no = errno;
        fprintf( stderr, "Could not truncate shared mem file %s: %s\n",
                 name.c_str(), strerror( err_no ) );
        ::close( fid );
        return false;
    }

    // Memory map file
    void* ptr = mmap( NULL, mapsize, PROT_READ|PROT_WRITE,
                      MAP_SHARED|MAP_LOCKED, fid, 0 );
    if ( ptr == MAP_FAILED ) {
        int err_no = errno;
        fprintf( stderr, "Cannot memory map file: %s\n", strerror(err_no) );
        return false;
    }
    ::memset( ptr, 0, mapsize );

    _fid = fid;
    _ptr = ptr;
    _mapsize = mapsize;
    return true;
}

bool MMapFile::unlink( const std::string& name )
{
    int res = ::shm_unlink( name.c_str() );
    return res==0;
}
