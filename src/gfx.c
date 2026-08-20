#include "gfx.h"

#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_WRITE
#include "stb_image.h"

bool gfx_init(Gfx *g, const char *title, int scale)
{
    SDL_memset(g, 0, sizeof(*g));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    /* Point sampling: this is pixel art, we never want it blurred. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    g->window = SDL_CreateWindow(title,
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 GAME_W * scale, GAME_H * scale,
                                 SDL_WINDOW_RESIZABLE);
    if (!g->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    g->renderer = SDL_CreateRenderer(g->window, -1,
                                     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    /* Draw in 224x288 units and let SDL letterbox the result.
       Integer scaling used to be on, because with raster sprites a fractional
       zoom made some rows of pixels taller than others. The artwork is
       geometry now, so it can be drawn at whatever size the window actually
       is: the logical size keeps the game's coordinates - and therefore its
       paths, speeds and collision radii - in the same 224x288 units, while the
       picture itself is resolution independent. */
    SDL_RenderSetLogicalSize(g->renderer, GAME_W, GAME_H);
    SDL_RenderSetIntegerScale(g->renderer, SDL_FALSE);

    return true;
}

void gfx_shutdown(Gfx *g)
{
    gfx_free_texture(&g->sheet);
    if (g->renderer) SDL_DestroyRenderer(g->renderer);
    if (g->window)   SDL_DestroyWindow(g->window);
    g->renderer = NULL;
    g->window   = NULL;
    SDL_Quit();
}

bool gfx_load_texture(Gfx *g, Texture *out, const char *path, bool colorkey_black)
{
    int w, h, channels;
    stbi_uc *pixels = stbi_load(path, &w, &h, &channels, 4); /* force RGBA */
    if (!pixels) {
        fprintf(stderr, "failed to load '%s': %s\n", path, stbi_failure_reason());
        return false;
    }

    /* The rip stores the transparent background as opaque black, matching the
       hardware's palette entry 0. Punch it out so sprites composite properly. */
    if (colorkey_black) {
        for (int i = 0; i < w * h; ++i) {
            stbi_uc *p = &pixels[i * 4];
            if (p[0] == 0 && p[1] == 0 && p[2] == 0) p[3] = 0;
        }
    }

    SDL_Texture *tex = SDL_CreateTexture(g->renderer, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STATIC, w, h);
    if (!tex) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        stbi_image_free(pixels);
        return false;
    }

    SDL_UpdateTexture(tex, NULL, pixels, w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    stbi_image_free(pixels);

    out->tex = tex;
    out->w   = w;
    out->h   = h;
    return true;
}

void gfx_free_texture(Texture *t)
{
    if (t->tex) SDL_DestroyTexture(t->tex);
    t->tex = NULL;
    t->w = t->h = 0;
}

void gfx_blit(Gfx *g, const SDL_Rect *src, int x, int y)
{
    SDL_Rect dst = { x, y, src->w, src->h };
    SDL_RenderCopy(g->renderer, g->sheet.tex, src, &dst);
}

void gfx_blit_flip(Gfx *g, const SDL_Rect *src, int x, int y, SDL_RendererFlip flip)
{
    SDL_Rect dst = { x, y, src->w, src->h };
    SDL_RenderCopyEx(g->renderer, g->sheet.tex, src, &dst, 0.0, NULL, flip);
}

void gfx_blit_rot(Gfx *g, const SDL_Rect *src, int x, int y, double angle)
{
    SDL_Rect dst = { x, y, src->w, src->h };
    SDL_RenderCopyEx(g->renderer, g->sheet.tex, src, &dst, angle, NULL, SDL_FLIP_NONE);
}

bool gfx_screenshot(Gfx *g, const char *path)
{
    /* A NULL rect makes SDL read back the current viewport, which under
       logical scaling is exactly the 224x288 picture blown up to its on-screen
       size - the letterbox bars are left out for free. */
    int w, h;
    if (SDL_GetRendererOutputSize(g->renderer, &w, &h) != 0) {
        fprintf(stderr, "SDL_GetRendererOutputSize failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32,
                                                       SDL_PIXELFORMAT_ARGB8888);
    if (!surf) {
        fprintf(stderr, "SDL_CreateRGBSurfaceWithFormat failed: %s\n", SDL_GetError());
        return false;
    }

    bool ok = SDL_RenderReadPixels(g->renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                   surf->pixels, surf->pitch) == 0
           && SDL_SaveBMP(surf, path) == 0;
    if (!ok) fprintf(stderr, "screenshot failed: %s\n", SDL_GetError());

    SDL_FreeSurface(surf);
    return ok;
}

void gfx_begin_frame(Gfx *g)
{
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 255);
    SDL_RenderClear(g->renderer);
}

void gfx_end_frame(Gfx *g)
{
    SDL_RenderPresent(g->renderer);
}
