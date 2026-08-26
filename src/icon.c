#include "icon.h"
#include "shape.h"

#include <stdio.h>
#include <string.h>

/* How much of the box the fighter fills. Short of the edges, because Windows
   crops nothing and an icon that touches its bounds looks bigger than its
   neighbours rather than better drawn. */
#define ICON_FILL 0.94f

/* The sizes Windows actually asks for. 256 is the one the large-icon views and
   the installer use; 16 is the title bar. */
static const int ICON_SIZES[] = { 16, 24, 32, 48, 64, 128, 256 };

SDL_Surface *icon_render(Gfx *g, int size)
{
    SDL_Texture *target = SDL_CreateTexture(g->renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_TARGET, size, size);
    if (!target) return NULL;

    SDL_Texture *was = SDL_GetRenderTarget(g->renderer);

    /* The renderer normally maps everything through the 224x288 picture. Drawing
       into a square texture means working in its own pixels, so the mapping goes
       away for the duration and is put back afterwards. */
    int lw = 0, lh = 0;
    SDL_RenderGetLogicalSize(g->renderer, &lw, &lh);
    SDL_RenderSetLogicalSize(g->renderer, 0, 0);

    SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(g->renderer, target);
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 0);
    SDL_RenderClear(g->renderer);

    /* The fighter is authored on a 16-unit box facing north, which is the pose
       it should be seen in. */
    Vec2 mid = { size * 0.5f, size * 0.5f };
    shape_draw(g, SHP_FIGHTER, mid, HEADING_N, size * ICON_FILL / 16.0f);

    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32,
                                                      SDL_PIXELFORMAT_ARGB8888);
    if (out) {
        SDL_RenderReadPixels(g->renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             out->pixels, out->pitch);
    }

    SDL_SetRenderTarget(g->renderer, was);
    SDL_RenderSetLogicalSize(g->renderer, lw, lh);
    SDL_DestroyTexture(target);
    return out;
}

static void put32(FILE *f, unsigned v)
{
    fputc((int)( v        & 0xFF), f);
    fputc((int)((v >>  8) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 24) & 0xFF), f);
}

static void put16(FILE *f, unsigned v)
{
    fputc((int)( v       & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
}

/* One image, as a DIB: a header claiming twice the height, the pixels bottom
   up, then the 1bpp mask that height belongs to. A 32-bit icon is keyed off its
   alpha and the mask is left clear, but it has to be there and it has to be the
   right size or the icon is silently rejected. */
static unsigned dib_size(int size)
{
    unsigned mask_row = (unsigned)(((size + 31) / 32) * 4);
    return 40u + (unsigned)(size * size * 4) + mask_row * (unsigned)size;
}

static void write_dib(FILE *f, SDL_Surface *s)
{
    int size = s->w;

    put32(f, 40);
    put32(f, (unsigned)size);
    put32(f, (unsigned)(size * 2));
    put16(f, 1);
    put16(f, 32);
    put32(f, 0);
    put32(f, dib_size(size) - 40u);
    put32(f, 0); put32(f, 0); put32(f, 0); put32(f, 0);

    for (int y = size - 1; y >= 0; --y) {
        const Uint8 *row = (const Uint8 *)s->pixels + (size_t)y * (size_t)s->pitch;
        fwrite(row, 1, (size_t)size * 4, f);   /* ARGB8888 is BGRA in memory */
    }

    unsigned mask_row = (unsigned)(((size + 31) / 32) * 4);
    for (int y = 0; y < size; ++y) {
        for (unsigned b = 0; b < mask_row; ++b) fputc(0, f);
    }
}

bool icon_write(Gfx *g, const char *path)
{
    const int n = (int)ARRAY_COUNT(ICON_SIZES);
    SDL_Surface *img[ARRAY_COUNT(ICON_SIZES)];
    int made = 0;

    for (int i = 0; i < n; ++i) {
        img[i] = icon_render(g, ICON_SIZES[i]);
        if (!img[i]) break;
        ++made;
    }
    if (made != n) {
        for (int i = 0; i < made; ++i) SDL_FreeSurface(img[i]);
        fprintf(stderr, "icon: could not render (%s)\n", SDL_GetError());
        return false;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        for (int i = 0; i < n; ++i) SDL_FreeSurface(img[i]);
        fprintf(stderr, "icon: cannot write %s\n", path);
        return false;
    }

    put16(f, 0);
    put16(f, 1);
    put16(f, (unsigned)n);

    unsigned offset = 6u + 16u * (unsigned)n;
    for (int i = 0; i < n; ++i) {
        int size = ICON_SIZES[i];
        fputc(size >= 256 ? 0 : size, f);      /* 256 is written as zero */
        fputc(size >= 256 ? 0 : size, f);
        fputc(0, f);
        fputc(0, f);
        put16(f, 1);
        put16(f, 32);
        put32(f, dib_size(size));
        put32(f, offset);
        offset += dib_size(size);
    }

    for (int i = 0; i < n; ++i) write_dib(f, img[i]);

    fclose(f);
    for (int i = 0; i < n; ++i) SDL_FreeSurface(img[i]);

    printf("wrote %s (%d sizes, %u bytes)\n", path, n, offset);
    return true;
}
