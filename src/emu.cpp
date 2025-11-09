
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include "emu.h"
using namespace std;

// Map files into memory for carts bigger than physical RAM
// Handly for NES/SMS carts
// Uses app1 as a cache with a crappy FS on top - default arduino config gives 1280k

#ifdef ESP_PLATFORM
#include <esp_spi_flash.h>
#include <esp_attr.h>
#include <esp_partition.h>
#include <SD.h>
#ifndef MINIZ_NO_STDIO
#define MINIZ_NO_STDIO 1
#endif
#include "miniz.h"

// only map 1 file at a time
spi_flash_mmap_handle_t _file_handle = 0;

static void print_part(const esp_partition_t *pPart)
{
    printf("main: partition type = %d.\n", pPart->type);
    printf("main: partition subtype = %d.\n", pPart->subtype);
    printf("main: partition starting address = %x.\n", pPart->address);
    printf("main: partition size = %x.\n", pPart->size);
    printf("main: partition label = %s.\n", pPart->label);
    printf("main: partition encrypted = %d.\n", pPart->encrypted);
    printf("\n");
}

static void print_parts(esp_partition_iterator_t it)
{
    while (it)
    {
        print_part((esp_partition_t *) esp_partition_get(it));
        it = esp_partition_next(it);
    }
}

typedef struct {
    uint32_t sig;
    uint32_t offset;
    uint32_t len;
    uint32_t flags;
    char name[128-16];
} FlashFile;

#define FSIG ('F' | ('I' << 8) | ('L' << 16) | ('E' << 24))

class CrapFS {
public:
    #define DIR_BLOCK_SIZE 0x4000   // 16k or 128 entries
    // skip 64K at start of partition

    const esp_partition_t* _part;
    uint8_t *_buf;
    FlashFile* _dir;
    int _count;

    CrapFS() : _buf(0),_count(0)
    {
        _part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "app1");
        if (!_part) {
            printf("CrapFS::CrapFS app1 not found\n");
            return;
        }
        print_part(_part);

        _buf = new uint8_t[DIR_BLOCK_SIZE];
        if (!_buf)
            return;
        _dir = (FlashFile*)_buf;
        esp_err_t err = esp_partition_read(_part, 0, _buf, DIR_BLOCK_SIZE);
        if (err) {
            printf("CrapFS::CrapFS dir read failed %d\n",err);
            return;
        }

        // init directory if required
        _count = DIR_BLOCK_SIZE/sizeof(FlashFile);
        if (_dir[0].sig != FSIG)
            reformat();

        // dump dir
        for (int i = 0; i < _count; i++) {
            if (_dir[i].sig == FSIG)
                printf("%08X %08X %s\n",_dir[i].offset,_dir[i].len,_dir[i].name);
        }
    }

    ~CrapFS()
    {
        delete _buf;
    }

    uint32_t align(uint32_t n)
    {
        return (n + 0xFFFF) & 0xFFFF0000;
    }

    void reformat()
    {
        esp_partition_erase_range(_part,0,_part->size); // erase entire partition
        memset(_buf,0xFF,sizeof(DIR_BLOCK_SIZE));       // only use the first 16k of first block
    }

    uint8_t* mmap(const FlashFile* file)
    {
        if (!file)
            return 0;
        printf("CrapFS::mmap mapping %s offset:%08X len:%d\n",file->name,file->offset,file->len);
        void* data = 0;
        if (esp_partition_mmap(_part, file->offset, file->len, SPI_FLASH_MMAP_DATA, (const void**)&data, &_file_handle) == 0)
        {
            printf("CrapFS::mmap mapped to %08X\n",data);
            return (uint8_t*)data;
        }
        return 0;
    }

    // see if the file exists
    FlashFile* find(const std::string& path)
    {
        for (int i = 0; i < _count; i++) {
            if (_dir[i].sig == FSIG && strcmp(_dir[i].name,path.c_str()) == 0)
                return _dir + i;
        }
        return NULL;
    }

    int copy(const std::string& path, int offset, int len)
    {
        FILE *f = fopen(path.c_str(), "rb");
        File sf;
        bool use_sd = false;
        if (!f) {
            // Fallback to Arduino SD
            sf = SD.open(path.c_str(), FILE_READ);
            if (!sf)
                return -1;
            use_sd = true;
        }
        #define BUF_SIZE 4096
        uint8_t* buf = new uint8_t[BUF_SIZE];
        esp_err_t err;
        int i = 0;
        while (i < len) {
            int n = len-i;
            if (n > BUF_SIZE)
                n = BUF_SIZE;
            if (use_sd) {
                int r = sf.read(buf, n);
                if (r <= 0) { err = ESP_FAIL; break; }
            } else {
                size_t r = fread(buf,1,n,f);
                if ((int)r != n) { err = ESP_FAIL; break; }
            }
            printf("CrapFS::copy writing %d of %d\n",i,len);
            err = esp_partition_write(_part, i + offset, buf, n);
            if (err)
                break;
            i += n;
        }
        if (use_sd) sf.close(); else fclose(f);
        delete buf;
        return err;
    }

    //
    FlashFile* create(const std::string& path, int len)
    {
        uint32_t start = 0x10000;
        for (int i = 0; i < _count; i++) {
            if (_dir[i].sig != FSIG) {
                _dir[i].sig = FSIG;
                _dir[i].offset = start;
                _dir[i].len = len;  //
                strcpy(_dir[i].name,path.c_str());
                printf("CrapFS::created %s %08X %d\n",_dir[i].name,start,len);
                esp_err_t err = esp_partition_erase_range(_part,0, DIR_BLOCK_SIZE);     // erase dir
                if (err == 0)
                    err = esp_partition_write(_part, 0, _buf, DIR_BLOCK_SIZE);    // update dir
                if (err) {
                    printf("CrapFS::create dir write failed %d\n",err);
                    return NULL;
                }
                if (copy(path,start,len))
                    return NULL;
                return _dir+i;
            }
            start = align(_dir[i].offset + _dir[i].len);
        }
        // create failed. might want to invoke nuclear option
        return NULL;
    }
};

uint8_t* map_file(const char* path, int len)
{
    // SD-only build: avoid flash mapping/caching. Load into RAM.
    uint8_t* d = nullptr;
    int l = len;
    if (Emu::load(std::string(path), &d, &l) == 0)
        return d;
    return nullptr;
}

void unmap_file(uint8_t* ptr)
{
    // SD-only path: free the RAM buffer allocated in map_file/load
    if (ptr)
        free(ptr);
}

FILE* mkfile(const char* path)
{
    return fopen(path,"wb");
}

#else
#include <sys/stat.h>
#ifndef MINIZ_NO_STDIO
#define MINIZ_NO_STDIO 1
#endif
#include "../miniz.h"

uint8_t* map_file(const char* path, int len)
{
    uint8_t* d;
    Emu::load(path,&d,&len);
    return d;
}

void unmap_file(uint8_t* ptr)
{
    delete ptr;
}

FILE* mkfile(const char* path)
{
    std::string v = path;
    std::string dir = v.substr(0,v.find_last_of("/"));
    mkdir(dir.c_str(), 0755);
    return fopen(path,"wb");
}

#endif


// unpack file and write to FS, use rom miniz on esp32
// uses quite a lot of memory, call before initializing screen on atari
// could actually use the screen mem (which might look cool) or the main cpu mem for buffer
int unpack(const char* dst, const uint8_t* d, int len)
{
    printf("unpacking %s\n",dst);
    FILE* f = mkfile(dst);
    if (!f)
        return -1;

    #define BUF_SIZE 0x8000
    uint8_t* buf = new uint8_t[BUF_SIZE];
    if (!buf) {
        fclose(f);
        return -1;  // could use a smaller window on compression but would not generalize to other people's zips
    }

    tinfl_decompressor* dec = new tinfl_decompressor;   // largist
    size_t in_bytes, out_bytes;
    tinfl_status status;
    int i = 0;

    tinfl_init(dec);
    while (i < len) {
        in_bytes = len-i;
        out_bytes = BUF_SIZE;
        status = tinfl_decompress(dec,d+i,&in_bytes,buf,buf,&out_bytes,11);
        if (out_bytes != fwrite(buf,1,out_bytes,f)) {
            status = TINFL_STATUS_FAILED;
            break;
        }
        i += in_bytes;
    }

    delete [] buf;
    delete dec;
    fclose(f);

    if (status == TINFL_STATUS_FAILED) {
        remove(dst);
        return -1;
    }
    return 0;
}

Emu::Emu(const char* n,int w,int h, int st, int aformat, int cc, int f) :
    name(n),width(w),height(h),standard(st),audio_format(aformat),cc_width(cc),flavor(f)
{
    //audio_frequency = 15625; // requires fixed point sampler
    audio_frequency = standard == 1 ? 15720 : 15600;
    audio_frame_samples = standard ? (audio_frequency << 16)/60 : (audio_frequency << 16)/50;   // fixed point sampler
    audio_fraction = 0;
}

Emu::~Emu()
{
}

int Emu::frame_sample_count()
{
    int n = audio_frame_samples + audio_fraction;
    audio_fraction = n & 0xFFFF;
    return n >> 16;
}

int Emu::insert(const std::string& path, int flags, int disk_index)
{
    return -1;
}

const uint32_t* Emu::composite_palette()
{
    return standard ? ntsc_palette() : pal_palette();
}

// determine file type
int Emu::head(const std::string& path, uint8_t* data, int len)
{
    FILE *f = fopen(path.c_str() , "rb");
    if (f) {
        printf("Emu::head fopen OK for %s\n", path.c_str());
        fread(data,1,len,f);
        fseek(f, 0, SEEK_END);
        int flen =(int)ftell(f);
        fclose(f);
        return flen;
    }
    // Fallback to Arduino SD
    printf("Emu::head fopen failed, trying SD for %s\n", path.c_str());
    File sf = SD.open(path.c_str(), FILE_READ);
    if (!sf) return -1;
    int out = 0;
    while (sf.available() && out < len) {
        int r = sf.read(data + out, len - out);
        if (r <= 0) break;
        out += r;
    }
    int flen = (int)sf.size();
    sf.close();
    return flen;
}

int Emu::load(const std::string& path, uint8_t** data, int* len)
{
    *data = 0;
    *len = 0;
    FILE *f = fopen(path.c_str(), "rb");
    if (f) {
        printf("Emu::load fopen OK for %s\n", path.c_str());
        fseek(f, 0, SEEK_END);
        int fsize = (int)ftell(f);
        fseek(f, 0, SEEK_SET);
        printf("Emu::load: fopen size=%d for %s\n", fsize, path.c_str());
        uint8_t* d = (uint8_t*)malloc(fsize);
        if (!d) {
            printf("Emu::load failed for %s (out of memory malloc %d)\n",path.c_str(), fsize);
            fclose(f);
            return -1;
        }
        size_t rd = fread(d, 1, fsize, f);
        fclose(f);
        if ((int)rd != fsize) {
            printf("Emu::load fread short: %d/%d for %s\n", (int)rd, fsize, path.c_str());
            free(d);
            return -1;
        }
        printf("Emu::load %d bytes %s\n",fsize,path.c_str());
        *data = d;
        *len = fsize;
        return 0;
    }
    // Fallback to Arduino SD
    printf("Emu::load fopen failed, trying SD for %s\n", path.c_str());
    File sf = SD.open(path.c_str(), FILE_READ);
    if (!sf) {
        printf("Emu::load failed for %s (not found)\n",path.c_str());
        return -1;
    }
    int fsize = (int)sf.size();
    printf("Emu::load: SD size=%d for %s\n", fsize, path.c_str());
    if (fsize <= 0 || fsize > (2*1024*1024)) {
        printf("Emu::load size invalid: %d for %s\n", fsize, path.c_str());
        sf.close();
        return -1;
    }
    uint8_t* d = (uint8_t*)malloc(fsize);
    if (!d) {
        printf("Emu::load failed for %s (out of memory malloc %d)\n",path.c_str(), fsize);
        sf.close();
        return -1;
    }
    int off = 0;
    while (off < fsize) {
        int r = sf.read(d + off, fsize - off);
        if (r <= 0) break;
        off += r;
    }
    sf.close();
    if (off != fsize) {
        printf("Emu::load SD read short: %d/%d for %s\n", off, fsize, path.c_str());
        free(d);
        return -1;
    }
    printf("Emu::load %d bytes %s (SD)\n",fsize,path.c_str());
    *data = d;
    *len = fsize;
    return 0;
}
