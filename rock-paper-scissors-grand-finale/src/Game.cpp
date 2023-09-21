#include "Game.h"

#include <SDL_image.h>
#include <SDL_ttf.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <random>
#include <iostream>

#include "misc.h"
#include "mathh.h"

void Game::Reset() {
	memset(entities, 0, entity_count * sizeof(*entities));

	for (int i = 0; i < entity_count; i++) {
		Entity* e = &entities[i];
		e->x = random.range(0.0f, map_w);
		e->y = random.range(0.0f, map_h);
		e->type = (EntityType) (random.next() % 3);
	}
}

void Game::Init() {
	std::cout << "entity count: ";
	std::cin >> entity_count;

	if (entity_count <= 0) {
		entity_count = 1000;
	}

	std::cout << "map size: ";
	std::cin >> map_w;

	if (map_w <= 0.0f) {
		map_w = 2000.0f;
	}

	map_h = map_w;

	std::cout << "entity speed: ";
	std::cin >> entity_speed;

	if (entity_speed <= 0.0f) {
		entity_speed = 1.0f;
	}

	entity_run_away_speed = entity_speed / 2.0f;

	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

	SDL_Init(SDL_INIT_VIDEO
			 | SDL_INIT_AUDIO);
	IMG_Init(IMG_INIT_PNG);
	TTF_Init();
	Mix_Init(0);

	// SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	window = SDL_CreateWindow("rock-paper-scissors-grand-finale",
							  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
							  GAME_W, GAME_H,
							  SDL_WINDOW_RESIZABLE);

	renderer = SDL_CreateRenderer(window, -1,
								  SDL_RENDERER_ACCELERATED
								  | SDL_RENDERER_TARGETTEXTURE);

	// SDL_RenderSetLogicalSize(renderer, GAME_W, GAME_H);

	Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 2048);

	Mix_Volume(-1, 32);

	tex_entities = IMG_LoadTexture(renderer, "entities.png");

	snd_rock     = Mix_LoadWAV("rock.wav");
	snd_paper    = Mix_LoadWAV("paper.wav");
	snd_scissors = Mix_LoadWAV("scissors.wav");

	{
		std::random_device d;
		uint32_t* s = (uint32_t*) random.s;
		s[0] = d();
		s[1] = d();
		s[2] = d();
		s[3] = d();
		s[4] = d();
		s[5] = d();
		s[6] = d();
		s[7] = d();
	}

	entities = (Entity*) malloc(entity_count * sizeof(*entities));

	if (!entities) {
		SDL_Log("Out of memory.");
		exit(1);
	}

	Reset();
}

void Game::Quit() {
	free(entities);

	Mix_FreeChunk(snd_scissors);
	Mix_FreeChunk(snd_paper);
	Mix_FreeChunk(snd_rock);

	SDL_DestroyTexture(tex_entities);

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	Mix_Quit();
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}

void Game::Run() {
	while (!quit) {
		Frame();
	}
}

void Game::Frame() {
	double t = GetTime();

	double frame_end_time = t + (1.0 / (double)GAME_FPS);

	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
				case SDL_QUIT: {
					quit = true;
					break;
				}

				case SDL_KEYDOWN: {
					SDL_Scancode scancode = ev.key.keysym.scancode;
					switch (scancode) {
						case SDL_SCANCODE_ESCAPE: {
							paused ^= true;
							break;
						}

						case SDL_SCANCODE_R: {
							Reset();
							break;
						}
					}
				}
			}
		}
	}

	float delta = 60.0f / (float)GAME_FPS;

	double update_took = GetTime();
	if (!paused) {
		Update(delta);
	}
	update_took = GetTime() - update_took;

	const Uint8* key = SDL_GetKeyboardState(nullptr);

	float spd = 20.0f;
	if (key[SDL_SCANCODE_LSHIFT]) spd = 10.0f;

	if (key[SDL_SCANCODE_LEFT])  camera_x -= spd * delta;
	if (key[SDL_SCANCODE_RIGHT]) camera_x += spd * delta;
	if (key[SDL_SCANCODE_UP])    camera_y -= spd * delta;
	if (key[SDL_SCANCODE_DOWN])  camera_y += spd * delta;

	double draw_took = GetTime();
	Draw(delta);
	draw_took = GetTime() - draw_took;

#ifndef __EMSCRIPTEN__
	t = GetTime();
	double time_left = frame_end_time - t;
	if (time_left > 0.0) {
		SDL_Delay((Uint32) (time_left * (0.95 * 1000.0)));
		while (GetTime() < frame_end_time) {}
	}
#endif

	if (frame % 60 == 0) {
		double fps = 1.0 / (t - prev_time);
		SDL_Log("update: %fms", update_took * 1000.0);
		SDL_Log("draw:   %fms", draw_took * 1000.0);
		SDL_Log("FPS:    %.2f\n\n", fps);
	}

	frame++;

	prev_time = t;
}

Entity* Game::find_closest(Entity* e) {
	Entity* result = nullptr;
	float dist = INFINITY;

	for (int i = 0; i < entity_count; i++) {
		Entity* e2 = &entities[i];

		if (e->type == e2->type) {
			continue;
		}

		float d = point_distance(e->x, e->y, e2->x, e2->y);
		if (d < dist) {
			dist = d;
			result = e2;
		}
	}

	return result;
}

void Game::Update(float delta) {
	for (int i = 0; i < entity_count; i++) {
		Entity* e = &entities[i];

		EntityType prey = EntityType::SCISSORS;
		// EntityType predator = EntityType::PAPER;
		if (e->type == EntityType::PAPER) {
			prey = EntityType::ROCK;
			// predator = EntityType::SCISSORS;
		} else if (e->type == EntityType::SCISSORS) {
			prey = EntityType::PAPER;
			// predator = EntityType::ROCK;
		}

		if (Entity* e2 = find_closest(e)) {
			float dx = e2->x - e->x;
			float dy = e2->y - e->y;
			normalize0(dx, dy, &dx, &dy);

			float spd = entity_speed;
			if (e2->type != prey) {
				spd = entity_run_away_speed;
				dx = -dx;
				dy = -dy;
			}

			e->x += dx * spd * delta;
			e->y += dy * spd * delta;

			float shiver = spd * entity_shiver_multiplier;
			e->x += random.range(-shiver, shiver);
			e->y += random.range(-shiver, shiver);
		}
	}

	for (int i = 0; i < entity_count; i++) {
		Entity* e = &entities[i];

		for (int j = 0; j < entity_count; j++) {
			if (i == j) {
				continue;
			}

			Entity* e2 = &entities[j];

			if (!circle_vs_circle(e->x, e->y, 16.0f, e2->x, e2->y, 16.0f)) {
				continue;
			}

			switch (e->type) {
				case EntityType::ROCK: {
					if (e2->type == EntityType::SCISSORS) {
						e2->type = EntityType::ROCK;
						play_sound(snd_rock);
					}
					break;
				}

				case EntityType::PAPER: {
					if (e2->type == EntityType::ROCK) {
						e2->type = EntityType::PAPER;
						play_sound(snd_paper);
					}
					break;
				}

				case EntityType::SCISSORS: {
					if (e2->type == EntityType::PAPER) {
						e2->type = EntityType::SCISSORS;
						play_sound(snd_scissors);
					}
					break;
				}
			}
		}
	}
}

void Game::Draw(float delta) {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);

	for (int i = 0; i < entity_count; i++) {
		Entity* e = &entities[i];

		SDL_Rect src = {
			(int)e->type * 32,
			0,
			32,
			32
		};

		SDL_Rect dest = {
			(int) (e->x - 16.0f - camera_x),
			(int) (e->y - 16.0f - camera_y),
			32,
			32
		};

		SDL_RenderCopy(renderer, tex_entities, &src, &dest);
	}

	SDL_RenderPresent(renderer);
}
