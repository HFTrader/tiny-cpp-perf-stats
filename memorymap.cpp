/**
 * Copyright Vitorian LLC (2020)
 *
 * This application will test if spinlocks are ok to be used in a shared memory configuration
 * and what is the best solution
 *
 */
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <stdint.h>
#include <xmmintrin.h>

#include <string>
#include <atomic>

#include "Snapshot.h"

constexpr bool DEBUG = false;

class BryceSpinLock
{
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() noexcept {
        for ( std::uint64_t k=0; flag.test_and_set(std::memory_order_acquire); ++k ) {
            if ( k<4 );
            else if ( k<16 ) __asm__ __volatile__( "rep;nop":::"memory" );
            else if ( k<64 ) sched_yield();
            else {
                timespec rqtp = {0,0};
                rqtp.tv_sec = 0; rqtp.tv_nsec = 1000;
                nanosleep( &rqtp, nullptr );
                }
        }
    }
    bool try_lock() noexcept {
        return flag.test_and_set(std::memory_order_acquire);
    }
    void unlock() noexcept {
        flag.clear( std::memory_order_release );
    }
    bool locked() noexcept {
        bool locked = flag.test_and_set(std::memory_order_acquire);
        if ( !locked ) {
            flag.clear( std::memory_order_release );
            return false;
        }
        return true;
    }
};

class TraditionalSpinLock
{
public:
    TraditionalSpinLock() { __sync_lock_release(&val); }
    ~TraditionalSpinLock() {}

    void lock() noexcept {
        while ( !try_lock() ) {
            __asm__ __volatile__( "rep; nop;" ::: "memory" );
        }

    }
    bool try_lock() noexcept {
        return __sync_lock_test_and_set( &val, 1 )==0;
    }
    void unlock() noexcept {
        __sync_synchronize();
        __sync_lock_release(&val);
    }
    bool locked() noexcept {
        return val != 0;
    }
private:
    volatile uint32_t val;
};

class RelaxedSpinLock
{
public:
    RelaxedSpinLock() : lock_(0) {}
    ~RelaxedSpinLock() {}

    void lock() noexcept {
        for (;;) {
            if ( !lock_.exchange(true, std::memory_order_acquire) ) {
                return;
            }
            while ( lock_.load(std::memory_order_relaxed) ) {
                for ( uint32_t j=0; j<10; ++j ) __builtin_ia32_pause();
                asm volatile("pause\n": : :"memory");
            }
        }
    }
    bool try_lock() noexcept {
        return !lock_.load( std::memory_order_relaxed) &&
            !lock_.exchange( true, std::memory_order_acquire);
    }
    void unlock() noexcept {
        lock_.store( false, std::memory_order_release );
    }
    bool locked() noexcept {
        return lock_.load(std::memory_order_relaxed);
    }
private:
    std::atomic<bool> lock_;
};



template< class SpinLock >
int testSL( Snapshot& snap, uint32_t numloops, uint32_t offset )
{
    // Create path
    char shmname[PATH_MAX];
    ::sprintf( shmname, "mmtest-%d", ::getpid() );
    int fid = ::shm_open( shmname, O_CREAT|O_RDWR, S_IRWXU|S_IRWXG );
    if ( fid<0 ) {
        int err_no = errno;
        fprintf( stderr, "Cannot open file [%s]: %s\n",
                 shmname, strerror(err_no) );
        return 1;
    }

    const uint32_t pagesize = getpagesize();
    const uint32_t mapsize = 16*pagesize;
    int res = ::ftruncate( fid, mapsize );
    if ( res<0 ) {
        int err_no = errno;
        fprintf( stderr, "Could not truncate shared mem file %s: %s\n",
                 shmname, strerror( err_no ) );
        ::close( fid );
        return false;
    }

    // Memory map file
    void* ptr = mmap( NULL, mapsize, PROT_READ|PROT_WRITE, MAP_SHARED, fid, 0 );
    if ( ptr == MAP_FAILED ) {
        int err_no = errno;
        fprintf( stderr, "Cannot memory map file: %s\n", strerror(err_no) );
        return 1;
    }
    ::memset( ptr, 0, mapsize );

    // Fork child
    uint32_t thcount = 0;
    pid_t pid = fork();
    bool isparent = ( pid!=0 );
    if ( !isparent ) thcount++;
    const uint32_t MASK = 1;
    const uint32_t PROCID = isparent ? 0 : 1;
    const char* prname = !isparent ? "Child" : "Parent";
    if ( DEBUG )  fprintf( stderr, "[%s] Memory: %p  %d\n", prname, ptr, *((uint32_t*)ptr) );
    if ( DEBUG ) fprintf( stderr, "[%s] Test starting...\n", prname );

    // Start test
    volatile uint32_t* baseptr = (volatile uint32_t*)ptr;
    SpinLock* lock = new((void*)&baseptr[0])SpinLock;
    volatile uint32_t* counter = &baseptr[offset];

    if ( DEBUG ) fprintf( stderr, "[%s] Status: [%s]\n", prname, lock->locked()? "Locked":"Unlocked" );
    if ( DEBUG ) fprintf( stderr, "[%s] Starting \n", prname );
    uint32_t value = 0;
    snap.start();
    uint64_t t0 = __builtin_ia32_rdtsc();
    while ( value < numloops ) {
        if ( DEBUG ) fprintf( stderr, "[%s] Locking...\n", prname );
        lock->lock();
        if ( DEBUG ) fprintf( stderr, "[%s] Locked...%d\n", prname, *counter );
        if ( (*counter & MASK) == PROCID ) {
            *counter += 1;
            if ( DEBUG ) fprintf( stderr, "[%s] Incrementing counter to %d\n", prname, *counter );
        }
        value = *counter;
        if ( DEBUG ) fprintf( stderr, "[%s] Unlocking...\n", prname );
        lock->unlock();
        if ( DEBUG ) fprintf( stderr, "[%s] Unlocked...\n", prname );
    }
    uint64_t t1 = __builtin_ia32_rdtsc();
    snap.stop( "Loop", numloops, offset );
    fprintf( stderr, "[%s] Finished %ld cycles, %ld per loop\n", prname, t1-t0, (t1-t0)/numloops );
    // Close and remove
    ::close(fid);
    if ( isparent ) {
        int status = 0;
        ::waitpid(pid, &status, 0 );
        usleep( 1000000 );
        shm_unlink( shmname );
    }
    else {
        exit(0);
    }
}




int main( int argc, char* argv[] )
{
    if ( argc<2 ) {
        fprintf( stdout, "Usage: test <num> [numloops]\n" );
        return 0;
    }
    uint32_t testnum = ::atoi( argv[1] );
    uint32_t maxloops = 16384;
    if ( argc>2 ) {
        maxloops = ::atoi( argv[2] );
    }

    Snapshot snap;
    for ( uint32_t numloops = 1024; numloops< maxloops; numloops *= 2 )
    {
        for ( uint32_t offset = 1; offset < 1024; offset *= 2 ) {
            switch ( testnum ) {
            case 0:
                testSL<RelaxedSpinLock>( snap, numloops, offset );
                break;
            case 1:
                testSL<BryceSpinLock>( snap, numloops, offset );
                break;
            case 2:
                testSL<TraditionalSpinLock>( snap, numloops, offset );
                break;
            };
        }
    }
}
