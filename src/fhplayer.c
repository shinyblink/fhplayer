#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <farbherd.h>
#include <endian.h>

#include "fhstream.h"

// FHPlayer: Because loading everything into memory is a wonderful idea!
// This whole design was a bit of a failure TBH.
// If trying in future, I'd regulate having a seekable file for seek support.

int main(int argc, char * argv[]) {
	if (argc != 1) {
		fputs("fhplayer < stuff.fh\n", stderr);
		return 1;
	}

	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		fputs("SDL did not init.\n", stderr);
		return 1;
	}

	char * error;
	fhstream_t * fhstream = fhstream_new(stdin, &error);
	if (!fhstream) {
		fputs("fhstream failed to init: ", stderr);
		fputs(error, stderr);
		fputs("\n", stderr);
		return 1;
	}

	uint32_t width = fhstream->head.imageHead.width;
	uint32_t height = fhstream->head.imageHead.height;
	uint32_t frameTimeMul = fhstream->head.frameTimeMul;
	uint32_t frameTimeDiv = fhstream->head.frameTimeDiv;

	SDL_Window * wnd = SDL_CreateWindow("FHPlayer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, 0);
	SDL_Surface * frameSurface = SDL_CreateRGBSurface(0, width, height, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000);
	int quitting = 0;
	Uint32 expectedTimeBase = SDL_GetTicks();
	size_t frameBase = 0;
	if (wnd && frameSurface) {
		SDL_Surface * surf = SDL_GetWindowSurface(wnd);
		while (!quitting) {
			while (1) {
				Uint32 expectedTime = expectedTimeBase + FARBHERD_TIMEDIV_TO_MS((fhstream->currentFrame - frameBase) * frameTimeMul, frameTimeDiv);
				int timeLeft = expectedTime - SDL_GetTicks();
				SDL_Event se;
				int i = 0;
				if (timeLeft > 0) {
					i = SDL_WaitEventTimeout(&se, timeLeft);
				} else {
					i = SDL_PollEvent(&se);
				}
				if (i) {
					if (se.type == SDL_QUIT) {
						quitting = 1;
						break;
					} else if (se.type == SDL_KEYDOWN) {
						if (se.key.keysym.sym == SDLK_BACKSPACE) {
							// This is an example of how to seek
							fhstream->currentFrame = 0;
							expectedTimeBase = SDL_GetTicks();
							frameBase = 0;
						}
					}
				}
				if (timeLeft <= 0)
					break;
			}
			SDL_LockSurface(frameSurface);
			if (!fhstream_getframe(frameSurface->pixels, fhstream)) {
				fhstream->currentFrame++;
			} else {
				expectedTimeBase = SDL_GetTicks() + 50;
				frameBase = fhstream->currentFrame;
			}
			SDL_UnlockSurface(frameSurface);
			SDL_Rect rect;
			rect.x = 0;
			rect.y = 0;
			rect.w = width;
			rect.h = height;
			SDL_FillRect(surf, NULL, SDL_MapRGB(frameSurface->format, 255, 0, 255));
			SDL_BlitSurface(frameSurface, NULL, surf, &rect);
			SDL_UpdateWindowSurface(wnd);
		}
	}
	if (wnd)
		SDL_DestroyWindow(wnd);
	if (frameSurface)
		SDL_FreeSurface(frameSurface);
	SDL_Quit();
	return 0;
}
