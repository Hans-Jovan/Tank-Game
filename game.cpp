// --- Header dan Library ---
#include <graphics.h>
#include <windows.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include <mmsystem.h>

// --- Fungsi Load Image ---
void* loadimage(const char* filename, int width, int height) {
    void* buffer = new char[imagesize(0, 0, width, height)];
    readimagefile(filename, 0, 0, width, height);
    getimage(0, 0, width, height, buffer);
    cleardevice();
    return buffer;
}

void showGameOverScreen(int width, int height, int killCount) {
    cleardevice();
    setbkcolor(BLACK);
    settextstyle(BOLD_FONT, HORIZ_DIR, 5);
    setcolor(RED);
    outtextxy(width / 2 - 180, height / 2 - 100, (char*)"GAME OVER");

    settextstyle(DEFAULT_FONT, HORIZ_DIR, 3);
    setcolor(WHITE);
    char buffer[64];
    sprintf(buffer, "Kill: %d", killCount);
    outtextxy(width / 2 - 60, height / 2, buffer);

    delay(3000);  
    closegraph();
    exit(0);
}


void* MaskBullet = NULL;
void* GambarBullet = NULL;
void* GambarObstacle = NULL;
void* MaskedTank = NULL;
void* Tank = NULL;
void* Turret[8];
void* MaskedTurret[8];
void* background = NULL;






// --- Class Obstacle ---
class Obstacle {
public:
    int x, y, w, h;
};

// --- Class OpenSpot ---
class OpenSpot {
public:
    int x, y, timer;
};

// --- Class Bullet ---
class Bullet {
public:
    float x, y, vx, vy;
};

// --- Class Enemy ---
class Enemy {
public:
    int x, y;
    float angle;
    DWORD lastShotTime;
    int aiType;
    Enemy(int x, int y, int aiType) : x(x), y(y), angle(0), lastShotTime(0), aiType(aiType) {}
};

// --- Class Player ---
class Player {
public:
    int x, y;
    float angle;
    Player() : x(100), y(100), angle(0) {}
};

// --- Game Class ---
class Game {
public:
    int width = 1280, height = 720;
    int maxX, maxY;
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Obstacle> obstacles;
    std::vector<OpenSpot> openedSpots;
    std::vector<Bullet> enemyBullets;
    Bullet playerBullet;
    bool shot = false, prevMouseDown = false;
    DWORD lastShotTimePlayer = 0;
    int hp = 3, Kill = 0;
    const int playerSpeed = 2, visionRadius = 200, bulletRadius = 10, openRadius = 50, openDuration = 150;

    Game() {
        initwindow(width, height, "Game Window");
        maxX = getmaxx();
        maxY = getmaxy();
        mciSendStringA("open Player.wav alias player", NULL, 0, NULL);
        mciSendStringA("open Enemy.wav alias enemy", NULL, 0, NULL);
        mciSendStringA("open soundtrack.mp3 alias soundtrack", NULL, 0, NULL);
        mciSendString("play soundtrack from 0 repeat", NULL, 0, NULL);
        enemies = { Enemy(400, 300, 0), Enemy(600, 150, 1), Enemy(800, 500, 2) };
        obstacles = { {300, 200, 80, 80}, {700, 400, 80, 80}, {500, 100, 80, 80} };
    }

    bool isBlocked(int x, int y, int w, int h) {
        for (auto& o : obstacles) {
            if (x + w > o.x && x < o.x + o.w && y + h > o.y && y < o.y + o.h)
                return true;
        }
        return false;
    }

    void run() {
        while (!GetAsyncKeyState(VK_ESCAPE)) {
            update();
            render();
            delay(16);
            swapbuffers();
        }
        PlaySound(NULL, 0, 0);
        closegraph();
    }

    void update() {
        handleInput();
        updatePlayerBullet();
        updateEnemies();
        updateEnemyBullets();
        checkCollisions();
        updateOpenSpots();
    }

    void handleInput() {
        int nx = player.x, ny = player.y;
        if (GetAsyncKeyState(VK_UP)) ny -= playerSpeed;
        if (GetAsyncKeyState(VK_DOWN)) ny += playerSpeed;
        if (GetAsyncKeyState(VK_LEFT)) nx -= playerSpeed;
        if (GetAsyncKeyState(VK_RIGHT)) nx += playerSpeed;

        if (!isBlocked(nx, player.y, 20, 20)) player.x = std::clamp(nx, 0, maxX - 20);
        if (!isBlocked(player.x, ny, 20, 20)) player.y = std::clamp(ny, 0, maxY - 20);

        POINT m; GetCursorPos(&m);
        ScreenToClient(GetForegroundWindow(), &m);
        float dx = m.x - (player.x + 10), dy = m.y - (player.y + 10);
        float targetAngle = atan2(dy, dx);
        float diff = targetAngle - player.angle;
        if (diff > M_PI) diff -= 2 * M_PI;
        if (diff < -M_PI) diff += 2 * M_PI;
        player.angle += diff * 0.025f;

        DWORD now = GetTickCount();
        bool mouseDown = GetAsyncKeyState(VK_LBUTTON) & 0x8000;
        if (mouseDown && !prevMouseDown && !shot && now - lastShotTimePlayer >= 3000) {
            float speed = 10;
            playerBullet = { player.x + 10 + 20 * cos(player.angle), player.y + 10 + 20 * sin(player.angle), speed * cos(player.angle), speed * sin(player.angle) };
            shot = true;
            lastShotTimePlayer = now;
            mciSendStringA("play player from 0", NULL, 0, NULL);
        }
        prevMouseDown = mouseDown;
    }

    void updatePlayerBullet() {
        if (!shot) return;
        playerBullet.x += playerBullet.vx;
        playerBullet.y += playerBullet.vy;

        if (playerBullet.x < 0 || playerBullet.x > maxX || playerBullet.y < 0 || playerBullet.y > maxY) shot = false;
        for (auto& o : obstacles) {
            if (playerBullet.x >= o.x && playerBullet.x <= o.x + o.w &&
                playerBullet.y >= o.y && playerBullet.y <= o.y + o.h) {
                shot = false;
                return;
            }
        }
        for (auto& e : enemies) {
            if (playerBullet.x >= e.x && playerBullet.x <= e.x + 20 &&
                playerBullet.y >= e.y && playerBullet.y <= e.y + 20) {
                openedSpots.push_back({ e.x + 10, e.y + 10, openDuration });
                e.x = rand() % (maxX - 20);
                e.y = rand() % (maxY - 20);
                Kill++;
                shot = false;
                delay(120);
                return;
            }
        }
    }

    void updateEnemies() {
        for (auto& e : enemies) {
            float dx = (player.x + 10) - (e.x + 10);
            float dy = (player.y + 10) - (e.y + 10);
            float len = sqrt(dx * dx + dy * dy);
            if (len > 0) e.angle = atan2(dy, dx);

            int moveX = 0, moveY = 0;
            if (e.aiType == 1 && len > 1) {
                moveX = (int)(dx / len * 2);
                moveY = (int)(dy / len * 2);
            } else if (e.aiType == 2) {
                moveX = (int)(2 * cos(GetTickCount() / 200.0));
                moveY = (int)(2 * sin(GetTickCount() / 300.0));
            }
            if (!isBlocked(e.x + moveX, e.y, 20, 20)) e.x += moveX;
            if (!isBlocked(e.x, e.y + moveY, 20, 20)) e.y += moveY;

            if (GetTickCount() - e.lastShotTime >= 3000) {
                enemyBullets.push_back({ e.x + 10.0f, e.y + 10.0f, 5 * dx / len, 5 * dy / len });
                e.lastShotTime = GetTickCount();
                mciSendStringA("play enemy from 0", NULL, 0, NULL);
            }
        }
    }

    void updateEnemyBullets() {
        for (auto& b : enemyBullets) {
            b.x += b.vx; b.y += b.vy;
        }
        enemyBullets.erase(std::remove_if(enemyBullets.begin(), enemyBullets.end(), [&](Bullet& b) {
            for (auto& o : obstacles) {
                if (b.x >= o.x && b.x <= o.x + o.w && b.y >= o.y && b.y <= o.y + o.h)
                    return true;
            }
            return b.x < 0 || b.x > maxX || b.y < 0 || b.y > maxY;
        }), enemyBullets.end());
    }

    void checkCollisions() {
        for (auto& b : enemyBullets) {
            float dx = b.x - (player.x + 10), dy = b.y - (player.y + 10);
            if (sqrt(dx * dx + dy * dy) <= 15) {
                hp--;
                enemyBullets.clear();
                if (hp <= 0) {
                    delay(1000);
                    showGameOverScreen(width, height, Kill);
                }

                break;
            }
        }
    }

    void updateOpenSpots() {
        for (auto& s : openedSpots) s.timer--;
        openedSpots.erase(std::remove_if(openedSpots.begin(), openedSpots.end(), [](OpenSpot& s) {
            return s.timer <= 0;
        }), openedSpots.end());
    }

    void render() {
        cleardevice();
        bar(0, 0, maxX, maxY);
        for (int i = 0; i < 6; ++i) {
            int gray = 100 - i * 14;
            setfillstyle(SOLID_FILL, COLOR(gray, gray, gray));
            fillellipse(player.x + 10, player.y + 10, visionRadius - i * 10, visionRadius - i * 10);
            putimage(0, 0, background, COPY_PUT);
        }
        
        for (auto& s : openedSpots) {
            setfillstyle(SOLID_FILL, BLACK);
            fillellipse(s.x, s.y, openRadius, openRadius);
        }

        

        // Tampilkan tank & moncongnya

        putimage(player.x, player.y, MaskedTank, AND_PUT);
        putimage(player.x, player.y, Tank, XOR_PUT);

       // Hitung sudut dalam derajat
        int angleDeg = (int)(player.angle * 180.0 / M_PI);
        if (angleDeg < 0) angleDeg += 360;

        // Hitung index sprite turret
        int index = ((angleDeg + 22 - 90) / 45) % 8;
        if (index < 0) index += 8;

        // Posisi tengah tank
        int centerX = player.x + 15;
        int centerY = player.y + 15;

        // Ukuran turret
        const int turretWidth = 15, turretHeight = 15;

        // Gambar turret di tengah tank sesuai arah
        putimage(centerX - turretWidth / 2, centerY - turretHeight / 2, MaskedTurret[index], AND_PUT);
        putimage(centerX - turretWidth / 2, centerY - turretHeight / 2, Turret[index], XOR_PUT);



       
       
        if (shot) {
            
            putimage(playerBullet.x, playerBullet.y, MaskBullet, AND_PUT);
            putimage(playerBullet.x, playerBullet.y, GambarBullet, XOR_PUT);
        }

        

        for (auto& o : obstacles)
            putimage(o.x, o.y, GambarObstacle, COPY_PUT);

        for (auto& e : enemies) {
            float dist = sqrt(pow(player.x - e.x, 2) + pow(player.y - e.y, 2));
            bool visible = dist <= visionRadius;
            for (auto& s : openedSpots) {
                if (!visible && sqrt(pow(s.x - (e.x + 10), 2) + pow(s.y - (e.y + 10), 2)) <= openRadius) {
                    visible = true;
                    break;
                }
            }
            if (!visible) continue;

            setfillstyle(SOLID_FILL, RED);
            bar(e.x, e.y, e.x + 20, e.y + 20);
            setcolor(WHITE);
            line(e.x + 10, e.y + 10, e.x + 10 + (int)(20 * cos(e.angle)), e.y + 10 + (int)(20 * sin(e.angle)));
        }

        setfillstyle(SOLID_FILL, WHITE);
        for (auto& b : enemyBullets)
            fillellipse((int)b.x, (int)b.y, 5, 5);

        char buffer[32];
        sprintf(buffer, "HP: %d", hp);
        outtextxy(10, 10, buffer);
        sprintf(buffer, "Kill: %d", Kill);
        outtextxy(10, 40, buffer);
    }
};

void showStartScreen(int width, int height) {
    cleardevice();

    // Tampilkan judul "OMBRE" dengan efek warna perang (gradasi ombre)
    settextstyle(BOLD_FONT, HORIZ_DIR, 4);
    char* title = (char*)"OMBRE";
    int x = width / 2 - 90;
    int y = height / 2 - 100;

    for (int i = 0; i < strlen(title); ++i) {
        // Efek warna: merah → oranye → kuning → keputihan
        int r = std::min(255, 200 + i * 12);     // merah dominan
        int g = std::min(255, 50 + i * 35);      // hijau naik → oranye → kuning
        int b = std::max(0, 40 - i * 10);        // biru makin redup

        setcolor(COLOR(r, g, b));
        char huruf[2] = { title[i], '\0' };
        outtextxy(x + i * 35, y, huruf);  // Jarak antar huruf
    }

    // Gambar tombol "PLAY"
    int buttonX = width / 2 - 75;
    int buttonY = height / 2;
    int buttonW = 150;
    int buttonH = 50;

    setfillstyle(SOLID_FILL, LIGHTBLUE);
    bar(buttonX, buttonY, buttonX + buttonW, buttonY + buttonH);

    setcolor(BLACK);
    settextstyle(DEFAULT_FONT, HORIZ_DIR, 2);
    outtextxy(buttonX + 45, buttonY + 15, (char*)"PLAY");

    // Tunggu sampai tombol diklik
    while (true) {
        if (GetAsyncKeyState(VK_LBUTTON)) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(GetForegroundWindow(), &p);

            int mx = p.x;
            int my = p.y;

            if (mx >= buttonX && mx <= buttonX + buttonW &&
                my >= buttonY && my <= buttonY + buttonH) {
                delay(200);  // Hindari double-click
                break;
            }
        }
        delay(20);
    }

    cleardevice(); // Hapus layar awal sebelum masuk game
}





int main() {
    Game game;
    showStartScreen(game.width, game.height);

    background = loadimage("background.gif", game.width, game.height);
    GambarObstacle = loadimage("Tree.gif", 80, 80);

    const int bulletWidth = 8, bulletHeight = 8;
    const int tankWidth = 20, tankHeight = 20;
    const int turretWidth = 10, turretHeight = 10;

    // Load Bullet
    readimagefile("C:/Users/Hans/Documents/coding/graphics/MaskedBullet.gif", 0, 0, bulletWidth - 1, bulletHeight - 1);
    MaskBullet = malloc(imagesize(0, 0, bulletWidth - 1, bulletHeight - 1));
    getimage(0, 0, bulletWidth - 1, bulletHeight - 1, MaskBullet);

    readimagefile("C:/Users/Hans/Documents/coding/graphics/Bullet.gif", 0, 0, bulletWidth - 1, bulletHeight - 1);
    GambarBullet = malloc(imagesize(0, 0, bulletWidth - 1, bulletHeight - 1));
    getimage(0, 0, bulletWidth - 1, bulletHeight - 1, GambarBullet);


    // Load Tank
    readimagefile("C:/Users/Hans/Documents/coding/graphics/MaskedTank.gif", 0, 0, tankWidth, tankHeight);
    MaskedTank = malloc(imagesize(0, 0, tankWidth - 1, tankHeight - 1));
    getimage(0, 0, tankWidth - 1, tankHeight - 1, MaskedTank);

    readimagefile("C:/Users/Hans/Documents/coding/graphics/Tank.gif", 0, 0, tankWidth, tankHeight);
    Tank = malloc(imagesize(0, 0, tankWidth - 1, tankHeight - 1));
    getimage(0, 0, tankWidth - 1, tankHeight - 1, Tank);

   // Load Turret

    for (int i = 0; i < 8; ++i) {
        char path[64];

        // Masked
        sprintf(path, "C:/Users/Hans/Documents/coding/graphics/Turret/Masked%d.gif", i * 45);
        readimagefile(path, 0, 0, turretWidth - 1, turretHeight - 1);
        MaskedTurret[i] = malloc(imagesize(0, 0, turretWidth - 1, turretHeight - 1));
        getimage(0, 0, turretWidth - 1, turretHeight - 1, MaskedTurret[i]);

        // Gambar Asli
        sprintf(path, "C:/Users/Hans/Documents/coding/graphics/Turret/%d.gif", i * 45);
        readimagefile(path, 0, 0, turretWidth - 1, turretHeight - 1);
        Turret[i] = malloc(imagesize(0, 0, turretWidth - 1, turretHeight - 1));
        getimage(0, 0, turretWidth - 1, turretHeight - 1, Turret[i]);
    }



    game.run();
    return 0;
}
