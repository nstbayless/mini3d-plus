#include "pattern.h"

#if ENABLE_CUSTOM_PATTERNS
static inline
uint32_t* Pattern_refcount(PatternTable* p)
{
    return (uint32_t*)p-1;
}

static inline Pattern_free(PatternTable* p)
{
    if (p != &patterns)
    {
        m3d_free(Pattern_refcount(p));
    }
}

PatternTable* Pattern_new()
{
    void* v = m3d_malloc(sizeof(uint32_t) + sizeof(PatternTable));
    *(uint32_t*)v = 1;
    return v + sizeof(uint32_t);
}

PatternTable* Pattern_ref(PatternTable* p)
{
    if (p != &patterns)
    {
        (*Pattern_refcount(p))++;
    }
}

void Pattern_unref(PatternTable* p)
{
    if (p != &patterns)
    {
        if (--(*Pattern_refcount(p)) == 0)
        {
            Pattern_free(p);
        }
    }
}
#endif