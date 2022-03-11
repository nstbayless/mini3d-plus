#include "texture.h"
#if ENABLE_TEXTURES

#if ENABLE_TEXTURES_GREYSCALE
#include "image/spng.h"
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wparentheses"
#endif

// checks extension for ending
static int
strendswith(const char* path, const char* ext)
{
    if (!path) return 0;
    size_t lenpath = strlen(path);
    size_t lenext = strlen(ext);
    if (lenpath > lenext) return 0;
    for (size_t i = 0; i < lenext; ++i)
    {
        if (*(path + lenpath - i) != *(ext + lenext - i))
        {
            return 0;
        }
    }
    return 1;
}

Texture* Texture_loadFromPath(const char* path, int greyscale, const char** outerr)
{
    if (greyscale)
    {
        #if ENABLE_TEXTURES_GREYSCALE
        if (strendswith(path, ".pdi"))
        {
            *outerr = "cannot load .pdi file as greyscale texture.";
            return NULL;
        }
        
        // load file
        FileStat fstat;
        if (pd->file->stat(path, &fstat) != 0)
        {
            *outerr = pd->file->geterr();
            // FIXME: is there an easier way?
            if (strcmp(*outerr, "No such file") == 0)
            {
                if (strendswith(path, ".png"))
                {
                    *outerr = "No such file. (Note that .png files are converted into .pdi at build time!)";
                }
            }
            return NULL;
        }
        if (fstat.isdir)
        {
            *outerr = "is a directory, not an image file.";
            return NULL;
        }
        if (fstat.size == 0)
        {
            *outerr = "file is empty";
            return NULL;
        }
        SDFile* file = pd->file->open(path, kFileRead | kFileReadData);
        if (!file)
        {
            *outerr = pd->file->geterr();
            if (strcmp(*outerr, "No such file") == 0)
            {
                if (strendswith(path, ".png"))
                {
                    *outerr = "No such file. (Note that .png files are converted into .pdi at build time!)";
                }
            }
            return NULL;
        }
        uint8_t* fbuff = m3d_malloc(fstat.size);
        if (!fbuff)
        {
            *outerr = "out of memory";
            return NULL;
        }
        
        if (pd->file->read(file, fbuff, fstat.size) < 0)
        {
            m3d_free(fbuff);
            *outerr = pd->file->geterr();
            pd->file->close(file);
            return NULL;
        }
        
        if (pd->file->close(file) != 0)
        {
            m3d_free(fbuff);
            *outerr = pd->file->geterr();
            return NULL;
        }
        
        // interpret png
        size_t len;
        int err;
        struct spng_alloc alloc = {
            m3d_malloc,
            m3d_realloc,
            m3d_calloc,
            m3d_free
        };
        spng_ctx* ctx = spng_ctx_new2(&alloc, 0);
        if (!ctx)
        {
            *outerr = "unable to create spng context";
            m3d_free(fbuff);
            return NULL;
        }
        if (err = spng_set_png_buffer(ctx, fbuff, fstat.size))
        {
            goto spng_err_f;
        }
        if (err = spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &len))
        {
            goto spng_err_f;
        }
        if (len == 0)
        {
            m3d_free(fbuff);
            spng_ctx_free(ctx);
            *outerr = "image has size 0";
            return NULL;
        }
        uint8_t* dbuff = m3d_malloc(len);
        if (!dbuff)
        {
            *outerr = "out of memory";
            m3d_free(dbuff);
            m3d_free(fbuff);
            spng_ctx_free(ctx);
            return NULL;
        }
        if (err = spng_decode_image(ctx, dbuff, len, SPNG_FMT_RGBA8, 0))
        {
        spng_err_fd:
            m3d_free(dbuff);
        spng_err_f:
            m3d_free(fbuff);
        spng_err:
            spng_ctx_free(ctx);
            *outerr = spng_strerror(err);
            return NULL;
        }
        struct spng_ihdr ihdr;
        if (err = spng_get_ihdr(ctx, &ihdr))
        {
            goto spng_err_fd;
        }
        if (ihdr.width == 0 || ihdr.height == 0 || ihdr.width * ihdr.height > len)
        {
            m3d_free(dbuff);
            m3d_free(fbuff);
            spng_ctx_free(ctx);
            *outerr = "image has invalid size";
            return NULL;
        }
        
        spng_ctx_free(ctx);
        m3d_free(fbuff);
        
        void* v = malloc(sizeof(uint32_t) + sizeof(GreyBitmap) + ihdr.width * ihdr.height);
        *(uint32_t*)v = 1;
        GreyBitmap* g = v + sizeof(uint32_t);
        uint8_t* tbuff = v + sizeof(GreyBitmap) + sizeof(uint32_t);
        
        g->width = ihdr.width;
        g->height = ihdr.height;
        g->transparency = 0;
        
        // convert png to greyscale
        for (size_t i = 0; i < ihdr.width * ihdr.height; ++i)
        {
            uint8_t* p = dbuff + i * 4;
            uint8_t* t = tbuff + i;
            *t = ((uint16_t)p[0] + p[1] + p[2]) * (LIGHTING_PATTERN_COUNT - 1) / (
                LIGHTING_PATTERN_COUNT == 33
                ? 0x2e6
                : 0x2fd
            );
            if (LIGHTING_PATTERN_COUNT != 33 && *t >= LIGHTING_PATTERN_COUNT) *t = LIGHTING_PATTERN_COUNT - 1;
            
            // alpha
            if (p[3] < 0x80)
            {
                g->transparency = 1;
            }
            else
            {
                *t |= 0x80;
            }
        }
        m3d_free(dbuff);
        
        return (void*) (((uintptr_t)g) | 1);
        #else
        *outerr = "cannot load greyscale image. Must activate ENABLE_TEXTURES_GREYSCALE.";
        return NULL;
        #endif
    }
    else
    {
        LCDBitmap* bitmap = pd->graphics->loadBitmap(path, outerr);
        if (bitmap)
        {
            void* t = m3d_malloc(sizeof(uint32_t) + sizeof(LCDBitmap*));
            if (!t)
            {
                *outerr = "out of memory";
                return NULL;
            }
            *(uint32_t*)t = 1;
            *(LCDBitmap**)(t + sizeof(uint32_t)) = bitmap;
            return t + sizeof(uint32_t);
        }
        else
        {
            return NULL;
        }
    }
}

static void Texture_free(Texture* t)
{
    #if ENABLE_TEXTURES_GREYSCALE
    if (Texture_isLCDBitmap(t))
    #endif
    {
        pd->graphics->freeBitmap(Texture_getLCDBitmap(t));
    }
    
    m3d_free(Texture_refCount(t));
}

Texture* Texture_ref(Texture* t)
{
    (*Texture_refCount(t))++;
    return t;
}

void Texture_unref(Texture* t)
{
    if (--(*Texture_refCount(t)) == 0)
    {
        Texture_free(t);
    }
}

Texture* Texture_fromLCDBitmap(LCDBitmap* bitmap)
{
    void* t = m3d_malloc(sizeof(uint32_t) + sizeof(LCDBitmap*));
    if (!t)
    {
        return NULL;
    }
    *(uint32_t*)t = 1;
    *(LCDBitmap**)(t + sizeof(uint32_t)) = bitmap;
    return t + sizeof(uint32_t);
}

#endif