#pragma once
#include <string>
#include <cstdint>

class MMapFile {
public:
    MMapFile();
    ~MMapFile();
    bool init( const std::string& name, uint32_t size );
    void* data() const { return _ptr; }
    uint32_t size() const { return _mapsize; }
    static bool unlink( const std::string& name );
private:
    int _fid;
    void* _ptr;
    uint32_t _mapsize;
};
