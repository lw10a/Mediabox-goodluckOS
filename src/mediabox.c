/*
 * mediabox - goodluckOS (R36S clone, GA36-MB): MP3 player + PDF reader + MP4/MKV/AVI video player.
 * Arayuz dili: Ingilizce (varsayilan) / Turkce, ana menude Y ile degisir.
 * SDL2 + SDL2_ttf (cihazda var), dr_mp3 (MP3 cozucu), MuPDF (PDF cizici, statik).
 * Dosyalar tek tek listelenmez: Muzik / Kitaplar -> klasor klasor gezilir.
 */
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <mupdf/fitz.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <pthread.h>
#include <stdatomic.h>

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define W 640
#define H 480
#define MAXP 1024

/* Ayni dosya goodluckOS'un puppy uygulamasinda kullaniliyor. */
static const char *FONT_MAIN = "/usr/share/fonts/Inter_24pt-Medium.ttf";
static const char *FONT_ALT  = "/home/player/ports/kennel/assets/fonts/Inter-Regular.ttf";
static const char *PROG_DIR  = "/home/player/.config/mediabox";
static const char *PROG_FILE = "/home/player/.config/mediabox/progress.tsv";
static const char *SET_FILE  = "/home/player/.config/mediabox/settings.cfg";

/* /etc/profile.d/sdl_controller.sh icindeki eslemenin aynisi (ortamdan gelmezse diye). */
static const char *PAD_MAP =
    "1900c621474133362d4d422047616d00,GA36-MB Gamepad,dpleft:b15,rightx:a2,dpright:b16,"
    "rightshoulder:b5,dpdown:b14,righty:a3,leftshoulder:b4,y:b3,x:b2,b:b0,a:b1,dpup:b13,"
    "back:b8,leftstick:b11,start:b9,lefty:a1,guide:b8,lefttrigger:b6,righttrigger:b7,"
    "rightstick:b12,leftx:a0,platform:Linux";

/* ---------------------------------------------------------------- tipler */
typedef enum { T_DIR, T_MP3, T_PDF, T_VID, T_CAT_MUSIC, T_CAT_BOOKS, T_CAT_VIDEO, T_CAT_LANG, T_CAT_HELP } EType;
typedef struct { char name[256]; char path[MAXP]; EType type; } Entry;
typedef enum { S_HOME, S_BROWSE, S_PLAYER, S_READER, S_VIDEO, S_HELP } Screen;
typedef enum { K_UP, K_DOWN, K_LEFT, K_RIGHT, K_A, K_B, K_X, K_Y, K_L, K_R, K_L2, K_R2,
               K_START, K_SELECT } Key;

/* ---------------------------------------------------------------- global durum */
static SDL_Window *win;
static SDL_Renderer *R;
static TTF_Font *f_big, *f_mid, *f_small, *f_tiny;
static SDL_GameController *pad;
static int running = 1;
static int dirty = 1;
static Screen scr = S_HOME;

static const SDL_Color C_BG   = {18, 19, 24, 255};
static const SDL_Color C_TEXT = {236, 236, 242, 255};
static const SDL_Color C_DIM  = {140, 143, 156, 255};
static const SDL_Color C_ACC  = {88, 166, 255, 255};
static const SDL_Color C_SEL  = {36, 56, 88, 255};
static const SDL_Color C_RED  = {232, 92, 92, 255};
static const SDL_Color C_AMB  = {236, 178, 70, 255};

/* gezgin */
static Entry *ents; static int nent, ncap;
static int sel, topi;
static char cwd[MAXP];
static int cat;                 /* 0 muzik, 1 kitaplar, 2 videolar */
static char roots[2][MAXP]; static int nroots;
static char msg[160];

/* ---------------------------------------------------------------- dil (EN / TR) */
typedef enum { L_EN, L_TR } Lang;
static Lang lang = L_EN;            /* varsayilan Ingilizce; secim settings.cfg'de saklanir */
static int home_sel;

enum {
    X_MUSIC, X_BOOKS, X_VIDEOS, X_LANGUAGE, X_HELP,
    X_SUB_MUSIC, X_SUB_BOOKS, X_SUB_VIDEOS, X_SUB_HELP,
    X_INTERNAL, X_EXTERNAL, X_NO_FILES, X_NO_FOLDER, X_ERR_DIR, X_ITEMS,
    X_NOW_PLAYING, X_NOTHING, X_REPEAT, X_SHUFFLE, X_OFF, X_ON, X_LIST, X_ONE,
    X_ERR_MP3, X_ERR_AUDIO, X_LOADING, X_OPENING, X_ERR_PAGE, X_ERR_MUPDF, X_ERR_LOCKED, X_ERR_PDF,
    X_ZOOM_PAGE, X_ZOOM_WIDTH,
    X_ERR_VOPEN, X_ERR_VINFO, X_ERR_NOVID, X_ERR_VCODEC, X_ERR_VSIZE, X_ERR_VTEX, X_ERR_THREAD,
    X_BUFFERING, X_VOL, X_DROPPED, X_FIT, X_FILL,
    X_H_OPEN, X_H_BACK, X_H_EXIT, X_H_PAGE, X_H_NOWPLAYING, X_H_SEEK, X_H_TRACK, X_H_PAUSE, X_H_STOP, X_H_LANG,
    X_HELP_MUSIC_T, X_HELP_MUSIC_1, X_HELP_MUSIC_2,
    X_HELP_VIDEO_T, X_HELP_VIDEO_1, X_HELP_VIDEO_2,
    X_HELP_BOOK_T, X_HELP_BOOK_1, X_HELP_BOOK_2,
    X_HELP_DIR_T, X_HELP_DIR_1, X_HELP_DIR_2,
    X_HELP_LANG,
    X_N
};

static const char *const STR[X_N][2] = {
    [X_MUSIC]       = {"Music", "Müzik"},
    [X_BOOKS]       = {"Books (PDF)", "Kitaplar (PDF)"},
    [X_VIDEOS]      = {"Videos", "Videolar"},
    [X_LANGUAGE]    = {"Language", "Dil"},
    [X_HELP]        = {"Help", "Yardım"},
    [X_SUB_MUSIC]   = {"MP3 player", "MP3 oynatıcı"},
    [X_SUB_BOOKS]   = {"PDF reader", "PDF okuyucu"},
    [X_SUB_VIDEOS]  = {"MP4 / MKV / AVI player", "MP4 / MKV / AVI oynatıcı"},
    [X_SUB_HELP]    = {"Controls and folders", "Tuşlar ve klasörler"},
    [X_INTERNAL]    = {"Internal card", "Dahili kart"},
    [X_EXTERNAL]    = {"External card", "Harici kart"},
    [X_NO_FILES]    = {"No files in this folder", "Bu klasörde dosya yok"},
    [X_NO_FOLDER]   = {"Folder not found: %s", "Klasör yok: %s"},
    [X_ERR_DIR]     = {"Could not open folder", "Klasör açılamadı"},
    [X_ITEMS]       = {"%d items", "%d öğe"},
    [X_NOW_PLAYING] = {"Now playing", "Şimdi çalıyor"},
    [X_NOTHING]     = {"Nothing is playing", "Hiçbir şey çalmıyor"},
    [X_REPEAT]      = {"Repeat", "Tekrar"},
    [X_SHUFFLE]     = {"Shuffle", "Karışık"},
    [X_OFF]         = {"Off", "Kapalı"},
    [X_ON]          = {"On", "Açık"},
    [X_LIST]        = {"List", "Liste"},
    [X_ONE]         = {"One", "Tek"},
    [X_ERR_MP3]     = {"Could not open MP3", "MP3 açılamadı"},
    [X_ERR_AUDIO]   = {"Could not open audio device", "Ses aygıtı açılamadı"},
    [X_LOADING]     = {"Loading...", "Yükleniyor..."},
    [X_OPENING]     = {"Opening...", "Açılıyor..."},
    [X_ERR_PAGE]    = {"Could not render page", "Sayfa çizilemedi"},
    [X_ERR_MUPDF]   = {"Could not start MuPDF", "MuPDF başlatılamadı"},
    [X_ERR_LOCKED]  = {"Password-protected PDFs are not supported", "Şifreli PDF açılamaz"},
    [X_ERR_PDF]     = {"Could not open PDF", "PDF açılamadı"},
    [X_ZOOM_PAGE]   = {"Fit page", "Sayfaya sığdır"},
    [X_ZOOM_WIDTH]  = {"Fit width", "Genişliğe sığdır"},
    [X_ERR_VOPEN]   = {"Could not open video", "Video açılamadı"},
    [X_ERR_VINFO]   = {"Could not read video info", "Video bilgisi okunamadı"},
    [X_ERR_NOVID]   = {"No video stream found", "Video akışı yok"},
    [X_ERR_VCODEC]  = {"Unsupported video format (H.264 / MPEG-4 needed)", "Bu video biçimi desteklenmiyor (H.264 / MPEG-4 gerekir)"},
    [X_ERR_VSIZE]   = {"Invalid video size", "Geçersiz video boyutu"},
    [X_ERR_VTEX]    = {"Could not create video texture", "Video dokusu oluşturulamadı"},
    [X_ERR_THREAD]  = {"Could not start decoder thread", "Çözücü iş parçacığı başlatılamadı"},
    [X_BUFFERING]   = {"Buffering...", "Yükleniyor..."},
    [X_VOL]         = {"Vol", "Ses"},
    [X_DROPPED]     = {"Dropped", "Atlanan"},
    [X_FIT]         = {"Fit", "Sığdır"},
    [X_FILL]        = {"Fill", "Doldur"},
    [X_H_OPEN]      = {"Open", "Aç"},
    [X_H_BACK]      = {"Back", "Geri"},
    [X_H_EXIT]      = {"Exit", "Çıkış"},
    [X_H_PAGE]      = {"Page", "Sayfa"},
    [X_H_NOWPLAYING]= {"Now playing", "Çalan parça"},
    [X_H_SEEK]      = {"Seek", "Sar"},
    [X_H_TRACK]     = {"Track", "Parça"},
    [X_H_PAUSE]     = {"Pause", "Duraklat"},
    [X_H_STOP]      = {"Stop", "Durdur"},
    [X_H_LANG]      = {"Language", "Dil"},
    [X_HELP_MUSIC_T] = {"Music (MP3)", "Müzik (MP3)"},
    [X_HELP_MUSIC_1] = {"A pause    < > seek 10 s    L/R previous / next",
                        "A duraklat    < > 10 sn sar    L/R önceki / sonraki"},
    [X_HELP_MUSIC_2] = {"X shuffle    Y repeat    Select stop    B back (music keeps playing)",
                        "X karışık    Y tekrar    Select durdur    B geri (müzik çalmaya devam eder)"},
    [X_HELP_VIDEO_T] = {"Videos (MP4, MKV, AVI)", "Videolar (MP4, MKV, AVI)"},
    [X_HELP_VIDEO_1] = {"A pause    < > seek 10 s    L/R seek 1 min",
                        "A duraklat    < > 10 sn sar    L/R 1 dk sar"},
    [X_HELP_VIDEO_2] = {"^ v volume    X fit / fill screen    B or Select quit",
                        "^ v ses    X sığdır / doldur    B veya Select çık"},
    [X_HELP_BOOK_T]  = {"Books (PDF)", "Kitaplar (PDF)"},
    [X_HELP_BOOK_1]  = {"L/R page    L2/R2 10 pages    A zoom    X fit page",
                        "L/R sayfa    L2/R2 10 sayfa    A yakınlaştır    X sayfaya sığdır"},
    [X_HELP_BOOK_2]  = {"D-pad scrolls a zoomed page    B back (your page is saved)",
                        "Yön tuşları büyütülmüş sayfayı kaydırır    B geri (sayfa konumu kaydedilir)"},
    [X_HELP_DIR_T]   = {"Folders", "Klasörler"},
    [X_HELP_DIR_1]   = {"/home/player/music    /home/player/books    /home/player/videos",
                        "/home/player/music    /home/player/books    /home/player/videos"},
    [X_HELP_DIR_2]   = {"Same folder names on the external card: /media/external/...",
                        "Harici kartta aynı klasör adları: /media/external/..."},
    [X_HELP_LANG]    = {"Y on the main menu switches between English and Turkish",
                        "Ana menüde Y tuşu İngilizce ile Türkçe arasında geçiş yapar"},
};
#define TX(id) (STR[id][lang])

static void settings_load(void) {
    FILE *f = fopen(SET_FILE, "r");
    if (!f) return;
    char l[64];
    while (fgets(l, sizeof l, f)) {
        if (strncmp(l, "lang=tr", 7) == 0) lang = L_TR;
        else if (strncmp(l, "lang=en", 7) == 0) lang = L_EN;
    }
    fclose(f);
}

static void settings_save(void) {
    mkdir(PROG_DIR, 0755);
    FILE *f = fopen(SET_FILE, "w");
    if (!f) return;
    fprintf(f, "lang=%s\n", lang == L_TR ? "tr" : "en");
    fclose(f);
}

static void lang_toggle(void) {
    lang = lang == L_EN ? L_TR : L_EN;
    settings_save();
    dirty = 1;
}

/* ---------------------------------------------------------------- yardimcilar */
static void fill(int x, int y, int w, int h, SDL_Color c, int a) {
    if (w <= 0 || h <= 0) return;
    SDL_SetRenderDrawBlendMode(R, a < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(R, c.r, c.g, c.b, (Uint8)a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(R, &r);
}

static int text_w(TTF_Font *f, const char *s) {
    int w = 0, h = 0;
    if (f && s[0]) TTF_SizeUTF8(f, s, &w, &h);
    return w;
}

#define TC_N 128
typedef struct { TTF_Font *f; SDL_Color c; char s[200]; SDL_Texture *t; int w, h; Uint32 used; } TCache;
static TCache tcache[TC_N];
static Uint32 tc_clock;

static void tc_free(void) {
    for (int i = 0; i < TC_N; i++)
        if (tcache[i].t) { SDL_DestroyTexture(tcache[i].t); tcache[i].t = NULL; }
}

static void draw_text(TTF_Font *f, const char *s, int x, int y, SDL_Color c, int maxw) {
    if (!f || !s) return;
    char buf[512];
    snprintf(buf, sizeof buf, "%s", s);
    if (maxw > 0 && text_w(f, buf) > maxw) {
        size_t n = strlen(buf);
        while (n > 0) {
            n--;
            while (n > 0 && (buf[n] & 0xC0) == 0x80) n--;   /* UTF-8 karakter basina git */
            buf[n] = 0;
            char t[520];
            snprintf(t, sizeof t, "%s...", buf);
            if (text_w(f, t) <= maxw) { snprintf(buf, sizeof buf, "%s", t); break; }
        }
    }
    if (!buf[0]) return;
    int cacheable = strlen(buf) < sizeof tcache[0].s;
    if (cacheable) {                                      /* ayni yaziyi her karede yeniden cizme */
        for (int i = 0; i < TC_N; i++) {
            TCache *e = &tcache[i];
            if (e->t && e->f == f && e->c.r == c.r && e->c.g == c.g && e->c.b == c.b && strcmp(e->s, buf) == 0) {
                e->used = ++tc_clock;
                SDL_Rect d = {x, y, e->w, e->h};
                SDL_RenderCopy(R, e->t, NULL, &d);
                return;
            }
        }
    }
    SDL_Surface *sf = TTF_RenderUTF8_Blended(f, buf, c);
    if (!sf) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(R, sf);
    if (t) {
        SDL_Rect d = {x, y, sf->w, sf->h};
        SDL_RenderCopy(R, t, NULL, &d);
        if (cacheable) {
            TCache *v = &tcache[0];
            for (int i = 0; i < TC_N; i++) {
                if (!tcache[i].t) { v = &tcache[i]; break; }
                if (tcache[i].used < v->used) v = &tcache[i];
            }
            if (v->t) SDL_DestroyTexture(v->t);
            v->f = f; v->c = c;
            snprintf(v->s, sizeof v->s, "%s", buf);
            v->t = t; v->w = sf->w; v->h = sf->h; v->used = ++tc_clock;
        } else SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(sf);
}

static void draw_text_right(TTF_Font *f, const char *s, int xr, int y, SDL_Color c) {
    draw_text(f, s, xr - text_w(f, s), y, c, 0);
}

/* ---------------------------------------------------------------- cizim ilkelleri */
static const SDL_Color CC_MUSIC = {88, 166, 255, 255};
static const SDL_Color CC_BOOKS = {240, 112, 128, 255};
static const SDL_Color CC_VIDEO = {96, 200, 130, 255};
static const SDL_Color CC_LANG  = {236, 178, 70, 255};
static const SDL_Color CC_HELP  = {176, 140, 245, 255};
static const SDL_Color C_DARK   = {14, 16, 24, 255};
static const SDL_Color C_PILL   = {52, 58, 82, 255};
static const SDL_Color C_KEY    = {70, 77, 104, 255};
static const SDL_Color C_CARD   = {28, 31, 46, 255};

static SDL_Texture *bg_tex;

static void bg_init(void) {
    Uint32 px[H];
    for (int y = 0; y < H; y++) {
        double t = (double)y / (H - 1);
        int r = (int)(26 + (12 - 26) * t), g = (int)(30 + (13 - 30) * t), b = (int)(50 + (21 - 50) * t);
        px[y] = 0xFF000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
    }
    bg_tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, H);
    if (bg_tex) SDL_UpdateTexture(bg_tex, NULL, px, 4);
}

static void draw_bg(void) {
    if (bg_tex) SDL_RenderCopy(R, bg_tex, NULL, NULL);
    else fill(0, 0, W, H, C_BG, 255);
}

static void fill_rr(int x, int y, int w, int h, int r, SDL_Color c, int a) {
    if (w <= 0 || h <= 0) return;
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r <= 0) { fill(x, y, w, h, c, a); return; }
    fill(x, y + r, w, h - 2 * r, c, a);
    for (int i = 0; i < r; i++) {
        double dy = (double)(r - i) - 0.5;
        int dx = r - (int)(sqrt((double)r * r - dy * dy) + 0.5);
        fill(x + dx, y + i, w - 2 * dx, 1, c, a);
        fill(x + dx, y + h - 1 - i, w - 2 * dx, 1, c, a);
    }
}

static void fill_circle(int cx, int cy, int r, SDL_Color c, int a) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)(sqrt((double)r * r - (double)dy * dy) + 0.5);
        fill(cx - dx, cy + dy, 2 * dx + 1, 1, c, a);
    }
}

static void draw_text_center(TTF_Font *f, const char *s, int cx, int y, SDL_Color c, int maxw) {
    int w = text_w(f, s);
    if (maxw > 0 && w > maxw) w = maxw;
    draw_text(f, s, cx - w / 2, y, c, maxw);
}

/* ikonlar: cx,cy merkez, s yaricap/olcek */
static void icon_play(int cx, int cy, int s, SDL_Color c) {
    for (int i = -s; i <= s; i++) {
        int w = (int)((double)s * 1.5 * (1.0 - (double)abs(i) / (double)s)) + 1;
        fill(cx - s / 2, cy + i, w, 1, c, 255);
    }
}
static void icon_pause(int cx, int cy, int s, SDL_Color c) {
    int bw = s * 2 / 3 > 3 ? s * 2 / 3 : 3, gap = s / 2 + 2;
    fill_rr(cx - gap / 2 - bw, cy - s, bw, 2 * s, 2, c, 255);
    fill_rr(cx + gap / 2, cy - s, bw, 2 * s, 2, c, 255);
}
static void icon_next(int cx, int cy, int s, SDL_Color c) {
    for (int i = -s; i <= s; i++) {
        int w = (int)((double)s * 1.5 * (1.0 - (double)abs(i) / (double)s)) + 1;
        fill(cx - s, cy + i, w, 1, c, 255);
    }
    fill(cx + s - 2, cy - s, 3, 2 * s + 1, c, 255);
}
static void icon_prev(int cx, int cy, int s, SDL_Color c) {
    for (int i = -s; i <= s; i++) {
        int w = (int)((double)s * 1.5 * (1.0 - (double)abs(i) / (double)s)) + 1;
        fill(cx + s - w, cy + i, w, 1, c, 255);
    }
    fill(cx - s, cy - s, 3, 2 * s + 1, c, 255);
}
static void icon_note(int cx, int cy, int s, SDL_Color c) {        /* bagli sekizlik nota */
    int r = s * 33 / 100 > 3 ? s * 33 / 100 : 3, sw = s / 7 > 2 ? s / 7 : 2;
    int h1x = cx - s / 2, h1y = cy + s / 2, h2x = cx + s / 2, h2y = cy + s / 4;
    int x1 = h1x + r - sw, x2 = h2x + r - sw, top = cy - s;
    fill_circle(h1x, h1y, r, c, 255);
    fill_circle(h2x, h2y, r, c, 255);
    fill(x1, top, sw, h1y - top, c, 255);
    fill(x2, top, sw, h2y - top, c, 255);
    fill_rr(x1, top, x2 - x1 + sw, s * 3 / 10 > 3 ? s * 3 / 10 : 3, 2, c, 255);
}
static void icon_book(int cx, int cy, int s, SDL_Color c) {
    fill_rr(cx - s, cy - s * 4 / 5, s - 1, s * 8 / 5, 3, c, 255);
    fill_rr(cx + 2, cy - s * 4 / 5, s - 1, s * 8 / 5, 3, c, 255);
}
static void icon_video(int cx, int cy, int s, SDL_Color c, SDL_Color hole) {
    fill_rr(cx - s, cy - s * 7 / 10, 2 * s, s * 14 / 10, 4, c, 255);
    icon_play(cx, cy, s / 2 > 2 ? s / 2 : 2, hole);
}

static int pill_l(int x, int y, const char *s, SDL_Color bg, SDL_Color fg) {
    int w = text_w(f_tiny, s) + 18;
    fill_rr(x, y, w, 22, 11, bg, 255);
    draw_text(f_tiny, s, x + 9, y + 2, fg, 0);
    return x + w + 8;
}
static int pill(int xr, int y, const char *s, SDL_Color bg, SDL_Color fg) {
    int w = text_w(f_tiny, s) + 18;
    pill_l(xr - w, y, s, bg, fg);
    return xr - w - 8;
}

/* alt tus ipuclari: kv = {tus, etiket, tus, etiket, ...}, n cift. Sigmazsa ikinci satira gecer. */
static void draw_hints(const char *const *kv, int n) {
    int wid[10], rowi[10], rx[10], kw[10];
    if (n > 10) n = 10;
    int x = 20, row = 0;
    for (int i = 0; i < n; i++) {
        kw[i] = text_w(f_tiny, kv[2 * i]) + 14;
        if (kw[i] < 24) kw[i] = 24;
        wid[i] = kw[i] + 6 + text_w(f_tiny, kv[2 * i + 1]) + 18;
        if (x + wid[i] - 18 > W - 20 && x > 20 && row < 1) { row++; x = 20; }
        rowi[i] = row; rx[i] = x; x += wid[i];
    }
    int rows = row + 1;
    for (int i = 0; i < n; i++) {
        int y = H - 30 - (rows - 1 - rowi[i]) * 26;
        fill_rr(rx[i], y, kw[i], 22, 7, C_KEY, 255);
        draw_text_center(f_tiny, kv[2 * i], rx[i] + kw[i] / 2, y + 2, C_TEXT, 0);
        draw_text(f_tiny, kv[2 * i + 1], rx[i] + kw[i] + 6, y + 2, C_DIM, 0);
    }
}

static int has_ext(const char *name, const char *ext) {
    size_t n = strlen(name), e = strlen(ext);
    return n > e && strcasecmp(name + n - e, ext) == 0;
}

static int is_video_name(const char *n) {
    return has_ext(n, ".mp4") || has_ext(n, ".m4v") || has_ext(n, ".mov") ||
           has_ext(n, ".mkv") || has_ext(n, ".avi");
}

static int is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static const char *base_name(const char *p) {
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static void title_from_path(const char *p, char *out, size_t n) {
    snprintf(out, n, "%s", base_name(p));
    char *dot = strrchr(out, '.');
    if (dot && dot != out) *dot = 0;
}

static void fmt_time(char *out, size_t n, double sec) {
    int s = sec < 0 ? 0 : (int)sec;
    if (s >= 3600) snprintf(out, n, "%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
    else snprintf(out, n, "%d:%02d", s / 60, s % 60);
}

/* ---------------------------------------------------------------- PDF ilerleme kaydi */
typedef struct { char path[MAXP]; int page; } Prog;
static Prog progs[256]; static int nprogs;

static void prog_load(void) {
    FILE *f = fopen(PROG_FILE, "r");
    if (!f) return;
    char line[MAXP + 32];
    while (nprogs < 256 && fgets(line, sizeof line, f)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        char *p = tab + 1;
        size_t l = strlen(p);
        while (l && (p[l - 1] == '\n' || p[l - 1] == '\r')) p[--l] = 0;
        progs[nprogs].page = atoi(line);
        snprintf(progs[nprogs].path, MAXP, "%s", p);
        nprogs++;
    }
    fclose(f);
}

static int prog_get(const char *path) {
    for (int i = 0; i < nprogs; i++) if (strcmp(progs[i].path, path) == 0) return progs[i].page;
    return 0;
}

static void prog_set(const char *path, int page) {
    int idx = -1;
    for (int i = 0; i < nprogs; i++) if (strcmp(progs[i].path, path) == 0) { idx = i; break; }
    if (idx < 0) {
        if (nprogs == 256) { memmove(progs, progs + 1, sizeof(Prog) * 255); nprogs = 255; }
        idx = nprogs++;
        snprintf(progs[idx].path, MAXP, "%s", path);
    }
    progs[idx].page = page;
    mkdir(PROG_DIR, 0755);
    FILE *f = fopen(PROG_FILE, "w");
    if (!f) return;
    for (int i = 0; i < nprogs; i++) fprintf(f, "%d\t%s\n", progs[i].page, progs[i].path);
    fclose(f);
}

/* ---------------------------------------------------------------- gezgin */
static void ents_clear(void) { nent = 0; sel = 0; topi = 0; }

static Entry *ents_add(void) {
    if (nent == ncap) {
        ncap = ncap ? ncap * 2 : 128;
        ents = realloc(ents, sizeof(Entry) * ncap);
        if (!ents) { fprintf(stderr, "bellek yok\n"); exit(1); }
    }
    Entry *e = &ents[nent++];
    memset(e, 0, sizeof *e);
    return e;
}

static int cmp_ent(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    int dx = x->type == T_DIR, dy = y->type == T_DIR;
    if (dx != dy) return dy - dx;                 /* klasorler once */
    return strcasecmp(x->name, y->name);
}

static void show_home(void) {
    static const EType items[] = {T_CAT_MUSIC, T_CAT_BOOKS, T_CAT_VIDEO, T_CAT_LANG, T_CAT_HELP};
    ents_clear();
    for (int i = 0; i < 5; i++) { Entry *e = ents_add(); e->type = items[i]; }
    sel = home_sel < 5 ? home_sel : 0;
    cwd[0] = 0;
    scr = S_HOME;
}

static void enter_dir(const char *path) {
    ents_clear();
    snprintf(cwd, sizeof cwd, "%s", path);
    DIR *d = opendir(path);
    if (!d) { snprintf(msg, sizeof msg, "%s", TX(X_ERR_DIR)); return; }
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char full[MAXP];
        snprintf(full, sizeof full, "%s/%s", path, de->d_name);
        EType t;
        if (is_dir(full)) t = T_DIR;
        else if (cat == 0 && has_ext(de->d_name, ".mp3")) t = T_MP3;
        else if ((cat == 0 || cat == 2) && is_video_name(de->d_name)) t = T_VID;
        else if (cat == 1 && has_ext(de->d_name, ".pdf")) t = T_PDF;
        else continue;
        Entry *e = ents_add();
        snprintf(e->name, sizeof e->name, "%s", de->d_name);
        snprintf(e->path, sizeof e->path, "%s", full);
        e->type = t;
        if (t != T_DIR) {                       /* uzantiyi gizle */
            char *dot = strrchr(e->name, '.');
            if (dot) *dot = 0;
        }
    }
    closedir(d);
    qsort(ents, nent, sizeof(Entry), cmp_ent);
}

static void show_roots(void) {
    ents_clear();
    cwd[0] = 0;
    for (int i = 0; i < nroots; i++) {
        Entry *e = ents_add();
        e->name[0] = 0;                            /* etiket cizimde dile gore secilir */
        snprintf(e->path, sizeof e->path, "%s", roots[i]);
        e->type = T_DIR;
    }
}

static void open_category(int c) {
    static const char *rm[2] = {"/home/player/music", "/media/external/music"};
    static const char *rb[2] = {"/home/player/books", "/media/external/books"};
    static const char *rv[2] = {"/home/player/videos", "/media/external/videos"};
    cat = c;
    nroots = 0;
    msg[0] = 0;
    for (int i = 0; i < 2; i++) {
        const char *p = c == 0 ? rm[i] : c == 1 ? rb[i] : rv[i];
        if (is_dir(p)) snprintf(roots[nroots++], MAXP, "%s", p);
    }
    scr = S_BROWSE;
    if (nroots == 0) {
        ents_clear(); cwd[0] = 0;
        snprintf(msg, sizeof msg, TX(X_NO_FOLDER), c == 0 ? rm[0] : c == 1 ? rb[0] : rv[0]);
    } else if (nroots == 1) enter_dir(roots[0]);
    else show_roots();
}

static int is_root(const char *p) {
    for (int i = 0; i < nroots; i++) if (strcmp(roots[i], p) == 0) return 1;
    return 0;
}

static void select_path(const char *path) {
    for (int i = 0; i < nent; i++) if (strcmp(ents[i].path, path) == 0) { sel = i; return; }
}

static void go_up(void) {
    if (!cwd[0]) { show_home(); return; }
    if (is_root(cwd)) {
        if (nroots > 1) { char prev[MAXP]; snprintf(prev, sizeof prev, "%s", cwd); show_roots(); select_path(prev); }
        else show_home();
        return;
    }
    char prev[MAXP], parent[MAXP];
    snprintf(prev, sizeof prev, "%s", cwd);
    snprintf(parent, sizeof parent, "%s", cwd);
    char *s = strrchr(parent, '/');
    if (s && s != parent) *s = 0;
    enter_dir(parent);
    select_path(prev);
}

/* ---------------------------------------------------------------- ses (MP3) */
static SDL_AudioDeviceID adev;
static drmp3 mp3;
static int mp3_open;
static volatile int a_ended;
static int a_paused;
static drmp3_uint64 cur_frames, total_frames;
static Uint32 rate = 44100;

static char **pl; static int pl_n, pl_i;
static int repeat_mode = 1;     /* 0 kapali, 1 liste, 2 tek */
static int shuffle;
static char play_err[160];

static void audio_cb(void *ud, Uint8 *stream, int len) {
    (void)ud;
    int ch = (int)mp3.channels;
    drmp3_uint64 want = (drmp3_uint64)len / (2u * (drmp3_uint64)ch);
    drmp3_uint64 got = drmp3_read_pcm_frames_s16(&mp3, want, (drmp3_int16 *)stream);
    cur_frames += got;
    if (got < want) {
        memset(stream + got * 2u * (drmp3_uint64)ch, 0, (size_t)((want - got) * 2u * (drmp3_uint64)ch));
        a_ended = 1;
    }
}

static void track_close(void) {
    if (adev) { SDL_CloseAudioDevice(adev); adev = 0; }
    if (mp3_open) { drmp3_uninit(&mp3); mp3_open = 0; }
    cur_frames = total_frames = 0;
}

static int track_open(const char *path) {
    track_close();
    a_ended = 0; a_paused = 0; play_err[0] = 0;
    if (!drmp3_init_file(&mp3, path, NULL)) {
        snprintf(play_err, sizeof play_err, "%s", TX(X_ERR_MP3));
        return 0;
    }
    mp3_open = 1;
    total_frames = drmp3_get_pcm_frame_count(&mp3);
    drmp3_seek_to_pcm_frame(&mp3, 0);          /* sayimdan sonra basa don */
    cur_frames = 0;
    rate = mp3.sampleRate ? mp3.sampleRate : 44100;

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = (int)rate;
    want.format = AUDIO_S16SYS;
    want.channels = (Uint8)(mp3.channels > 2 ? 2 : mp3.channels);
    want.samples = 4096;
    want.callback = audio_cb;
    adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!adev) {
        snprintf(play_err, sizeof play_err, "%s", TX(X_ERR_AUDIO));
        drmp3_uninit(&mp3); mp3_open = 0;
        return 0;
    }
    SDL_PauseAudioDevice(adev, 0);
    return 1;
}

static void pl_free(void) {
    for (int i = 0; i < pl_n; i++) free(pl[i]);
    free(pl); pl = NULL; pl_n = 0; pl_i = 0;
}

static int playing(void) { return mp3_open && adev; }

static void play_index(int i) {
    pl_i = i;
    track_open(pl[i]);
    dirty = 1;
}

static void playlist_from_browser(int idx) {
    pl_free();
    int start = 0;
    for (int i = 0; i < nent; i++) {
        if (ents[i].type != T_MP3) continue;
        pl = realloc(pl, sizeof(char *) * (pl_n + 1));
        pl[pl_n] = strdup(ents[i].path);
        if (i == idx) start = pl_n;
        pl_n++;
    }
    if (pl_n) play_index(start);
}

static void track_step(int dir, int automatic) {
    if (pl_n == 0) return;
    int idx = pl_i;
    if (automatic && repeat_mode == 2) {
        /* ayni parca */
    } else if (shuffle && pl_n > 1) {
        do idx = rand() % pl_n; while (idx == pl_i);
    } else {
        idx += dir;
        if (idx >= pl_n) {
            if (automatic && repeat_mode == 0) { track_close(); dirty = 1; return; }
            idx = 0;
        }
        if (idx < 0) idx = pl_n - 1;
    }
    play_index(idx);
}

static double elapsed_sec(void) {
    if (!adev) return 0;
    SDL_LockAudioDevice(adev);
    drmp3_uint64 c = cur_frames;
    SDL_UnlockAudioDevice(adev);
    return (double)c / (double)rate;
}

static void seek_rel(double secs) {
    if (!adev) return;
    SDL_LockAudioDevice(adev);
    double cur = (double)cur_frames / (double)rate + secs;
    if (cur < 0) cur = 0;
    drmp3_uint64 t = (drmp3_uint64)(cur * (double)rate);
    if (total_frames && t >= total_frames) t = total_frames - 1;
    if (drmp3_seek_to_pcm_frame(&mp3, t)) cur_frames = t;
    SDL_UnlockAudioDevice(adev);
}

/* ---------------------------------------------------------------- PDF okuyucu */
static fz_context *ctx;
static fz_document *doc;
static int npages, page;
static SDL_Texture *ptex;
static int tw, th, px, py;
static int zoomi;       /* 0 sayfaya sigdir, 1 genislige sigdir, 2.. buyutme */
static const float zmult[] = {0.f, 1.0f, 1.5f, 2.0f, 3.0f};
static const char *zoom_name(int i) {
    static const char *pct[] = {"", "", "150%", "200%", "300%"};
    return i == 0 ? TX(X_ZOOM_PAGE) : i == 1 ? TX(X_ZOOM_WIDTH) : pct[i];
}
static char rd_path[MAXP];
static char rd_err[160];
static int pan_bottom_next;

static void reader_close(void) {
    if (ptex) { SDL_DestroyTexture(ptex); ptex = NULL; }
    if (doc) { fz_drop_document(ctx, doc); doc = NULL; }
    if (ctx) { fz_drop_context(ctx); ctx = NULL; }
    npages = 0;
}

static void draw_center_msg(const char *s) {
    draw_bg();
    int w = text_w(f_mid, s) + 60;
    if (w > W - 40) w = W - 40;
    fill_rr((W - w) / 2, H / 2 - 34, w, 60, 16, C_SEL, 255);
    draw_text_center(f_mid, s, W / 2, H / 2 - 20, C_TEXT, W - 100);
    SDL_RenderPresent(R);
}

static void reader_render(void) {
    if (ptex) { SDL_DestroyTexture(ptex); ptex = NULL; }
    rd_err[0] = 0;
    draw_center_msg(TX(X_LOADING));

    SDL_RendererInfo info;
    int maxtex = 2048;
    if (SDL_GetRendererInfo(R, &info) == 0 && info.max_texture_width > 0 && info.max_texture_height > 0)
        maxtex = info.max_texture_width < info.max_texture_height ? info.max_texture_width : info.max_texture_height;

    fz_page *pg = NULL;
    fz_pixmap *pix = NULL;
    SDL_Texture *tex = NULL;
    int w = 0, h = 0;
    fz_var(pg); fz_var(pix); fz_var(tex); fz_var(w); fz_var(h);

    fz_try(ctx) {
        pg = fz_load_page(ctx, doc, page);
        fz_rect b = fz_bound_page(ctx, pg);
        float bw = b.x1 - b.x0, bh = b.y1 - b.y0;
        if (bw < 1 || bh < 1) fz_throw(ctx, FZ_ERROR_GENERIC, "bos sayfa");
        float s = zoomi == 0 ? fminf((float)W / bw, (float)H / bh) : ((float)W / bw) * zmult[zoomi];
        float smax = fminf((float)maxtex / bw, (float)maxtex / bh);
        float smem = sqrtf(8.0e6f / (bw * bh));         /* en fazla ~8 megapiksel */
        if (s > smax) s = smax;
        if (s > smem) s = smem;
        pix = fz_new_pixmap_from_page(ctx, pg, fz_scale(s, s), fz_device_rgb(ctx), 0);
        w = fz_pixmap_width(ctx, pix);
        h = fz_pixmap_height(ctx, pix);
        tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, w, h);
        if (!tex) fz_throw(ctx, FZ_ERROR_GENERIC, "texture");
        SDL_UpdateTexture(tex, NULL, fz_pixmap_samples(ctx, pix), fz_pixmap_stride(ctx, pix));
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pix);
        fz_drop_page(ctx, pg);
    }
    fz_catch(ctx) {
        if (tex) { SDL_DestroyTexture(tex); tex = NULL; }
        snprintf(rd_err, sizeof rd_err, "%s", TX(X_ERR_PAGE));
    }
    ptex = tex; tw = w; th = h;
    px = tw > W ? (tw - W) / 2 : 0;
    py = 0;
    if (pan_bottom_next) { py = th > H ? th - H : 0; pan_bottom_next = 0; }
    prog_set(rd_path, page);
    dirty = 1;
}

static void reader_open(const char *path) {
    reader_close();
    snprintf(rd_path, sizeof rd_path, "%s", path);
    rd_err[0] = 0;
    scr = S_READER;
    draw_center_msg(TX(X_OPENING));
    ctx = fz_new_context(NULL, NULL, 48 * 1024 * 1024);
    if (!ctx) { snprintf(rd_err, sizeof rd_err, "%s", TX(X_ERR_MUPDF)); return; }
    int ok = 0, locked = 0;
    fz_var(ok); fz_var(locked);
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        doc = fz_open_document(ctx, path);
        if (fz_needs_password(ctx, doc)) { locked = 1; fz_throw(ctx, FZ_ERROR_GENERIC, "sifre"); }
        npages = fz_count_pages(ctx, doc);
        ok = npages > 0;
    }
    fz_catch(ctx) { ok = 0; }
    if (!ok) {
        snprintf(rd_err, sizeof rd_err, "%s", locked ? TX(X_ERR_LOCKED) : TX(X_ERR_PDF));
        return;
    }
    page = prog_get(path);
    if (page < 0 || page >= npages) page = 0;
    reader_render();
}

static void reader_goto(int p) {
    if (npages <= 0) return;
    if (p < 0) p = 0;
    if (p >= npages) p = npages - 1;
    if (p == page && ptex) return;
    page = p;
    reader_render();
}

static void reader_key(Key k) {
    if (!doc) { if (k == K_B || k == K_A) scr = S_BROWSE; if (k == K_B || k == K_A) reader_close(); return; }
    int step = 64;
    switch (k) {
    case K_B: reader_close(); scr = S_BROWSE; break;
    case K_L: reader_goto(page - 1); break;
    case K_R: reader_goto(page + 1); break;
    case K_L2: reader_goto(page - 10); break;
    case K_R2: reader_goto(page + 10); break;
    case K_A: zoomi = (zoomi + 1) % 5; reader_render(); break;
    case K_X: zoomi = 0; reader_render(); break;
    case K_LEFT:
        if (tw > W) { px -= step; if (px < 0) px = 0; } else reader_goto(page - 1);
        break;
    case K_RIGHT:
        if (tw > W) { px += step; if (px > tw - W) px = tw - W; } else reader_goto(page + 1);
        break;
    case K_UP:
        if (th > H) {
            if (py <= 0) { if (page > 0) { pan_bottom_next = 1; reader_goto(page - 1); } }
            else { py -= step; if (py < 0) py = 0; }
        } else reader_goto(page - 1);
        break;
    case K_DOWN:
        if (th > H) {
            if (py >= th - H) reader_goto(page + 1);
            else { py += step; if (py > th - H) py = th - H; }
        } else reader_goto(page + 1);
        break;
    default: break;
    }
    dirty = 1;
}

static void reader_draw(void) {
    fill(0, 0, W, H, (SDL_Color){0, 0, 0, 255}, 255);
    if (rd_err[0] || !doc) {
        draw_bg();
        const char *m = rd_err[0] ? rd_err : TX(X_ERR_PDF);
        fill_rr(60, 170, W - 120, 110, 16, C_SEL, 255);
        draw_text_center(f_mid, m, W / 2, 200, C_RED, W - 160);
        const char *k[2] = {"B", TX(X_H_BACK)};
        draw_hints(k, 1);
        return;
    }
    if (ptex) {
        SDL_Rect src, dst;
        src.w = tw > W ? W : tw;  src.h = th > H ? H : th;
        src.x = tw > W ? px : 0;  src.y = th > H ? py : 0;
        dst.w = src.w; dst.h = src.h;
        dst.x = (W - dst.w) / 2;  dst.y = (H - dst.h) / 2;
        SDL_RenderCopy(R, ptex, &src, &dst);
    }
    char lab[160];
    snprintf(lab, sizeof lab, "%d / %d   %s", page + 1, npages, zoom_name(zoomi));
    pill(W - 10, H - 34, lab, C_DARK, C_TEXT);
}

/* ---------------------------------------------------------------- video (MP4) */
/* Mimari: tek bir cozucu is parcacigi (demux + video + ses cozme). Ses, halka tamponla SDL ses
 * aygitina akar; ses saati ana saattir. Video kareleri kucuk bir kuyruga girer, ana dongu saate
 * gore gosterir, gecikmis kareleri atar. */
#define VQ_N 12
typedef struct { AVFrame *f; double pts; } VSlot;

static AVFormatContext *v_fmt;
static AVCodecContext *v_vctx, *v_actx;
static AVStream *v_vst, *v_ast;
static int v_vidx = -1, v_aidx = -1;
static struct SwrContext *v_swr;
static struct SwsContext *v_sws, *v_sws2;
static pthread_t v_th;
static int v_th_on;
static pthread_mutex_t v_mx = PTHREAD_MUTEX_INITIALIZER;
static atomic_int v_abort, v_seek_req, v_eof;
static double v_seek_to;
static unsigned v_seek_gen, v_seek_done;
static double v_discard_until, v_t0, v_last_vpts, v_frame_dur;

static VSlot v_q[VQ_N];
static int v_qr, v_qw, v_qn;

static Uint8 *v_ring;
static int v_ring_sz, v_ring_r, v_ring_w, v_ring_n;
static SDL_AudioDeviceID v_adev;
static int v_arate;
static double v_alat, v_clock_base;
static long long v_consumed;
static int v_vol = 100;
static Uint8 *v_abuf;
static int v_abuf_cap;

static int v_open_ok, v_paused, v_buffering, v_audio_done, v_fit;
static Uint32 v_buf_t0, v_wall_t0, v_osd_until, v_last_seek_tick, v_osd_tick;
static double v_buf_pos, v_wall_base, v_dur;
static int v_dropped, v_have_frame, v_osd_prev;
static char v_title[300];
static char v_err[160];

static SDL_Texture *v_tex;
static int v_tex_rgb, v_w, v_h;
static double v_sar = 1.0;
static Uint8 *v_rgb;

static void v_audio_cb(void *ud, Uint8 *stream, int len) {
    (void)ud;
    pthread_mutex_lock(&v_mx);
    int n = len < v_ring_n ? len : v_ring_n;
    if (n > 0) {
        int first = v_ring_sz - v_ring_r;
        if (first > n) first = n;
        memcpy(stream, v_ring + v_ring_r, (size_t)first);
        if (n > first) memcpy(stream + first, v_ring, (size_t)(n - first));
        v_ring_r = (v_ring_r + n) % v_ring_sz;
        v_ring_n -= n;
        v_consumed += n / 4;
    }
    pthread_mutex_unlock(&v_mx);
    if (n < len) memset(stream + n, 0, (size_t)(len - n));
    if (v_vol != 100) {
        Sint16 *p = (Sint16 *)stream;
        for (int i = 0; i < n / 2; i++) {
            int x = (p[i] * v_vol) / 100;
            p[i] = (Sint16)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
        }
    }
}

static int v_ring_put(const Uint8 *d, int len, int block) {
    while (len > 0) {
        pthread_mutex_lock(&v_mx);
        int space = v_ring_sz - v_ring_n;
        int n = len < space ? len : space;
        if (n > 0) {
            int first = v_ring_sz - v_ring_w;
            if (first > n) first = n;
            memcpy(v_ring + v_ring_w, d, (size_t)first);
            if (n > first) memcpy(v_ring, d + first, (size_t)(n - first));
            v_ring_w = (v_ring_w + n) % v_ring_sz;
            v_ring_n += n;
        }
        pthread_mutex_unlock(&v_mx);
        d += n; len -= n;
        if (len > 0) {
            if (!block) return 0;                      /* tampon dolu: artani birak */
            if (atomic_load(&v_abort) || atomic_load(&v_seek_req)) return 0;
            usleep(4000);
        }
    }
    return 1;
}

static void v_handle_audio(AVFrame *fr, int block) {
    if (!v_adev) return;
    double pts = fr->best_effort_timestamp == AV_NOPTS_VALUE ? -1.0
               : (double)fr->best_effort_timestamp * av_q2d(v_ast->time_base) - v_t0;
    if (pts >= 0 && fr->sample_rate > 0 &&
        pts + (double)fr->nb_samples / fr->sample_rate < v_discard_until) return;   /* sarma sonrasi atla */
    if (!v_swr) {
        AVChannelLayout out, in;
        memset(&out, 0, sizeof out); memset(&in, 0, sizeof in);
        av_channel_layout_default(&out, 2);
        if (fr->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
            av_channel_layout_default(&in, fr->ch_layout.nb_channels > 0 ? fr->ch_layout.nb_channels : 2);
        else av_channel_layout_copy(&in, &fr->ch_layout);
        int r = swr_alloc_set_opts2(&v_swr, &out, AV_SAMPLE_FMT_S16, v_arate, &in,
                                    (enum AVSampleFormat)fr->format, fr->sample_rate, 0, NULL);
        av_channel_layout_uninit(&in);
        av_channel_layout_uninit(&out);
        if (r < 0 || swr_init(v_swr) < 0) { swr_free(&v_swr); return; }
    }
    int maxo = swr_get_out_samples(v_swr, fr->nb_samples) + 64;
    if (maxo * 4 > v_abuf_cap) {
        free(v_abuf);
        v_abuf_cap = maxo * 4;
        v_abuf = malloc((size_t)v_abuf_cap);
        if (!v_abuf) { v_abuf_cap = 0; return; }
    }
    uint8_t *outp[1] = { v_abuf };
    int got = swr_convert(v_swr, outp, maxo, (const uint8_t **)fr->extended_data, fr->nb_samples);
    if (got > 0) v_ring_put(v_abuf, got * 4, block);
}

static void v_handle_video(AVFrame *fr, int block) {
    double pts;
    if (fr->best_effort_timestamp != AV_NOPTS_VALUE)
        pts = (double)fr->best_effort_timestamp * av_q2d(v_vst->time_base) - v_t0;
    else pts = v_last_vpts + v_frame_dur;
    v_last_vpts = pts;
    if (pts + 1e-3 < v_discard_until) return;
    for (;;) {
        pthread_mutex_lock(&v_mx);
        int n = v_qn;
        pthread_mutex_unlock(&v_mx);
        if (n < VQ_N) break;
        if (!block) return;                            /* kuyruk dolu: kareyi birak */
        if (atomic_load(&v_abort) || atomic_load(&v_seek_req)) return;
        usleep(3000);
    }
    AVFrame *out = NULL;
    if (fr->format == AV_PIX_FMT_YUV420P || fr->format == AV_PIX_FMT_YUVJ420P) {
        out = av_frame_clone(fr);
    } else {
        out = av_frame_alloc();
        if (!out) return;
        out->format = AV_PIX_FMT_YUV420P; out->width = fr->width; out->height = fr->height;
        if (av_frame_get_buffer(out, 32) < 0) { av_frame_free(&out); return; }
        v_sws = sws_getCachedContext(v_sws, fr->width, fr->height, (enum AVPixelFormat)fr->format,
                                     fr->width, fr->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                                     NULL, NULL, NULL);
        if (!v_sws) { av_frame_free(&out); return; }
        sws_scale(v_sws, (const uint8_t *const *)fr->data, fr->linesize, 0, fr->height,
                  out->data, out->linesize);
    }
    if (!out) return;
    pthread_mutex_lock(&v_mx);
    v_q[v_qw].f = out; v_q[v_qw].pts = pts;
    v_qw = (v_qw + 1) % VQ_N; v_qn++;
    pthread_mutex_unlock(&v_mx);
}

static void v_queue_clear_locked(void) {
    while (v_qn > 0) {
        av_frame_free(&v_q[v_qr].f);
        v_qr = (v_qr + 1) % VQ_N; v_qn--;
    }
}

/* Paket kuyruklari: demux hicbir zaman cozucuyu beklemez, ses ve goruntu paketleri ayri birikir.
 * (Tek kuyrukta video dolunca okuma duruyor, dosyada arkadaki ses paketlerine siranin gelmesi
 * gecikiyor ve ses tamponu bosaliyordu.) Yalnizca cozucu is parcacigi kullanir. */
#define PQ_N 4096
typedef struct { AVPacket *p[PQ_N]; int r, w, n; size_t bytes; } PQ;
static PQ v_vpq, v_apq;

static int pq_push(PQ *q, AVPacket *pk) {
    if (q->n >= PQ_N) return 0;
    AVPacket *c = av_packet_alloc();
    if (!c) return 0;
    q->bytes += (size_t)(pk->size > 0 ? pk->size : 0);
    av_packet_move_ref(c, pk);
    q->p[q->w] = c; q->w = (q->w + 1) % PQ_N; q->n++;
    return 1;
}
static AVPacket *pq_pop(PQ *q) {
    if (q->n == 0) return NULL;
    AVPacket *c = q->p[q->r];
    q->r = (q->r + 1) % PQ_N; q->n--;
    size_t sz = (size_t)(c->size > 0 ? c->size : 0);
    q->bytes = q->bytes > sz ? q->bytes - sz : 0;
    return c;
}
static void pq_clear(PQ *q) {
    AVPacket *c;
    while ((c = pq_pop(q)) != NULL) av_packet_free(&c);
    q->r = q->w = q->n = 0; q->bytes = 0;
}

static void v_drain(AVCodecContext *c, AVFrame *fr, int is_video) {
    if (!c) return;
    avcodec_send_packet(c, NULL);
    while (!atomic_load(&v_abort) && !atomic_load(&v_seek_req) && avcodec_receive_frame(c, fr) == 0) {
        if (is_video) v_handle_video(fr, 1); else v_handle_audio(fr, 1);
        av_frame_unref(fr);
    }
}

static void *v_thread(void *arg) {
    (void)arg;
    AVPacket *pk = av_packet_alloc();
    AVFrame *fr = av_frame_alloc();
    int demux_done = 0, drained = 0;
    while (!atomic_load(&v_abort)) {
        if (atomic_load(&v_seek_req)) {
            pthread_mutex_lock(&v_mx);
            double t = v_seek_to; unsigned gen = v_seek_gen;
            atomic_store(&v_seek_req, 0);
            pthread_mutex_unlock(&v_mx);
            if (t < 0) t = 0;
            av_seek_frame(v_fmt, -1, (int64_t)((t + v_t0) * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
            if (v_vctx) avcodec_flush_buffers(v_vctx);
            if (v_actx) avcodec_flush_buffers(v_actx);
            if (v_swr) swr_init(v_swr);
            pq_clear(&v_vpq); pq_clear(&v_apq);
            demux_done = 0; drained = 0;
            v_discard_until = t; v_last_vpts = t;
            atomic_store(&v_eof, 0);
            pthread_mutex_lock(&v_mx);
            v_queue_clear_locked();
            v_ring_r = v_ring_w = v_ring_n = 0;
            v_consumed = 0; v_clock_base = t;
            v_seek_done = gen;
            pthread_mutex_unlock(&v_mx);
            continue;
        }
        int did = 0;
        pthread_mutex_lock(&v_mx);
        int qn = v_qn, rfree = v_ring_sz - v_ring_n;
        pthread_mutex_unlock(&v_mx);

        /* 1) ses: ucuz, her turda birkac paket (sessizlik en kotu takilma) */
        for (int i = 0; i < 4 && v_actx && v_apq.n > 0 && rfree >= 16384 &&
                        !atomic_load(&v_seek_req); i++) {
            AVPacket *p = pq_pop(&v_apq);
            if (avcodec_send_packet(v_actx, p) == 0)
                while (avcodec_receive_frame(v_actx, fr) == 0) {
                    v_handle_audio(fr, 0);
                    av_frame_unref(fr);
                }
            av_packet_free(&p);
            pthread_mutex_lock(&v_mx);
            rfree = v_ring_sz - v_ring_n;
            pthread_mutex_unlock(&v_mx);
            did = 1;
        }
        /* 2) video: kuyrukta yer varsa bir paket coz */
        if (v_vctx && v_vpq.n > 0 && qn < VQ_N - 4 && !atomic_load(&v_seek_req)) {
            AVPacket *p = pq_pop(&v_vpq);
            if (avcodec_send_packet(v_vctx, p) == 0)
                while (avcodec_receive_frame(v_vctx, fr) == 0) {
                    v_handle_video(fr, 0);
                    av_frame_unref(fr);
                }
            av_packet_free(&p);
            did = 1;
        }
        /* 3) demux: kuyruklar sinirin altindaysa birkac paket oku */
        for (int i = 0; i < 4 && !demux_done && v_vpq.n < 3000 && v_vpq.bytes < (16u << 20) &&
                        v_apq.n < 3000 && v_apq.bytes < (4u << 20); i++) {
            int r = av_read_frame(v_fmt, pk);
            if (r < 0) { demux_done = 1; break; }
            if (pk->stream_index == v_vidx && v_vctx) { if (!pq_push(&v_vpq, pk)) av_packet_unref(pk); }
            else if (pk->stream_index == v_aidx && v_actx) { if (!pq_push(&v_apq, pk)) av_packet_unref(pk); }
            else av_packet_unref(pk);
            did = 1;
        }
        /* 4) dosya bitti ve paketler tukendi: cozuculeri bosalt */
        if (demux_done && !drained && v_vpq.n == 0 && v_apq.n == 0) {
            v_drain(v_vctx, fr, 1);
            v_drain(v_actx, fr, 0);
            if (!atomic_load(&v_seek_req) && !atomic_load(&v_abort)) { drained = 1; atomic_store(&v_eof, 1); }
            did = 1;
        }
        if (!did) usleep(2000);
    }
    av_frame_free(&fr);
    av_packet_free(&pk);
    return NULL;
}

static void v_close(void) {
    if (v_th_on) { atomic_store(&v_abort, 1); pthread_join(v_th, NULL); v_th_on = 0; }
    if (v_adev) { SDL_CloseAudioDevice(v_adev); v_adev = 0; }
    pq_clear(&v_vpq); pq_clear(&v_apq);
    pthread_mutex_lock(&v_mx);
    v_queue_clear_locked();
    pthread_mutex_unlock(&v_mx);
    if (v_swr) swr_free(&v_swr);
    if (v_sws) { sws_freeContext(v_sws); v_sws = NULL; }
    if (v_sws2) { sws_freeContext(v_sws2); v_sws2 = NULL; }
    if (v_vctx) avcodec_free_context(&v_vctx);
    if (v_actx) avcodec_free_context(&v_actx);
    if (v_fmt) avformat_close_input(&v_fmt);
    if (v_tex) { SDL_DestroyTexture(v_tex); v_tex = NULL; }
    free(v_ring); v_ring = NULL; v_ring_sz = 0;
    free(v_abuf); v_abuf = NULL; v_abuf_cap = 0;
    free(v_rgb); v_rgb = NULL;
    v_vidx = v_aidx = -1;
    v_open_ok = 0; v_have_frame = 0;
}

static AVCodecContext *v_open_codec(AVStream *st, int video) {
    const AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) return NULL;
    AVCodecContext *c = avcodec_alloc_context3(dec);
    if (!c) return NULL;
    if (avcodec_parameters_to_context(c, st->codecpar) < 0) { avcodec_free_context(&c); return NULL; }
    if (video) {
        c->thread_count = 2;
        c->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        c->skip_loop_filter = AVDISCARD_NONREF;        /* zayif CPU icin hiz */
        c->flags2 |= AV_CODEC_FLAG2_FAST;
    }
    if (avcodec_open2(c, dec, NULL) < 0) { avcodec_free_context(&c); return NULL; }
    return c;
}

static void v_start_buffering(double pos) {
    v_buffering = 1;
    v_buf_t0 = SDL_GetTicks();
    v_buf_pos = pos;
    v_wall_base = pos;
    v_audio_done = 0;
    if (v_adev) SDL_PauseAudioDevice(v_adev, 1);
}

static int v_open(const char *path) {
    track_close(); pl_free();                     /* calan muzigi durdur */
    v_close();
    v_err[0] = 0;
    scr = S_VIDEO;
    draw_center_msg(TX(X_OPENING));
    title_from_path(path, v_title, sizeof v_title);
    atomic_store(&v_abort, 0); atomic_store(&v_seek_req, 0); atomic_store(&v_eof, 0);
    v_seek_gen = v_seek_done = 0; v_discard_until = 0; v_last_vpts = 0;
    v_consumed = 0; v_clock_base = 0; v_dropped = 0; v_paused = 0; v_audio_done = 0;
    v_osd_until = SDL_GetTicks() + 3000; v_last_seek_tick = 0; v_osd_prev = 1;
    v_ring_r = v_ring_w = v_ring_n = 0;

    if (avformat_open_input(&v_fmt, path, NULL, NULL) < 0) { snprintf(v_err, sizeof v_err, "%s", TX(X_ERR_VOPEN)); return 0; }
    if (avformat_find_stream_info(v_fmt, NULL) < 0) { snprintf(v_err, sizeof v_err, "%s", TX(X_ERR_VINFO)); goto fail; }
    v_vidx = av_find_best_stream(v_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (v_vidx < 0) { snprintf(v_err, sizeof v_err, "%s", TX(X_ERR_NOVID)); goto fail; }
    v_vst = v_fmt->streams[v_vidx];
    v_vctx = v_open_codec(v_vst, 1);
    if (!v_vctx) { snprintf(v_err, sizeof v_err, "%s", TX(X_ERR_VCODEC)); goto fail; }
    v_aidx = av_find_best_stream(v_fmt, AVMEDIA_TYPE_AUDIO, -1, v_vidx, NULL, 0);
    if (v_aidx >= 0) {
        v_ast = v_fmt->streams[v_aidx];
        v_actx = v_open_codec(v_ast, 0);
        if (!v_actx) v_aidx = -1;
    }

    v_t0 = v_fmt->start_time != AV_NOPTS_VALUE ? (double)v_fmt->start_time / AV_TIME_BASE : 0.0;
    v_dur = v_fmt->duration != AV_NOPTS_VALUE ? (double)v_fmt->duration / AV_TIME_BASE : 0.0;
    AVRational fr = av_guess_frame_rate(v_fmt, v_vst, NULL);
    v_frame_dur = fr.num > 0 && fr.den > 0 ? (double)fr.den / fr.num : 1.0 / 25.0;
    v_w = v_vctx->width; v_h = v_vctx->height;
    if (v_w <= 0 || v_h <= 0) { snprintf(v_err, sizeof v_err, "%s", TX(X_ERR_VSIZE)); goto fail; }
    AVRational sar = av_guess_sample_aspect_ratio(v_fmt, v_vst, NULL);
    v_sar = sar.num > 0 && sar.den > 0 ? (double)sar.num / sar.den : 1.0;

    if (v_actx) {
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = v_actx->sample_rate > 0 ? v_actx->sample_rate : 44100;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = 2048;
        want.callback = v_audio_cb;
        v_adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (v_adev) {
            v_arate = have.freq;
            v_alat = (double)have.samples / (double)have.freq;
            v_ring_sz = (v_arate * 6) & ~3;           /* ~1,5 sn tampon */
            v_ring = malloc((size_t)v_ring_sz);
            if (!v_ring) { SDL_CloseAudioDevice(v_adev); v_adev = 0; }
        }
    }

    if (!v_adev && v_actx) { avcodec_free_context(&v_actx); v_aidx = -1; }

    v_tex_rgb = 0;
    v_tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, v_w, v_h);
    if (!v_tex) {                                      /* YUV dokusu yoksa RGB'ye don */
        v_tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, v_w, v_h);
        v_tex_rgb = 1;
        v_rgb = malloc((size_t)v_w * (size_t)v_h * 3u);
        if (!v_tex || !v_rgb) { snprintf(v_err, sizeof v_err, "%s", TX(X_ERR_VTEX)); goto fail; }
    }

    v_have_frame = 0; v_open_ok = 1;
    v_start_buffering(0.0);
    if (pthread_create(&v_th, NULL, v_thread, NULL) != 0) { snprintf(v_err, sizeof v_err, "%s", TX(X_ERR_THREAD)); v_open_ok = 0; goto fail; }
    v_th_on = 1;
    return 1;
fail:
    v_close();
    return 0;
}

static double v_audio_clock(void) {
    pthread_mutex_lock(&v_mx);
    double c = v_clock_base + (double)v_consumed / (double)v_arate;
    pthread_mutex_unlock(&v_mx);
    c -= v_alat;
    return c < 0 ? 0 : c;
}

static int v_wall(void) { return !v_adev || v_audio_done; }

static double v_get_clock(void) {
    if (v_buffering) return v_buf_pos;
    if (!v_wall()) return v_audio_clock();
    if (v_paused) return v_wall_base;
    return v_wall_base + (double)(SDL_GetTicks() - v_wall_t0) / 1000.0;
}

static void v_set_paused(int p) {
    if (p == v_paused) return;
    if (v_wall() && !v_buffering) { if (p) v_wall_base = v_get_clock(); else v_wall_t0 = SDL_GetTicks(); }
    v_paused = p;
    if (v_adev) SDL_PauseAudioDevice(v_adev, v_paused || v_buffering);
}

static void v_seek_rel(double secs) {
    Uint32 now = SDL_GetTicks();
    if (now - v_last_seek_tick < 300) return;          /* tus tekrarini sinirla */
    v_last_seek_tick = now;
    double t = v_get_clock() + secs;
    if (t < 0) t = 0;
    if (v_dur > 2 && t > v_dur - 1) t = v_dur - 1;
    pthread_mutex_lock(&v_mx);
    v_seek_to = t; v_seek_gen++;
    pthread_mutex_unlock(&v_mx);
    atomic_store(&v_seek_req, 1);
    v_start_buffering(t);
}

static void v_show(AVFrame *f) {
    if (!v_tex_rgb) {
        SDL_UpdateYUVTexture(v_tex, NULL, f->data[0], f->linesize[0], f->data[1], f->linesize[1],
                             f->data[2], f->linesize[2]);
    } else {
        v_sws2 = sws_getCachedContext(v_sws2, v_w, v_h, AV_PIX_FMT_YUV420P, v_w, v_h, AV_PIX_FMT_RGB24,
                                      SWS_FAST_BILINEAR, NULL, NULL, NULL);
        if (!v_sws2) return;
        uint8_t *dst[1] = { v_rgb };
        int dls[1] = { v_w * 3 };
        sws_scale(v_sws2, (const uint8_t *const *)f->data, f->linesize, 0, v_h, dst, dls);
        SDL_UpdateTexture(v_tex, NULL, v_rgb, v_w * 3);
    }
    v_have_frame = 1;
    dirty = 1;
}

static void v_tick(void) {
    if (!v_open_ok) return;
    Uint32 now = SDL_GetTicks();
    pthread_mutex_lock(&v_mx);
    int qn = v_qn, rn = v_ring_n;
    int synced = v_seek_done == v_seek_gen;
    pthread_mutex_unlock(&v_mx);
    int eof = atomic_load(&v_eof);

    if (v_buffering) {
        int ready = synced && !atomic_load(&v_seek_req) &&
                    (eof || (qn >= 3 && (!v_adev || rn >= v_arate)));
        if (ready || now - v_buf_t0 > 3000) {
            v_buffering = 0;
            if (v_wall()) v_wall_t0 = now;
            if (v_adev) SDL_PauseAudioDevice(v_adev, v_paused);
            dirty = 1;
        }
    } else if (v_adev && !v_audio_done && eof && rn == 0) {
        v_wall_base = v_audio_clock();                 /* ses bitti, saat duvar saatine gecer */
        v_wall_t0 = now;
        v_audio_done = 1;
    }

    if (!v_buffering && !v_paused) {
        double clk = v_get_clock();
        AVFrame *show = NULL;
        for (;;) {
            pthread_mutex_lock(&v_mx);
            if (v_qn == 0 || v_q[v_qr].pts > clk + 0.004) { pthread_mutex_unlock(&v_mx); break; }
            AVFrame *f = v_q[v_qr].f;
            v_qr = (v_qr + 1) % VQ_N; v_qn--;
            pthread_mutex_unlock(&v_mx);
            if (show) { av_frame_free(&show); v_dropped++; }
            show = f;
        }
        if (show) { v_show(show); av_frame_free(&show); }

        pthread_mutex_lock(&v_mx);
        int done = eof && v_qn == 0 && (!v_adev || v_ring_n == 0);
        pthread_mutex_unlock(&v_mx);
        if (done) {
            v_close();
            scr = S_BROWSE;
            dirty = 1;
            return;
        }
    }

    int osd = now < v_osd_until || v_paused || v_buffering;
    if (osd != v_osd_prev) { v_osd_prev = osd; dirty = 1; }
    if (osd && now - v_osd_tick >= 250) { v_osd_tick = now; dirty = 1; }
}

static void v_key(Key k) {
    if (!v_open_ok) { if (k == K_A || k == K_B) { v_close(); scr = S_BROWSE; } return; }
    v_osd_until = SDL_GetTicks() + 3000;
    switch (k) {
    case K_A: case K_START: v_set_paused(!v_paused); break;
    case K_LEFT:  v_seek_rel(-10); break;
    case K_RIGHT: v_seek_rel(10); break;
    case K_L: v_seek_rel(-60); break;
    case K_R: v_seek_rel(60); break;
    case K_UP:   v_vol += 10; if (v_vol > 200) v_vol = 200; break;
    case K_DOWN: v_vol -= 10; if (v_vol < 0) v_vol = 0; break;
    case K_X: v_fit = !v_fit; break;
    case K_B: case K_SELECT: v_close(); scr = S_BROWSE; break;
    default: break;
    }
}

static void v_draw(void) {
    fill(0, 0, W, H, (SDL_Color){0, 0, 0, 255}, 255);
    if (!v_open_ok) {
        draw_bg();
        const char *m = v_err[0] ? v_err : TX(X_ERR_VOPEN);
        fill_rr(60, 170, W - 120, 110, 16, C_SEL, 255);
        draw_text_center(f_mid, m, W / 2, 200, C_RED, W - 160);
        const char *k[2] = {"B", TX(X_H_BACK)};
        draw_hints(k, 1);
        return;
    }
    if (v_tex && v_have_frame) {
        double dar = (double)v_w * v_sar / (double)v_h;
        if (!v_fit) {                                   /* sigdir (siyah bantlar) */
            int dw = W, dh = (int)(W / dar);
            if (dh > H) { dh = H; dw = (int)(H * dar); }
            SDL_Rect dst = {(W - dw) / 2, (H - dh) / 2, dw, dh};
            SDL_RenderCopy(R, v_tex, NULL, &dst);
        } else {                                        /* doldur (kenarlar kirpilir) */
            double ta = (double)W / H;
            SDL_Rect src;
            int sw = (int)((double)v_h * ta / v_sar);
            if (sw <= v_w) { src.w = sw; src.h = v_h; src.x = (v_w - sw) / 2; src.y = 0; }
            else {
                int sh = (int)((double)v_w * v_sar / ta);
                src.w = v_w; src.h = sh; src.x = 0; src.y = (v_h - sh) / 2;
            }
            SDL_RenderCopy(R, v_tex, &src, NULL);
        }
    }
    Uint32 now = SDL_GetTicks();
    if (v_buffering) {
        const char *m = TX(X_BUFFERING);
        int w = text_w(f_small, m) + 44;
        fill_rr((W - w) / 2, H / 2 - 24, w, 40, 20, C_DARK, 190);
        draw_text_center(f_small, m, W / 2, H / 2 - 15, C_TEXT, W - 80);
    } else if (v_paused) {
        fill_circle(W / 2, H / 2 - 20, 42, C_DARK, 150);
        icon_pause(W / 2, H / 2 - 20, 16, C_TEXT);
    }
    if (now < v_osd_until || v_paused || v_buffering) {
        fill_rr(14, H - 100, W - 28, 88, 14, C_DARK, 190);
        draw_text(f_small, v_title, 30, H - 94, C_TEXT, W - 60);
        double cur = v_get_clock();
        if (v_dur > 0 && cur > v_dur) cur = v_dur;
        int bx = 30, bw = W - 60, by = H - 62;
        fill_rr(bx, by, bw, 8, 4, C_PILL, 255);
        if (v_dur > 0) {
            int fw = (int)(bw * (cur / v_dur));
            if (fw > 0) fill_rr(bx, by, fw < 8 ? 8 : fw, 8, 4, CC_VIDEO, 255);
            fill_circle(bx + fw, by + 4, 7, C_TEXT, 255);
        }
        if (v_paused || v_buffering) icon_play(38, H - 34, 8, CC_VIDEO);
        else icon_pause(38, H - 34, 8, CC_VIDEO);
        char a[32], b[32], tm[80], p[64];
        fmt_time(a, sizeof a, cur);
        fmt_time(b, sizeof b, v_dur);
        snprintf(tm, sizeof tm, v_dur > 0 ? "%s / %s" : "%s", a, b);
        draw_text(f_small, tm, 58, H - 45, C_TEXT, 220);
        int xr = W - 30;
        snprintf(p, sizeof p, "%s: %d", TX(X_DROPPED), v_dropped);
        xr = pill(xr, H - 44, p, C_PILL, C_DIM);
        xr = pill(xr, H - 44, v_fit ? TX(X_FILL) : TX(X_FIT), C_PILL, C_TEXT);
        snprintf(p, sizeof p, "%s %d%%", TX(X_VOL), v_vol);
        pill(xr, H - 44, p, C_PILL, C_TEXT);
    }
}

/* ---------------------------------------------------------------- cizim: ekranlar */
#define ROW_H 40
#define HOME_H 66
#define LIST_Y 62

static SDL_Color cat_color(void) { return cat == 0 ? CC_MUSIC : cat == 1 ? CC_BOOKS : CC_VIDEO; }

static void draw_badge(int x, int y, EType t) {
    switch (t) {
    case T_DIR:
        fill_rr(x, y + 3, 16, 9, 3, (SDL_Color){214, 160, 60, 255}, 255);
        fill_rr(x, y + 7, 34, 19, 5, C_AMB, 255);
        break;
    case T_MP3:
        fill_rr(x, y + 3, 34, 22, 6, CC_MUSIC, 255);
        draw_text_center(f_tiny, "MP3", x + 17, y + 5, C_DARK, 0);
        break;
    case T_PDF:
        fill_rr(x, y + 3, 34, 22, 6, CC_BOOKS, 255);
        draw_text_center(f_tiny, "PDF", x + 17, y + 5, C_DARK, 0);
        break;
    case T_VID:
        fill_rr(x, y + 3, 34, 22, 6, CC_VIDEO, 255);
        draw_text_center(f_tiny, "VID", x + 17, y + 5, C_DARK, 0);
        break;
    default:
        fill_rr(x, y + 3, 34, 22, 6, C_ACC, 255);
        break;
    }
}

static int np_visible(void) { return playing() && (scr == S_HOME || scr == S_BROWSE); }

static int rows_visible(void) {
    if (scr == S_HOME) return nent > 0 ? nent : 1;
    int bottom = np_visible() ? 400 : 440;
    return (bottom - LIST_Y) / ROW_H;
}

static void list_clamp(void) {
    int rv = rows_visible();
    if (nent == 0) { sel = topi = 0; return; }
    if (sel < 0) sel = 0;
    if (sel >= nent) sel = nent - 1;
    if (sel < topi) topi = sel;
    if (sel >= topi + rv) topi = sel - rv + 1;
    if (topi < 0) topi = 0;
}

static void draw_header(const char *title, SDL_Color col) {
    fill_rr(20, 14, 6, 30, 3, col, 255);
    draw_text(f_big, title, 38, 10, C_TEXT, W - 230);
    fill(20, 52, W - 40, 1, C_DIM, 70);
}

static void draw_now_playing_bar(void) {
    fill_rr(10, 406, W - 20, 36, 12, C_SEL, 255);
    char t[300];
    title_from_path(pl[pl_i], t, sizeof t);
    if (a_paused) icon_play(30, 424, 7, C_ACC); else icon_pause(30, 424, 7, C_ACC);
    draw_text(f_small, t, 48, 413, C_TEXT, 440);
    char a[32];
    fmt_time(a, sizeof a, elapsed_sec());
    draw_text_right(f_small, a, W - 22, 413, C_DIM);
    double tot = total_frames ? (double)total_frames / (double)rate : 0;
    if (tot > 0) {
        double fr = elapsed_sec() / tot;
        if (fr > 1) fr = 1;
        fill(24, 437, (int)((W - 48) * fr), 2, C_ACC, 255);
    }
}

static void home_info(EType t, const char **title, const char **sub, SDL_Color *col) {
    switch (t) {
    case T_CAT_MUSIC: *title = TX(X_MUSIC);  *sub = TX(X_SUB_MUSIC);  *col = CC_MUSIC; break;
    case T_CAT_BOOKS: *title = TX(X_BOOKS);  *sub = TX(X_SUB_BOOKS);  *col = CC_BOOKS; break;
    case T_CAT_VIDEO: *title = TX(X_VIDEOS); *sub = TX(X_SUB_VIDEOS); *col = CC_VIDEO; break;
    case T_CAT_LANG:  *title = TX(X_LANGUAGE); *sub = lang == L_EN ? "English" : "Türkçe"; *col = CC_LANG; break;
    default:          *title = TX(X_HELP);   *sub = TX(X_SUB_HELP);   *col = CC_HELP; break;
    }
}

static void draw_home(void) {
    draw_bg();
    fill_rr(20, 14, 6, 30, 3, C_ACC, 255);
    draw_text(f_big, "mediabox", 38, 10, C_TEXT, 300);
    draw_text_right(f_small, "MP3  ·  MP4  ·  PDF", W - 24, 20, C_DIM);
    fill(20, 52, W - 40, 1, C_DIM, 70);
    for (int i = 0; i < nent; i++) {
        int x = 20, y = LIST_Y + i * HOME_H, w = W - 40, h = 60, on = i == sel;
        const char *title, *sub;
        SDL_Color col;
        home_info(ents[i].type, &title, &sub, &col);
        if (on) {
            fill_rr(x - 2, y - 2, w + 4, h + 4, 16, col, 255);
            fill_rr(x, y, w, h, 14, (SDL_Color){34, 48, 80, 255}, 255);
        } else {
            fill_rr(x, y, w, h, 14, C_CARD, 255);
        }
        fill_rr(x + 10, y + 8, 44, 44, 12, col, 255);
        int cx = x + 32, cy = y + 30;
        switch (ents[i].type) {
        case T_CAT_MUSIC: icon_note(cx, cy + 1, 14, C_DARK); break;
        case T_CAT_BOOKS: icon_book(cx, cy, 13, C_DARK); break;
        case T_CAT_VIDEO: icon_video(cx, cy, 15, C_DARK, col); break;
        case T_CAT_LANG:  draw_text_center(f_small, lang == L_EN ? "EN" : "TR", cx, cy - 12, C_DARK, 0); break;
        default:          draw_text_center(f_mid, "?", cx, cy - 15, C_DARK, 0); break;
        }
        draw_text(f_mid, title, x + 70, y + 6, C_TEXT, w - 150);
        draw_text(f_tiny, sub, x + 70, y + 35, C_DIM, w - 150);
        if (ents[i].type == T_CAT_LANG) pill(x + w - 14, y + 19, lang == L_EN ? "EN  >  TR" : "TR  >  EN", C_PILL, C_TEXT);
        else if (on) icon_play(x + w - 26, y + 30, 7, col);
    }
    if (np_visible()) draw_now_playing_bar();
    const char *k[8] = {"A", TX(X_H_OPEN), "Y", TX(X_H_LANG), "B", TX(X_H_EXIT), "Start", TX(X_H_NOWPLAYING)};
    draw_hints(k, pl_n ? 4 : 3);
}

static void draw_browse(void) {
    draw_bg();
    SDL_Color cc = cat_color();
    char title[300];
    if (!cwd[0]) snprintf(title, sizeof title, "%s", cat == 0 ? TX(X_MUSIC) : cat == 1 ? TX(X_BOOKS) : TX(X_VIDEOS));
    else snprintf(title, sizeof title, "%s", base_name(cwd));
    draw_header(title, cc);
    if (nent > 0) {
        char n[48];
        snprintf(n, sizeof n, TX(X_ITEMS), nent);
        draw_text_right(f_small, n, W - 24, 20, C_DIM);
    }
    list_clamp();
    int rv = rows_visible();
    if (nent == 0) {
        draw_text(f_mid, msg[0] ? msg : TX(X_NO_FILES), 28, LIST_Y + 24, C_DIM, W - 56);
    }
    for (int i = 0; i < rv && topi + i < nent; i++) {
        int idx = topi + i;
        int y = LIST_Y + i * ROW_H;
        if (idx == sel) {
            fill_rr(14, y, W - 36, ROW_H - 4, 10, C_SEL, 255);
            fill_rr(14, y + 7, 4, ROW_H - 18, 2, cc, 255);
        }
        draw_badge(26, y + 3, ents[idx].type);
        const char *label = ents[idx].name;
        if (!cwd[0] && ents[idx].type == T_DIR && nroots > 1)
            label = strncmp(ents[idx].path, "/media/external", 15) == 0 ? TX(X_EXTERNAL) : TX(X_INTERNAL);
        draw_text(f_mid, label, 74, y + 4, C_TEXT, W - 120);
    }
    if (nent > rv) {                                  /* kaydirma cubugu */
        int th_ = rv * ROW_H - 4, bh = th_ * rv / nent;
        if (bh < 18) bh = 18;
        int by = LIST_Y + (th_ - bh) * topi / (nent - rv);
        fill_rr(W - 14, LIST_Y, 4, th_, 2, C_DIM, 80);
        fill_rr(W - 14, by, 4, bh, 2, cc, 255);
    }
    if (np_visible()) draw_now_playing_bar();
    const char *k[8] = {"A", TX(X_H_OPEN), "B", TX(X_H_BACK), "L/R", TX(X_H_PAGE), "Start", TX(X_H_NOWPLAYING)};
    draw_hints(k, pl_n ? 4 : 3);
}

static void draw_help(void) {
    draw_bg();
    draw_header(TX(X_HELP), CC_HELP);
    struct { int t, l1, l2; SDL_Color c; } sec[4] = {
        {X_HELP_MUSIC_T, X_HELP_MUSIC_1, X_HELP_MUSIC_2, CC_MUSIC},
        {X_HELP_VIDEO_T, X_HELP_VIDEO_1, X_HELP_VIDEO_2, CC_VIDEO},
        {X_HELP_BOOK_T,  X_HELP_BOOK_1,  X_HELP_BOOK_2,  CC_BOOKS},
        {X_HELP_DIR_T,   X_HELP_DIR_1,   X_HELP_DIR_2,   CC_LANG},
    };
    int y = 64;
    for (int i = 0; i < 4; i++) {
        fill_rr(20, y + 3, 4, 17, 2, sec[i].c, 255);
        draw_text(f_small, TX(sec[i].t), 34, y, sec[i].c, W - 60);
        draw_text(f_tiny, TX(sec[i].l1), 34, y + 25, C_TEXT, W - 54);
        draw_text(f_tiny, TX(sec[i].l2), 34, y + 44, C_DIM, W - 54);
        y += 76;
    }
    draw_text(f_tiny, TX(X_HELP_LANG), 20, y + 2, C_AMB, W - 40);
    const char *k[2] = {"B", TX(X_H_BACK)};
    draw_hints(k, 1);
}

static void draw_player(void) {
    draw_bg();
    draw_header(TX(X_NOW_PLAYING), CC_MUSIC);
    if (!pl_n || !playing()) {
        draw_text_center(f_mid, play_err[0] ? play_err : TX(X_NOTHING), W / 2, 200, C_DIM, W - 60);
        const char *k[2] = {"B", TX(X_H_BACK)};
        draw_hints(k, 1);
        return;
    }
    fill_rr(30, 74, 190, 190, 20, (SDL_Color){40, 58, 96, 255}, 255);
    fill_rr(36, 80, 178, 178, 16, (SDL_Color){52, 76, 124, 255}, 255);
    icon_note(125, 169, 52, C_TEXT);

    char t[300];
    title_from_path(pl[pl_i], t, sizeof t);
    draw_text(f_big, t, 246, 88, C_TEXT, W - 266);
    char dir[MAXP];
    snprintf(dir, sizeof dir, "%s", pl[pl_i]);
    char *s = strrchr(dir, '/'); if (s) *s = 0;
    draw_text(f_small, base_name(dir), 246, 134, C_DIM, W - 266);
    char info[64];
    snprintf(info, sizeof info, "%d / %d", pl_i + 1, pl_n);
    draw_text(f_small, info, 246, 162, C_DIM, 0);

    char st[96];
    int x = 246;
    snprintf(st, sizeof st, "%s: %s", TX(X_SHUFFLE), shuffle ? TX(X_ON) : TX(X_OFF));
    x = pill_l(x, 214, st, shuffle ? CC_MUSIC : C_PILL, shuffle ? C_DARK : C_DIM);
    snprintf(st, sizeof st, "%s: %s", TX(X_REPEAT),
             repeat_mode == 0 ? TX(X_OFF) : repeat_mode == 1 ? TX(X_LIST) : TX(X_ONE));
    pill_l(x, 214, st, repeat_mode ? CC_MUSIC : C_PILL, repeat_mode ? C_DARK : C_DIM);

    double el = elapsed_sec();
    double tot = total_frames ? (double)total_frames / (double)rate : 0;
    int bx = 30, bw = W - 60, by = 300;
    fill_rr(bx, by, bw, 10, 5, C_PILL, 255);
    int fw = 0;
    if (tot > 0) {
        double fr = el / tot; if (fr > 1) fr = 1;
        fw = (int)(bw * fr);
        if (fw > 0) fill_rr(bx, by, fw < 10 ? 10 : fw, 10, 5, CC_MUSIC, 255);
    }
    fill_circle(bx + fw, by + 5, 9, C_TEXT, 255);
    char a[32], b[32];
    fmt_time(a, sizeof a, el);
    fmt_time(b, sizeof b, tot);
    draw_text(f_small, a, bx, by + 20, C_TEXT, 0);
    if (tot > 0) draw_text_right(f_small, b, bx + bw, by + 20, C_DIM);

    int cy = 384;
    icon_prev(W / 2 - 96, cy, 13, C_TEXT);
    fill_circle(W / 2, cy, 32, CC_MUSIC, 255);
    if (a_paused) icon_play(W / 2 + 2, cy, 14, C_DARK); else icon_pause(W / 2, cy, 13, C_DARK);
    icon_next(W / 2 + 96, cy, 13, C_TEXT);

    const char *k[14] = {"A", TX(X_H_PAUSE), "< >", TX(X_H_SEEK), "L/R", TX(X_H_TRACK),
                         "X", TX(X_SHUFFLE), "Y", TX(X_REPEAT), "Select", TX(X_H_STOP), "B", TX(X_H_BACK)};
    draw_hints(k, 7);
}

static void draw_screen(void) {
    switch (scr) {
    case S_HOME:   draw_home(); break;
    case S_BROWSE: draw_browse(); break;
    case S_HELP:   draw_help(); break;
    case S_PLAYER: draw_player(); break;
    case S_READER: reader_draw(); break;
    case S_VIDEO:  v_draw(); break;
    }
}

/* ---------------------------------------------------------------- girdi */
static void list_move(int d) {
    if (nent == 0) return;
    sel += d;
    if (sel < 0) sel = nent - 1;
    if (sel >= nent) sel = 0;
}

static void list_activate(void) {
    if (nent == 0) return;
    Entry *e = &ents[sel];
    if (scr == S_HOME) home_sel = sel;
    switch (e->type) {
    case T_CAT_MUSIC: open_category(0); break;
    case T_CAT_BOOKS: open_category(1); break;
    case T_CAT_VIDEO: open_category(2); break;
    case T_CAT_LANG: lang_toggle(); break;
    case T_CAT_HELP: scr = S_HELP; break;
    case T_VID: v_open(e->path); break;
    case T_DIR: { char p[MAXP]; snprintf(p, sizeof p, "%s", e->path); enter_dir(p); break; }
    case T_MP3: playlist_from_browser(sel); if (playing() || play_err[0]) scr = S_PLAYER; break;
    case T_PDF: { char p[MAXP]; snprintf(p, sizeof p, "%s", e->path); reader_open(p); break; }
    }
}

static void handle_key(Key k) {
    dirty = 1;
    if (scr == S_HELP) { if (k == K_B || k == K_A || k == K_START) scr = S_HOME; return; }
    if (scr == S_READER) { reader_key(k); return; }
    if (scr == S_VIDEO) { v_key(k); return; }
    if (scr == S_PLAYER) {
        switch (k) {
        case K_A: case K_START:
            if (adev) { a_paused = !a_paused; SDL_PauseAudioDevice(adev, a_paused); }
            break;
        case K_LEFT:  seek_rel(-10); break;
        case K_RIGHT: seek_rel(10); break;
        case K_L:
            if (elapsed_sec() > 3) seek_rel(-1e9); else track_step(-1, 0);
            break;
        case K_R: track_step(1, 0); break;
        case K_X: shuffle = !shuffle; break;
        case K_Y: repeat_mode = (repeat_mode + 1) % 3; break;
        case K_SELECT: track_close(); pl_free(); scr = S_BROWSE; break;
        case K_B: scr = S_BROWSE; break;
        default: break;
        }
        return;
    }
    /* S_HOME / S_BROWSE */
    switch (k) {
    case K_UP: list_move(-1); break;
    case K_DOWN: list_move(1); break;
    case K_L: case K_LEFT:  if (k == K_L) { sel -= rows_visible(); if (sel < 0) sel = 0; } break;
    case K_R: case K_RIGHT: if (k == K_R) { sel += rows_visible(); if (sel >= nent) sel = nent - 1; if (sel < 0) sel = 0; } break;
    case K_A: list_activate(); break;
    case K_B: if (scr == S_HOME) running = 0; else go_up(); break;
    case K_START: if (playing() || pl_n) scr = S_PLAYER; break;
    case K_Y: if (scr == S_HOME) lang_toggle(); break;
    default: break;
    }
}

static int read_dirs(void) {
    if (!pad) return 0;
    int m = 0;
    int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    int ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    m |= 1;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  m |= 2;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  m |= 4;
    if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) m |= 8;
    if (abs(ax) > 16000 || abs(ay) > 16000) {
        if (abs(ax) >= abs(ay)) m |= ax < 0 ? 4 : 8;
        else m |= ay < 0 ? 1 : 2;
    }
    return m;
}

static void fire_dirs(int m) {
    if (m & 1) handle_key(K_UP);
    if (m & 2) handle_key(K_DOWN);
    if (m & 4) handle_key(K_LEFT);
    if (m & 8) handle_key(K_RIGHT);
}

static void open_pad(void) {
    if (pad) return;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); if (pad) break; }
}

static void handle_event(const SDL_Event *ev) {
    switch (ev->type) {
    case SDL_QUIT: running = 0; break;
    case SDL_CONTROLLERDEVICEADDED: open_pad(); break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (pad && !SDL_GameControllerGetAttached(pad)) { SDL_GameControllerClose(pad); pad = NULL; }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        switch (ev->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_A: handle_key(K_A); break;
        case SDL_CONTROLLER_BUTTON_B: handle_key(K_B); break;
        case SDL_CONTROLLER_BUTTON_X: handle_key(K_X); break;
        case SDL_CONTROLLER_BUTTON_Y: handle_key(K_Y); break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: handle_key(K_L); break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: handle_key(K_R); break;
        case SDL_CONTROLLER_BUTTON_START: handle_key(K_START); break;
        case SDL_CONTROLLER_BUTTON_BACK: handle_key(K_SELECT); break;
        default: break;
        }
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (ev->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT && ev->caxis.value > 16000) handle_key(K_L2);
        if (ev->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT && ev->caxis.value > 16000) handle_key(K_R2);
        break;
    case SDL_KEYDOWN:               /* klavye (test icin) */
        switch (ev->key.keysym.sym) {
        case SDLK_UP: handle_key(K_UP); break;
        case SDLK_DOWN: handle_key(K_DOWN); break;
        case SDLK_LEFT: handle_key(K_LEFT); break;
        case SDLK_RIGHT: handle_key(K_RIGHT); break;
        case SDLK_RETURN: handle_key(K_A); break;
        case SDLK_ESCAPE: handle_key(K_B); break;
        default: break;
        }
        break;
    default: break;
    }
}

/* ---------------------------------------------------------------- main */
int main(void) {
    srand((unsigned)time(NULL));
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        /* ses olmadan da acilabilsin */
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
            fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
            return 1;
        }
    }
    TTF_Init();
    win = SDL_CreateWindow("mediabox", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, 0);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    R = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!R) R = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!R) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(R, W, H);
    bg_init();
    SDL_ShowCursor(SDL_DISABLE);

    SDL_GameControllerAddMapping(PAD_MAP);
    open_pad();

    const char *ff = access(FONT_MAIN, R_OK) == 0 ? FONT_MAIN : FONT_ALT;
    f_big   = TTF_OpenFont(ff, 30);
    f_mid   = TTF_OpenFont(ff, 22);
    f_small = TTF_OpenFont(ff, 18);
    f_tiny  = TTF_OpenFont(ff, 14);
    if (!f_big || !f_mid || !f_small || !f_tiny) { fprintf(stderr, "font yuklenemedi: %s\n", ff); return 1; }

    prog_load();
    settings_load();
    show_home();

    int held = 0;
    Uint32 next_rep = 0, last_tick = 0;
    while (running) {
        SDL_Event ev;
        int timeout = scr == S_VIDEO ? 5 : (held ? 20 : 250);
        if (SDL_WaitEventTimeout(&ev, timeout)) {
            do handle_event(&ev); while (SDL_PollEvent(&ev));
        }
        Uint32 now = SDL_GetTicks();
        int m = read_dirs();
        int fresh = m & ~held;
        if (fresh) { fire_dirs(fresh); next_rep = now + 350; }
        else if (m && now >= next_rep) { fire_dirs(m); next_rep = now + 85; }
        held = m;

        if (scr == S_VIDEO) v_tick();
        if (a_ended) {
            a_ended = 0;
            track_step(1, 1);
            dirty = 1;
        }
        if (playing() && (scr == S_PLAYER || np_visible()) && now - last_tick >= 500) {
            last_tick = now;
            dirty = 1;
        }
        if (dirty) {
            dirty = 0;
            draw_screen();
            SDL_RenderPresent(R);
        }
    }

    v_close();
    track_close();
    pl_free();
    reader_close();
    free(ents);
    if (pad) SDL_GameControllerClose(pad);
    tc_free();
    if (bg_tex) SDL_DestroyTexture(bg_tex);
    TTF_CloseFont(f_big); TTF_CloseFont(f_mid); TTF_CloseFont(f_small); TTF_CloseFont(f_tiny);
    TTF_Quit();
    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
