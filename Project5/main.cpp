#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <math.h>
#include <string>
#include <iostream>
#include <vector>

#define COLOR_WHITE 0xFF, 0xFF, 0xFF, 0xFF
#define COLOR_WALL_1 0x00, 0x22, 0x20, 0xFF
#define COLOR_WALL_2 0x55, 0x00, 0x00, 0xFF


#define sgn(a) (a < 0 ? -1 : a > 0 ? +1 : 0)

bool isColliding(double x, double y);

SDL_Texture* loadTexture(const char* file);
SDL_Surface* loadSurface(const char* file);
SDL_Texture* cropTexture(SDL_Texture* src, int x, int y);

bool spaceKeyPressed = false;
bool tabKeyPressed = false;

struct Texture {
    SDL_Texture* texture;
    int width;
    int height;
};

struct Weapon
{
    SDL_Texture* texture;
    Mix_Chunk* soundEffect;
    int ammo;
    int damage;
    int rpm;
};

Weapon pistol; //PISTOL
Weapon shotgun; //SHOTGUN
Weapon* currentWeapon; //STORE THE CURRENT WEAPON AS A POINTER


// resolution (and in windowed mode, the screen size)
const int SCREEN_WIDTH = 800;		// horizontal resolution
const int SCREEN_HEIGHT = 600;		// vertical resolution

// the field of view
const double FOV = 1.30899694; 		// rad (75 deg)
const double MIN_DIST = 0.3;		// square sides / s
const double DARKEST_DIST = 6.0;	// any greater distance will not be darker
const int DARKNESS_ALPHA = 40;		// minimal lighting

// world size
const int GRID_WIDTH = 18; 
const int GRID_HEIGHT = 10;

const double DEFAULT_SPEED = 3; 	// sqsides/s
const double TURN_SPEED = M_PI; 	// rad/s


void* mPixels; //we fucking need this for the buffer

// the fixe
const int world[GRID_HEIGHT][GRID_WIDTH] =
{
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1 },
{ 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1 },
{ 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1 },
{ 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
{ 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1 },
{ 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1 },
{ 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

//The window we'll be rendering to
SDL_Window* window;

//The surface contained by the window
SDL_Surface* gSurface;

SDL_Renderer* gRenderer; //RENDERER

SDL_Texture* gTexture; // CURRENT TEXTURE FOR RAYCASTED WALL

SDL_Texture* gFloorTexture; // FLOOR TEXTURE
SDL_Texture* gCeilgTexture; // CEILING TEXTURE

Mix_Music* backgroundMusic; // LOOPED BACKGROUND MUSIC

TTF_Font* gFont; // CURRENT FONT

// a GPU accelerated wall texture
SDL_Texture* txt_walls[5];

SDL_Surface* srf_floor;
SDL_Surface* srf_ceilg;

Mix_Chunk* snd_meinleben; // NO FUCKING CLUE WHAT THIS IS????

// the initial player position
double px = 1.5, py = 1.5; // STORING PLAYER POSITION ON THIS X AND Y

// the intial central ray direction
double teta = 0.0; // WE NEED THIS TO READ THE PLAYER POSITION

// the tangent y-coordinate for teta=0, one for every horizontal pixel
double scr_pts[SCREEN_WIDTH];

// correction values for distortion
double distortion[SCREEN_WIDTH];

int ticks;			// ms 
int diffTicks;		// ms

double speed = 0.0; // sqsides/s
double turn = 0.0;  // rad/s


int init()
{
    double tan_FOV = tan(FOV / 2); 

    // a ray will be casted for every horizontal pixel
    for (int i = 0; i < SCREEN_WIDTH; i++) {
        scr_pts[i] = tan_FOV - (2 * tan_FOV * (i + 1)) / SCREEN_WIDTH;
        distortion[i] = 1.0 / sqrt(1 + scr_pts[i] * scr_pts[i]);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    window = SDL_CreateWindow( // We are making the WINDOW
        "Raycaster Demo",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0);
    if (window == NULL)
    {
        printf("Window could not be created dumbass SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    gSurface = SDL_GetWindowSurface(window); // MAKE A SURFACE TO RENDER ON
    if (gSurface == NULL)
    {
        printf("Surface could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    gRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED); // THE RENDERER
    if (gRenderer == NULL)
    {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    // Set renderer draw color
    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF); //BLACK ON BLACK ON BLACK, WE OBVIOUSLY DON'T WANT THIS SHIT TO BE RED OR SMTH LIKE THAT

    gTexture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (gTexture == NULL)
    {
        printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    //this dumbass method of renderer every single damn texture on at a time is very tedious will fix in the future

    gFloorTexture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 1, SCREEN_HEIGHT);
    if (gFloorTexture == NULL)
    {
        printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    gCeilgTexture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 1, SCREEN_HEIGHT);
    if (gCeilgTexture == NULL)
    {
        printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() < 0)
    {
        printf("TTF could not initialize! TTF_Error: %s\n", TTF_GetError());
        return -1;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
    }

    SDL_Surface* pistolSurface = IMG_Load("gun2.png");
    if (pistolSurface == nullptr)
    {
        printf("Failed to load pistol.png! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    pistol.texture = SDL_CreateTextureFromSurface(gRenderer, pistolSurface);
    SDL_FreeSurface(pistolSurface);

    // Load shotgun texture
    SDL_Surface* shotgunSurface = IMG_Load("gun1a.png");
    if (shotgunSurface == nullptr)
    {
        printf("Failed to load shotgun.png! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    shotgun.texture = SDL_CreateTextureFromSurface(gRenderer, shotgunSurface);
    SDL_FreeSurface(shotgunSurface);

    snd_meinleben = Mix_LoadWAV("mein_leben.wav");

    pistol.soundEffect = Mix_LoadWAV("dspistol.wav");
    if (pistol.soundEffect == nullptr)
    {
        printf("Failed to load pistol.wav! SDL_mixer Error: %s\n", Mix_GetError());
        return false;
    }

    shotgun.soundEffect = Mix_LoadWAV("dsshotgn.wav");
    if (shotgun.soundEffect == nullptr)
    {
        printf("Failed to load shotgun.wav! SDL_mixer Error: %s\n", Mix_GetError());
        return false;
    }

    backgroundMusic = Mix_LoadMUS("d_e3m1.wav");
    if (backgroundMusic == nullptr) {
        printf("Failed to load background music! SDL_mixer Error: %s\n", Mix_GetError());
        // Handle error
    }


    SDL_Color color = { 0x00, 0x00, 0x00 };
    gFont = TTF_OpenFont("arial.ttf", 36);
    if (gFont == NULL)
    {
        printf("Could not load font! SDL error: %s\n", TTF_GetError());
    }

    SDL_SetWindowTitle(window, "Raycaster in C++ and SDL");

    SDL_Texture* wall1 = loadTexture("tile000.png");
    SDL_Texture* wall2 = loadTexture("w3d_grayflag.png");
    SDL_Texture* wall3 = loadTexture("w3d_hitler.png"); // IGNORE THIS

    txt_walls[0] = wall1;
    txt_walls[1] = wall2;
    txt_walls[2] = wall3;

    //WHY IS IT GREEN????

    srf_ceilg = loadSurface("tile000.png");
    srf_floor = loadSurface("wood.png");

    return 0;
}

//Used to cut the sprites and shit

SDL_Texture* cropTexture(SDL_Texture* src, int x, int y)
{
    SDL_Texture* dst = SDL_CreateTexture(gRenderer, gSurface->format->format, SDL_TEXTUREACCESS_TARGET, 64, 64);
    SDL_Rect rect = { 64 * x, 64 * y, 64, 64 };
    SDL_SetRenderTarget(gRenderer, dst);
    SDL_RenderCopy(gRenderer, src, &rect, NULL);
    SDL_SetRenderTarget(gRenderer, NULL);

    return dst;
}

// SDL SUCKS SO WE GOTTA LOAD ALL OF OUR TEXTURE OURSELVES

SDL_Texture* loadTexture(const char* file)
{
    SDL_Surface* srfc = loadSurface(file);
    if (!srfc) {
        printf("IMG_LoadPNG_RW: %s\n", IMG_GetError());
        // handle error
    }

    SDL_Texture* txt = SDL_CreateTextureFromSurface(gRenderer, srfc);

    SDL_FreeSurface(srfc);

    return txt;
}

SDL_Surface* loadSurface(const char* file)
{
    SDL_RWops* rwop = SDL_RWFromFile(file, "rb");
    SDL_Surface* loaded = IMG_LoadPNG_RW(rwop);
    SDL_Surface* conv = NULL;
    if (loaded != NULL)
    {
        conv = SDL_ConvertSurface(loaded, gSurface->format, 0);
        SDL_FreeSurface(loaded);
    }
    return conv;
}


//Basic Weapon Initialization

void initWeapons()
{
    pistol.ammo = 20;
    pistol.damage = 10;
    pistol.rpm = 50;

    shotgun.ammo = 10;
    shotgun.damage = 25;
    shotgun.rpm = 50;

    currentWeapon = &pistol;
}

// Handle key down even
Uint32 weaponSwitchCooldown = 500;  // Cooldown duration in milliseconds (adjust as needed)
Uint32 lastWeaponSwitchTime = 0;


void switchWeapon()
{
    Uint32 currentTime = SDL_GetTicks();  // Get the current timestamp
    if (currentTime - lastWeaponSwitchTime >= weaponSwitchCooldown) {
        // Perform the weapon switch logic here
        if (currentWeapon == &pistol)
        {
            currentWeapon = &shotgun;
        }
        else
        {
            currentWeapon = &pistol;
        }
        // Update the last weapon switch time
        lastWeaponSwitchTime = currentTime;
    }
    
}


// Global variable for tracking the time of the last shot
// Global variable for rounds per minute (RPM) // Example value: 600 RPM
Uint32 lastShotTime;

// Fire the current weapon
void fireWeapon()
{
    Uint32 currentTime = SDL_GetTicks();

    // Calculate the cooldown duration based on rounds per minute (RPM)
    Uint32 cooldownDuration = 60000 / currentWeapon->rpm;

    // Check if enough time has passed since the last shot
    if (currentTime - lastShotTime >= cooldownDuration)
    {
        if (currentWeapon->ammo > 0)
        {
            // Reduce ammo count
            currentWeapon->ammo--;

            // Perform firing logic (e.g., raycasting, damage calculations, etc.)

            // Play the sound effect
            Mix_PlayChannel(-1, currentWeapon->soundEffect, 0);

            printf("Weapon fired! Ammo: %d\n", currentWeapon->ammo);

            // Update the time of the last shot
            lastShotTime = currentTime;
        }
        else
        {
            printf("Out of ammo!\n");
        }
    }
    else
    {
        printf("Weapon on cooldown!\n");
    }
}



// Render the current weapon and ammo on the screen
void renderWeapon()
{
    SDL_Color color = { 0x00, 0x00, 0x00 };
    SDL_Rect destRect;
    destRect.w = 294;
    destRect.h = 322;
    destRect.x = (SCREEN_WIDTH - destRect.w) / 2;  // Center horizontally
    destRect.y = SCREEN_HEIGHT - destRect.h + 10;  // Offset from the bottom

    SDL_RenderCopy(gRenderer, currentWeapon->texture, nullptr, &destRect);

    // Render ammo count
    char ammoText[16];
    snprintf(ammoText, sizeof(ammoText), "Ammo: %d", currentWeapon->ammo);
    SDL_Surface* surface = TTF_RenderText_Solid(gFont, ammoText, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(gRenderer, surface);

    SDL_Rect ammoRect;
    ammoRect.w = surface->w;
    ammoRect.h = surface->h;
    ammoRect.x = SCREEN_WIDTH - ammoRect.w - 10;   // Offset from the right
    ammoRect.y = SCREEN_HEIGHT - ammoRect.h - 10;
    

    SDL_RenderCopy(gRenderer, texture, nullptr, &ammoRect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}



// Example function to remove bullets




void renderScene()
{

    for (int i = 0; i < SCREEN_WIDTH; i++)
    {
        double r_x = 1.0;
        double r_y = scr_pts[i]; // precalculated
        double base = sqrt(r_x * r_x + r_y * r_y);
        r_x = r_x / base;
        r_y = r_y / base;

        // rotate this ray with teta
        double rot_x = cos(teta) * r_x - sin(teta) * r_y;
        double rot_y = sin(teta) * r_x + cos(teta) * r_y;

        // step sizes
        int step_x = sgn(rot_x);
        int step_y = sgn(rot_y);

        // grid lines and hitpoints to calculate
        int l_vx = round(px + 0.5 * step_x);
        double l_vy = -1;
        int l_hy = round(py + 0.5 * step_y);
        double l_hx = -1;

        // find hitpoint
        double dist = -1;
        int txt_x = -1; // 0..63 (texture width = 64)

        double hit_x = 0, hit_y = 0;
        int wall_idx;
        while (dist < 0)
        {
            // calculate the hitpoints with the grid lines
            if (l_vy == -1 && step_x != 0)
                l_vy = py + (l_vx - px) * (rot_y / rot_x);

            if (l_hx == -1 && step_y != 0)
                l_hx = px + (l_hy - py) * (rot_x / rot_y);

            // determine which one "wins" (= shortest distance)
            bool vertWin;
            if (l_vy != -1 && l_hx != -1)
            {    // 2 candidates, choose closest one
                vertWin = step_x * (l_vx - px) < step_x * (l_hx - px);
            }
            else
            {    // one candidate
                vertWin = l_vy != -1;
            }

            // determine array indices
            int arr_x = -1, arr_y = -1;
            if (vertWin)
            {
                hit_x = l_vx;
                hit_y = l_vy;

                txt_x = 64 * (hit_y - (int)hit_y);
                if (step_x == 1)
                { 	// // looking from the left, mirror the texture to correct
                    txt_x = 63 - txt_x;
                }

                l_vx += step_x;
                l_vy = -1;

                arr_x = step_x < 0 ? hit_x - 1 : hit_x;
                arr_y = GRID_HEIGHT - hit_y;
            }
            else
            {
                hit_x = l_hx;
                hit_y = l_hy;

                txt_x = 64 * (hit_x - (int)hit_x);
                if (step_y == -1)
                { 	// looking from above, mirror the texture to correct
                    txt_x = 63 - txt_x;
                }

                l_hx = -1;
                l_hy += step_y;

                arr_x = hit_x;
                arr_y = GRID_HEIGHT - (step_y < 0 ? hit_y - 1 : hit_y) - 1;
            }

            wall_idx = world[arr_y][arr_x];
            if (wall_idx != 0)
            {    // we've hit a block
                double dx = hit_x - px;
                double dy = hit_y - py;
                dist = sqrt(dx * dx + dy * dy);
            }
        }

        // correct distance and calculate height
        double corrected = dist * distortion[i];
        int height = SCREEN_HEIGHT / corrected;

        int y = (SCREEN_HEIGHT - height) / 2;
        int darkness = 255;
        if (corrected > DARKEST_DIST)
            darkness = DARKNESS_ALPHA;
        else if (corrected <= MIN_DIST)
            darkness = 255;
        else // interpolate
            darkness = (int)((corrected - MIN_DIST) * (DARKNESS_ALPHA - 255) / (DARKEST_DIST - MIN_DIST) + 255);

        SDL_Rect src = { txt_x, 0, 1, 64 };
        SDL_Rect dst = { i, y, 1, height };

        if (wall_idx - 1 >= 3)
        {
            printf("%d\n", wall_idx);
        }
        SDL_Texture* txt = txt_walls[wall_idx - 1];

        SDL_SetTextureColorMod(txt, darkness, darkness, darkness);
        SDL_RenderCopy(gRenderer, txt, &src, &dst);

        // get floor texture pixels

        if (y > 0)
        {
            Uint32* floor = new Uint32[y];
            Uint32* ceilg = new Uint32[y];
            Uint32* pixsflr = (Uint32*)srf_floor->pixels;
            Uint32* pixsclg = (Uint32*)srf_ceilg->pixels;
            for (int j = y - 1; j >= 0; j--)
            {
                double rev_height = SCREEN_HEIGHT - 2 * j;
                double rev_corr = SCREEN_HEIGHT / rev_height;
                double rev_dist = rev_corr / distortion[i];

                double real_x = px + rot_x * rev_dist;
                double real_y = py + rot_y * rev_dist;

                real_x = real_x - (int)real_x;
                real_y = real_y - (int)real_y;
                if (real_x < 0) real_x += 1;
                if (real_y < 0) real_y += 1;
                int tx = (int)(real_x * 64);
                int ty = (int)(real_y * 64);

                int darkflr = 255;
                if (rev_corr > DARKEST_DIST)
                    darkflr = DARKNESS_ALPHA;
                else if (rev_corr <= MIN_DIST)
                    darkflr = 255;
                else // interpolate
                    darkflr = (int)((rev_corr - MIN_DIST) * (DARKNESS_ALPHA - 255) / (DARKEST_DIST - MIN_DIST) + 255);
                double scale = 1.0 * darkflr / 255;

                Uint32 pixflr = (Uint32)pixsflr[64 * ty + tx];
                Uint32 pixclg = (Uint32)pixsclg[64 * ty + tx];

                Uint32 f_r = ((pixflr >> 16) & 0xFF) * scale;
                Uint32 f_g = ((pixflr >> 8) & 0xFF) * scale;
                Uint32 f_b = ((pixflr >> 0) & 0xFF) * scale;
                floor[y - 1 - j] = (f_r << 16) + (f_g << 8) + (f_b << 0);

                Uint32 g_r = ((pixclg >> 16) & 0xFF) * scale;
                Uint32 g_g = ((pixclg >> 8) & 0xFF) * scale;
                Uint32 g_b = ((pixclg >> 0) & 0xFF) * scale;
                ceilg[j] = (g_r << 16) + (g_g << 8) + (g_b << 0);

                
            }

            int pitch = 1 * sizeof(Uint32);
            SDL_Rect rect = { 0, 0, 1, y };

            if (SDL_LockTexture(gFloorTexture, &rect, &mPixels, &pitch) != 0) {
                printf("Error: %s\n", SDL_GetError());
            }
            Uint8* pixels = (Uint8*)mPixels;
            memcpy(pixels, floor, y * pitch);
            SDL_UnlockTexture(gFloorTexture);
            SDL_Rect dstflr = { i, y + height, 1, y };
            SDL_RenderCopy(gRenderer, gFloorTexture, &rect, &dstflr);

            if (SDL_LockTexture(gCeilgTexture, &rect, &mPixels, &pitch) != 0) {
                printf("Error: %s\n", SDL_GetError());
            }
            pixels = (Uint8*)mPixels;
            memcpy(pixels, ceilg, y * pitch);
            SDL_UnlockTexture(gCeilgTexture);

            SDL_Rect dstclg = { i, 0, 1, y };
            SDL_RenderCopy(gRenderer, gCeilgTexture, &rect, &dstclg);
        }

    }
}

void updateTicks() {
    int ticksNow = SDL_GetTicks();
    diffTicks = ticksNow - ticks;
    ticks = ticksNow;
}

void renderFPS()
{
    int fps = 0;
    if (diffTicks != 0)
    {
        fps = 1000 / diffTicks;
    }

    SDL_Color color = { COLOR_WHITE };
    std::string fps_str("FPS: ");
    std::string full = fps_str + std::to_string(fps);

    SDL_Surface* surface = TTF_RenderText_Blended(gFont, full.c_str(), color);
    int w = surface->w;
    int h = surface->h;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(gRenderer, surface);
    SDL_FreeSurface(surface);
    SDL_Rect rect = { 0, 0, w, h };
    SDL_RenderCopy(gRenderer, texture, NULL, &rect);
}

void close()
{
    if (gRenderer != NULL)
        SDL_DestroyRenderer(gRenderer);
    if (gSurface != NULL)
        SDL_FreeSurface(gSurface);
    if (window != NULL)
        SDL_DestroyWindow(window);
    if (gTexture != NULL)
        SDL_DestroyTexture(gTexture);
    if (gFont != NULL) {
        TTF_CloseFont(gFont);
    }

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void handleInput(const Uint8* states)
{
    speed = 0.0;
    turn = 0.0;
    if (states[SDL_SCANCODE_UP])
    {
        speed += DEFAULT_SPEED;
    }
    if (states[SDL_SCANCODE_DOWN])
    {
        speed -= DEFAULT_SPEED;
    }
    if (states[SDL_SCANCODE_LEFT])
    {
        turn += TURN_SPEED;
    }
    if (states[SDL_SCANCODE_RIGHT])
    {
        turn -= TURN_SPEED;
    }
    if (states[SDL_SCANCODE_LSHIFT])
    {
        speed *= 2;
        turn *= 2;
    }
    if (spaceKeyPressed)
    {
        fireWeapon();
    }

    if (tabKeyPressed)
    {
        switchWeapon();
    }
}

void updateWorld() {
    double coss = cos(teta);
    double sinn = sin(teta);

    int dsgn = sgn(speed); // for when walking backwards
    double sx = sgn(coss) * dsgn;
    double sy = sgn(sinn) * dsgn;

    double dt = (diffTicks / 1000.0);
    double dp = dt * speed;
    double dx = coss * dp;
    double dy = sinn * dp;

    double px_new = px + dx;
    double py_new = py + dy;

    // collision detection
    double cx_b = px_new - sx * MIN_DIST;
    double cx_f = px_new + sx * MIN_DIST;
    double cy_b = py_new - sy * MIN_DIST;
    double cy_f = py_new + sy * MIN_DIST;
    if (!isColliding(cx_f, cy_f)
        && !isColliding(cx_b, cy_f)
        && !isColliding(cx_f, cy_b))
    {	// direction where playing is looking at
        px = px_new;
        py = py_new;
    }
    else if (!isColliding(cx_f, py + MIN_DIST)
        && !isColliding(cx_f, py - MIN_DIST))
    { // X-direction
        px = px_new;
    }
    else if (!isColliding(px + MIN_DIST, cy_f)
        && !isColliding(px - MIN_DIST, cy_f))
    { // Y-direction
        py = py_new;
    }
    // else: no movement possible (corner)	

    double diffTurn = dt * turn;
    teta += diffTurn;
}

bool isColliding(double x, double y)
{
    int arr_x = (int)x;
    int arr_y = GRID_HEIGHT - 1 - (int)y;

    return world[arr_y][arr_x] != 0;
}

int main(int argc, char* args[])
{
    if (init() < 0)
        return -1;

    initWeapons();
    Mix_PlayMusic(backgroundMusic, -1);  // -1 means play the music looped indefinitely


    bool quit = false;

    while (!quit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_SPACE && !spaceKeyPressed)
                {
                    spaceKeyPressed = true;
                    // Handle space key press
                }
                if (event.key.keysym.sym == SDLK_TAB && !spaceKeyPressed)
                {
                    tabKeyPressed = true;
                    // Handle space key press
                }
            }
            else if (event.type == SDL_KEYUP)
            {
                if (event.key.keysym.sym == SDLK_SPACE)
                {
                    spaceKeyPressed = false;
                    // Handle space key release
                }
                if (event.key.keysym.sym == SDLK_TAB)
                {
                    tabKeyPressed = false;
                    // Handle space key release
                }

            }
        }


        const Uint8* keyStates = SDL_GetKeyboardState(NULL);
        handleInput(keyStates);
        updateTicks();
        updateWorld();


        //Fill the surface black
        SDL_SetRenderDrawColor(gRenderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(gRenderer);

        SDL_SetRenderTarget(gRenderer, gTexture);
        SDL_RenderClear(gRenderer);

        renderScene();
        renderFPS();
        renderWeapon();

        SDL_SetRenderTarget(gRenderer, NULL);

        SDL_RenderCopy(gRenderer, gTexture, NULL, NULL);

        // draw everything
        SDL_RenderPresent(gRenderer);
    }
    close();

    //Destroy window
    SDL_DestroyWindow(window);

    //Quit SDL subsystems
    SDL_Quit();


    return 0;
}