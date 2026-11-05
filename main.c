/*==================================================================
  main.c  ——  程序入口 + 数据层 + 公共工具
  ------------------------------------------------------------------
  负责人：A
  包含四块：
    1) 全局数据实体（头文件里只 extern，实体在这里定义一次）
    2) 数据层  dataInit / loadXxx / saveXxx
    3) 工具    inputInt / inputStr / pauseScreen / slotTime / rowName / addDays / newXxxId
    4) main    主菜单循环
==================================================================*/
#include "cinema.h"
#include <direct.h>     /* _mkdir */
#include <windows.h>    /* 控制台模式、光标位置：颜色自适应要用 */
#include <conio.h>      /* _getch：密码输入显示 * */
#include <io.h>         /* _isatty：判断 stdin 是不是控制台 */

/*==================== 1. 全局数据实体 ====================*/
/* 头文件里是 extern，实体只能在这里定义一次。
   别的 .c 里再写一遍 User g_users[...] 就会 LNK2005 重复定义。 */
User     g_users[MAX_USERS];
int      g_userCnt = 0;

Movie    g_movies[MAX_MOVIES];
int      g_movieCnt = 0;

Schedule g_scheds[MAX_SCHEDULES];
int      g_schedCnt = 0;

Ticket   g_tickets[MAX_TICKETS];
int      g_ticketCnt = 0;

char     g_today[11] = { 0 };

Admin    g_admins[MAX_ADMINS];
int      g_adminCnt = 0;
int      g_currentAdminId = 0;
int      g_currentAdminRole = 0;
char     g_currentAdminAccount[20] = { 0 };

/* 控制台能力探测结果：
   g_colorOn  1=支持 ANSI 颜色（Win10+ 控制台），0=不支持或输出被重定向
   g_cellW    □ 这个字符在本机控制台上占几列（宋体 2 列，Consolas 1 列） */
int      g_colorOn = 0;
int      g_cellW = 2;

/*==================== 2. 数据层 ====================*/
/* 读写规则（双方必须一致，改了要通知对方）：
     user.dat   : [int 条数][User × 条数]
     cinema.dat : [int 影片条数][Movie × 条数][int 场次条数][Schedule × 条数]
     ticket.dat : [int 条数][Ticket × 条数]
   一律 "rb"/"wb" 二进制模式。用 "r"/"w" 会因为换行符转换让整个结构体错位。 */

   /* 取系统当天日期，写进 out（10 字符 + '\0'） */
static void getToday(char* out)
{
    time_t    t = time(NULL);
    struct tm* p = localtime(&t);
    sprintf(out, "%04d-%02d-%02d", p->tm_year + 1900, p->tm_mon + 1, p->tm_mday);
}


void getNow(char* out)
{
    time_t    t = time(NULL);
    struct tm* p = localtime(&t);
    sprintf(out, "%04d-%02d-%02d %02d:%02d:%02d",
        p->tm_year + 1900, p->tm_mon + 1, p->tm_mday,
        p->tm_hour, p->tm_min, p->tm_sec);
}


/*------------------------------------------------------------------
  距离这一场开演还有多少分钟。
  已开场返回负数（-1 表示刚刚开场不到一分钟）。
  时间基准统一在这里算，别处一律不再自己拼 struct tm。
------------------------------------------------------------------*/
int minutesToStart(int sIdx)
{
    struct tm tmShow;
    time_t    tShow, tNow;
    long      secs;

    if (sIdx < 0 || sIdx >= g_schedCnt) return 0;

    memset(&tmShow, 0, sizeof(tmShow));
    if (sscanf(g_scheds[sIdx].date, "%d-%d-%d",
               &tmShow.tm_year, &tmShow.tm_mon, &tmShow.tm_mday) != 3)
        return 0;

    tmShow.tm_year -= 1900;
    tmShow.tm_mon  -= 1;

    switch (g_scheds[sIdx].slot)
    {
    case 1:  tmShow.tm_hour = 10; tmShow.tm_min = 0;  break;
    case 2:  tmShow.tm_hour = 14; tmShow.tm_min = 30; break;
    case 3:  tmShow.tm_hour = 19; tmShow.tm_min = 0;  break;
    default: return 0;
    }

    tmShow.tm_isdst = -1;                   /* 让系统自己判断夏令时 */
    tShow = mktime(&tmShow);
    if (tShow == (time_t)-1) return 0;

    tNow = time(NULL);
    secs = (long)(tShow - tNow);

    /* 向上取整：还剩 30 秒也算 1 分钟，避免"看着还有 0 分钟其实没开场" */
    if (secs > 0) return (int)((secs + 59) / 60);
    if (secs >= -59) return -1;
    return (int)(secs / 60);
}

/* 1=已经开场（含刚刚开场），0=还没到点 */
int isStarted(int sIdx)
{
    return minutesToStart(sIdx) <= 0;
}

/*------------------------------------------------------------------
  阶梯手续费。minutesLeft 传负数（已开场）时调用方不该再调这里。
    剩余 >= 24 小时      免费
    6 ~ 24 小时          2 元
    2 ~ 6 小时           5 元
    2 小时以内          10 元
  改签只换场次、人不退，所以按半价收（2→1，5→3，10→5）。
  改档位只改这一个函数，退票列表、确认页、跨档提示都跟着变。
------------------------------------------------------------------*/
float calcFee(long minutesLeft, int isChange)
{
    float fee;

    if (minutesLeft >= 24L * 60)      fee = 0.0f;
    else if (minutesLeft >= 6L * 60)  fee = 2.0f;
    else if (minutesLeft >= 2L * 60)  fee = 5.0f;
    else                              fee = 10.0f;

    if (isChange)
        fee = (float)(int)(fee / 2.0f + 0.5f);   /* 四舍五入到整元 */

    return fee;
}

/*------------------------------------------------------------------
  按用户编号查名字。找不到返回空串，调用方必须容忍空串。
  只用于"迁移老订单时回填一次"和"显示当前登录是谁"，
  绝不能拿它去覆盖订单里已经存好的 buyerName，那正好把快照毁了。
------------------------------------------------------------------*/
const char* userNameById(int uid)
{
    int i;

    for (i = 0; i < g_userCnt; i++)
        if (g_users[i].id == uid) return g_users[i].name;

    return "";
}

/*------------------------------------------------------------------
  量一下 □ 这个字符在本机控制台上占几列。
  做法：记下光标位置 → 打印一个 □ → 再看光标挪了几列。
  宋体等宽中文字体是 2 列，Consolas 之类西文字体只有 1 列，
  不实测的话，座位图换台机器就会错位。
------------------------------------------------------------------*/
static int measureCellWidth(void)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO a, b;
    COORD home;
    int   w, i;

    if (hOut == INVALID_HANDLE_VALUE) return 2;
    if (!GetConsoleScreenBufferInfo(hOut, &a)) return 2;

    /* 贴着行尾就没法量（会回绕），先回车到行首 */
    if (a.dwCursorPosition.X > a.dwSize.X - 4)
    {
        printf("\r");
        if (!GetConsoleScreenBufferInfo(hOut, &a)) return 2;
    }

    home = a.dwCursorPosition;

    printf("□");
    fflush(stdout);

    if (!GetConsoleScreenBufferInfo(hOut, &b))
    {
        printf("\r");
        return 2;
    }

    w = (int)b.dwCursorPosition.X - (int)a.dwCursorPosition.X;
    if (w < 1 || w > 2) w = 2;              /* 量不出来就按 2 列 */

    /* 把探针擦掉，光标放回原处，屏幕上不留痕迹 */
    SetConsoleCursorPosition(hOut, home);
    for (i = 0; i < w; i++) putchar(' ');
    SetConsoleCursorPosition(hOut, home);
    fflush(stdout);

    return w;
}

/*------------------------------------------------------------------
  启动时探测一次：能不能用 ANSI 颜色、一个方块占多宽。
  输出被重定向到文件时（自动化测试），GetConsoleMode 会失败，
  这里就自动降级成无色纯文本，不会往文件里灌一堆 ESC 序列。
------------------------------------------------------------------*/
void initConsoleColor(void)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;

    g_colorOn = 0;
    g_cellW   = 2;

    if (hOut == INVALID_HANDLE_VALUE) return;
    if (!GetConsoleMode(hOut, &mode)) return;          /* 不是控制台 */

    if (!SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        return;                                        /* Win7 之类不支持 */

    g_colorOn = 1;
    g_cellW   = measureCellWidth();
}

/*------------------------------------------------------------------
  密码输入：屏幕上一律显示 *，不显示明文。
  按退格擦掉一个星号，回车结束，ESC 放弃。
  stdin 不是控制台（管道喂数据做自动化测试）时，_getch 用不了，
  自动退回按行读 —— 这条分支删了，测试脚本就跑不动。
------------------------------------------------------------------*/
void inputPassword(char* buf, int size)
{
    int i = 0, ch;

    if (!_isatty(_fileno(stdin)))
    {
        if (fgets(buf, size, stdin) == NULL) { buf[0] = '\0'; return; }
        buf[strcspn(buf, "\r\n")] = '\0';
        return;
    }

    while (1)
    {
        ch = _getch();

        if (ch == '\r' || ch == '\n')
        {
            buf[i] = '\0';
            printf("\n");
            return;
        }
        if (ch == 8 || ch == 127)                   /* 退格 */
        {
            if (i > 0) { i--; printf("\b \b"); }
            continue;
        }
        if (ch == 27)                               /* ESC 放弃 */
        {
            buf[0] = '\0';
            printf("\n");
            return;
        }
        if (ch < 32 || ch > 126) continue;           /* 只收可见 ASCII */

        if (i < size - 1)
        {
            buf[i++] = (char)ch;
            printf("*");
        }
    }
}

/*------------------------------------------------------------------
  安全存盘：先写同目录下的 .tmp，写完刷盘、关闭，再"改名覆盖"。
  改名是原子的：写到一半断电、被任务管理器结束进程，正式文件仍是
  上一份完整的，不会变成 0 字节或者半截记录。
------------------------------------------------------------------*/
static FILE* openTempFile(const char* path, char* tmp, int size)
{
    snprintf(tmp, (size_t)size, "%s.tmp", path);
    return fopen(tmp, "wb");
}

static int commitTempFile(FILE* fp, const char* tmp, const char* path)
{
    if (fflush(fp) != 0) { fclose(fp); remove(tmp); return 0; }
    if (fclose(fp) != 0) { remove(tmp); return 0; }

#ifdef _WIN32
    if (!MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
#else
    if (rename(tmp, path) != 0)
#endif
    {
        remove(tmp);
        return 0;
    }
    return 1;
}

/* 读不出来的文件不原地覆盖：先改名留个档。
   否则"检测到损坏 → 重置内存 → 退出时保存"会把仅剩的原文件也冲掉。 */
static void quarantineFile(const char* path)
{
    char dst[160];

    snprintf(dst, (size_t)sizeof(dst), "%s.corrupt", path);
    remove(dst);
    if (rename(path, dst) == 0)
        printf("提示：%s 已另存为 %s，可人工检查或从 data/backup 恢复。\n", path, dst);
}

/* 文件总字节数（不改变当前读写位置） */
static long fileSizeOf(FILE* fp)
{
    long cur = ftell(fp), end;

    fseek(fp, 0, SEEK_END);
    end = ftell(fp);
    fseek(fp, cur, SEEK_SET);
    return end;
}

void loadUsers(void)
{
    FILE* fp;
    long  fsize, need;

    g_userCnt = 0;
    fp = fopen(FILE_USER, "rb");
    if (fp == NULL) return;                 /* 首次运行正常，无文件即无数据 */

    if (fread(&g_userCnt, sizeof(int), 1, fp) != 1)
    {
        g_userCnt = 0;
        fclose(fp);
        quarantineFile(FILE_USER);
        return;
    }
    if (g_userCnt < 0 || g_userCnt > MAX_USERS)   /* 文件损坏，整份丢弃 */
    {
        printf("警告：user.dat 条数异常(%d)，已重置。\n", g_userCnt);
        g_userCnt = 0;
        fclose(fp);
        quarantineFile(FILE_USER);
        return;
    }

    /* 条数对还不够，文件长度必须正好等于 4 + 条数×单条。
       文件被拷了一半时，尾部会读出一批全 0 记录（空账号、0 权限），
       这种"幽灵记录"必须整份拒绝，不能带着跑。 */
    fsize = fileSizeOf(fp);
    need  = (long)sizeof(int) + (long)g_userCnt * (long)sizeof(User);
    if (fsize != need)
    {
        printf("警告：user.dat 长度异常（%ld 字节，按 %d 条应为 %ld 字节），已重置。\n",
            fsize, g_userCnt, need);
        g_userCnt = 0;
        fclose(fp);
        quarantineFile(FILE_USER);
        return;
    }

    if (g_userCnt > 0 &&
        fread(g_users, sizeof(User), (size_t)g_userCnt, fp) != (size_t)g_userCnt)
    {
        printf("警告：user.dat 读取不完整，已重置。\n");
        memset(g_users, 0, sizeof(g_users));
        g_userCnt = 0;
        fclose(fp);
        quarantineFile(FILE_USER);
        return;
    }
    fclose(fp);
}

void saveUsers(void)
{
    char  tmp[128];
    FILE* fp;
    int   ok;

    fp = openTempFile(FILE_USER, tmp, (int)sizeof(tmp));
    if (fp == NULL)
    {
        printf("user.dat 保存失败，请检查 write 权限。\n");
        return;
    }

    ok = (fwrite(&g_userCnt, sizeof(int), 1, fp) == 1);
    if (ok && g_userCnt > 0)
        ok = (fwrite(g_users, sizeof(User), (size_t)g_userCnt, fp) == (size_t)g_userCnt);
    if (!ok)
    {
        fclose(fp);
        remove(tmp);
        printf("user.dat 写入失败，原文件未改动。\n");
        return;
    }
    if (!commitTempFile(fp, tmp, FILE_USER))
        printf("user.dat 替换失败，原文件未改动。\n");
}

void loadCinema(void)
{
    FILE* fp;
    long  fsize, need;

    g_movieCnt = 0;
    g_schedCnt = 0;
    fp = fopen(FILE_CINEMA, "rb");
    if (fp == NULL) return;

    fsize = fileSizeOf(fp);

    /* 第一段：影片 */
    if (fread(&g_movieCnt, sizeof(int), 1, fp) != 1) { g_movieCnt = 0; fclose(fp); quarantineFile(FILE_CINEMA); return; }
    if (g_movieCnt < 0 || g_movieCnt > MAX_MOVIES)
    {
        printf("警告：cinema.dat 影片条数异常(%d)，已重置。\n", g_movieCnt);
        g_movieCnt = g_schedCnt = 0;
        fclose(fp);
        quarantineFile(FILE_CINEMA);
        return;
    }

    /* 先看文件够不够放影片段：不够就是被截断了，别再往下读 */
    need = (long)sizeof(int) + (long)g_movieCnt * (long)sizeof(Movie) + (long)sizeof(int);
    if (fsize < need)
    {
        printf("警告：cinema.dat 长度不足（%ld 字节，至少应为 %ld 字节），已重置。\n",
            fsize, need);
        g_movieCnt = g_schedCnt = 0;
        fclose(fp);
        quarantineFile(FILE_CINEMA);
        return;
    }
    if (g_movieCnt > 0 &&
        fread(g_movies, sizeof(Movie), (size_t)g_movieCnt, fp) != (size_t)g_movieCnt)
    {
        printf("警告：cinema.dat 影片段读取不完整，已重置。\n");
        memset(g_movies, 0, sizeof(g_movies));
        g_movieCnt = g_schedCnt = 0;
        fclose(fp);
        quarantineFile(FILE_CINEMA);
        return;
    }

    /* 第二段：场次（座位在 Schedule 里，一起读进来） */
    if (fread(&g_schedCnt, sizeof(int), 1, fp) != 1) { g_schedCnt = 0; fclose(fp); quarantineFile(FILE_CINEMA); return; }
    if (g_schedCnt < 0 || g_schedCnt > MAX_SCHEDULES)
    {
        printf("警告：cinema.dat 场次条数异常(%d)，已重置场次。\n", g_schedCnt);
        g_schedCnt = 0;
        fclose(fp);
        quarantineFile(FILE_CINEMA);
        return;
    }

    /* 总长度必须严丝合缝：多一条少一条都说明文件坏了。
       少了会出现"幽灵场次"（日期空、票价 0），还能选座买出 0 元票 */
    need = (long)sizeof(int) + (long)g_movieCnt * (long)sizeof(Movie)
         + (long)sizeof(int) + (long)g_schedCnt * (long)sizeof(Schedule);
    if (fsize != need)
    {
        printf("警告：cinema.dat 长度异常（%ld 字节，按 %d 部影片 %d 场应为 %ld 字节），"
               "已重置场次。\n", fsize, g_movieCnt, g_schedCnt, need);
        g_schedCnt = 0;
        fclose(fp);
        quarantineFile(FILE_CINEMA);
        return;
    }
    if (g_schedCnt > 0 &&
        fread(g_scheds, sizeof(Schedule), (size_t)g_schedCnt, fp) != (size_t)g_schedCnt)
    {
        printf("警告：cinema.dat 场次段读取不完整，已重置场次。\n");
        memset(g_scheds, 0, sizeof(g_scheds));
        g_schedCnt = 0;
        fclose(fp);
        quarantineFile(FILE_CINEMA);
        return;
    }
    fclose(fp);
}

void saveCinema(void)
{
    char  tmp[128];
    FILE* fp;
    int   ok;

    fp = openTempFile(FILE_CINEMA, tmp, (int)sizeof(tmp));
    if (fp == NULL)
    {
        printf("cinema.dat 保存失败，请检查 write 权限。\n");
        return;
    }

    ok = (fwrite(&g_movieCnt, sizeof(int), 1, fp) == 1);
    if (ok && g_movieCnt > 0)
        ok = (fwrite(g_movies, sizeof(Movie), (size_t)g_movieCnt, fp) == (size_t)g_movieCnt);
    if (ok)
        ok = (fwrite(&g_schedCnt, sizeof(int), 1, fp) == 1);
    if (ok && g_schedCnt > 0)
        ok = (fwrite(g_scheds, sizeof(Schedule), (size_t)g_schedCnt, fp) == (size_t)g_schedCnt);
    if (!ok)
    {
        fclose(fp);
        remove(tmp);
        printf("cinema.dat 写入失败，原文件未改动。\n");
        return;
    }
    if (!commitTempFile(fp, tmp, FILE_CINEMA))
        printf("cinema.dat 替换失败，原文件未改动。\n");
}

/* 上一版订单格式：144 字节/条，比现在少一个购票人快照。
   新字段都加在结构体末尾，所以前 144 字节可以整块照搬。 */
typedef struct
{
    int   id;
    int   userId;
    int   schedId;
    char  movieName[50];
    char  date[11];
    int   slot;
    int   hallId;
    int   row;
    int   col;
    float price;
    int   state;
    char  purchase_time[DATETIME_LEN];
    char  refund_time[DATETIME_LEN];
    int   refund_admin_id;
} TicketV0;

void loadTickets(void)
{
    FILE* fp;
    TicketV0 old;
    long fsize, rec, body;
    int  i, bad = 0;

    g_ticketCnt = 0;
    fp = fopen(FILE_TICKET, "rb");
    if (fp == NULL) return;

    if (fread(&g_ticketCnt, sizeof(int), 1, fp) != 1) { g_ticketCnt = 0; fclose(fp); quarantineFile(FILE_TICKET); return; }
    if (g_ticketCnt < 0 || g_ticketCnt > MAX_TICKETS)
    {
        printf("警告：ticket.dat 条数异常(%d)，已重置。\n", g_ticketCnt);
        g_ticketCnt = 0;
        fclose(fp);
        quarantineFile(FILE_TICKET);
        return;
    }

    /* 按"文件长度 / 条数"倒推单条记录多大，再决定怎么读。
       这样旧版本的数据文件不用手工删，启动时自动升级。 */
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, sizeof(int), SEEK_SET);
    body = fsize - (long)sizeof(int);
    rec  = (g_ticketCnt > 0) ? body / g_ticketCnt : 0;

    /* 长度除不尽说明文件被截断或写了一半，宁可整份不读 */
    if (g_ticketCnt > 0 && body % (long)g_ticketCnt != 0)
    {
        printf("警告：ticket.dat 长度不是整条记录（%ld 字节 / %d 条），已放弃读取。\n",
            body, g_ticketCnt);
        g_ticketCnt = 0;
        fclose(fp);
        quarantineFile(FILE_TICKET);
        return;
    }

    if (g_ticketCnt > 0 && rec == (long)sizeof(Ticket))
    {
        if (fread(g_tickets, sizeof(Ticket), (size_t)g_ticketCnt, fp) != (size_t)g_ticketCnt)
        {
            printf("警告：ticket.dat 读取不完整，已重置订单表。\n");
            memset(g_tickets, 0, sizeof(g_tickets));
            g_ticketCnt = 0;
            fclose(fp);
            quarantineFile(FILE_TICKET);
            return;
        }
        fclose(fp);
        return;
    }

    if (g_ticketCnt > 0 && rec == (long)sizeof(TicketV0))
    {
        /* 老格式：逐条搬，购票人按账号当前名字回填一次。
           回填进去就成了快照，以后用户改名不会再动它。 */
        for (i = 0; i < g_ticketCnt; i++)
        {
            if (fread(&old, sizeof(TicketV0), 1, fp) != 1) { bad = 1; break; }

            memset(&g_tickets[i], 0, sizeof(Ticket));
            g_tickets[i].id     = old.id;
            g_tickets[i].userId = old.userId;
            g_tickets[i].schedId = old.schedId;
            strncpy(g_tickets[i].movieName, old.movieName, 49);
            g_tickets[i].movieName[49] = '\0';
            strncpy(g_tickets[i].date, old.date, 10);
            g_tickets[i].date[10] = '\0';
            g_tickets[i].slot   = old.slot;
            g_tickets[i].hallId = old.hallId;
            g_tickets[i].row    = old.row;
            g_tickets[i].col    = old.col;
            g_tickets[i].price  = old.price;
            g_tickets[i].state  = old.state;
            strncpy(g_tickets[i].purchase_time, old.purchase_time, DATETIME_LEN - 1);
            strncpy(g_tickets[i].refund_time, old.refund_time, DATETIME_LEN - 1);
            g_tickets[i].refund_admin_id = old.refund_admin_id;
            strncpy(g_tickets[i].buyerName, userNameById(old.userId), 19);
        }
        fclose(fp);

        /* 迁移中途读失败：不能把"读了一半"当成功写回去，直接放弃 */
        if (bad)
        {
            printf("警告：ticket.dat 旧格式读取中断，已放弃迁移，原文件保持不动。\n");
            memset(g_tickets, 0, sizeof(g_tickets));
            g_ticketCnt = 0;
            quarantineFile(FILE_TICKET);
            return;
        }

        g_ticketCnt = i;
        printf("订单文件是上一版格式，已升级：%d 条记录补上购票人（按当前用户名回填）。\n",
            g_ticketCnt);
        saveTickets();
        return;
    }

    if (g_ticketCnt > 0)
    {
        printf("警告：ticket.dat 记录长度异常（单条 %ld 字节），已放弃读取。\n", rec);
        g_ticketCnt = 0;
        fclose(fp);
        quarantineFile(FILE_TICKET);
        return;
    }
    fclose(fp);
}

void saveTickets(void)
{
    char  tmp[128];
    FILE* fp;
    int   ok;

    fp = openTempFile(FILE_TICKET, tmp, (int)sizeof(tmp));
    if (fp == NULL)
    {
        printf("ticket.dat 保存失败，请检查 write 权限。\n");
        return;
    }

    ok = (fwrite(&g_ticketCnt, sizeof(int), 1, fp) == 1);
    if (ok && g_ticketCnt > 0)
        ok = (fwrite(g_tickets, sizeof(Ticket), (size_t)g_ticketCnt, fp) == (size_t)g_ticketCnt);
    if (!ok)
    {
        fclose(fp);
        remove(tmp);
        printf("ticket.dat 写入失败，原文件未改动。\n");
        return;
    }
    if (!commitTempFile(fp, tmp, FILE_TICKET))
        printf("ticket.dat 替换失败，原文件未改动。\n");
}

void loadAdmins(void)
{
    FILE* fp;
    long  fsize, need;

    g_adminCnt = 0;
    fp = fopen(FILE_ADMIN, "rb");
    if (fp == NULL) return;

    if (fread(&g_adminCnt, sizeof(int), 1, fp) != 1)
    {
        g_adminCnt = 0;
        fclose(fp);
        quarantineFile(FILE_ADMIN);
        return;
    }
    if (g_adminCnt < 0 || g_adminCnt > MAX_ADMINS)
    {
        printf("警告：admin.dat 条数异常(%d)，已重置。\n", g_adminCnt);
        g_adminCnt = 0;
        fclose(fp);
        quarantineFile(FILE_ADMIN);
        return;
    }

    fsize = fileSizeOf(fp);
    need  = (long)sizeof(int) + (long)g_adminCnt * (long)sizeof(Admin);
    if (fsize != need)
    {
        printf("警告：admin.dat 长度异常（%ld 字节，按 %d 条应为 %ld 字节），已重置。\n",
            fsize, g_adminCnt, need);
        g_adminCnt = 0;
        fclose(fp);
        quarantineFile(FILE_ADMIN);
        return;
    }

    if (g_adminCnt > 0 &&
        fread(g_admins, sizeof(Admin), (size_t)g_adminCnt, fp) != (size_t)g_adminCnt)
    {
        printf("警告：admin.dat 读取不完整，已重置。\n");
        memset(g_admins, 0, sizeof(g_admins));
        g_adminCnt = 0;
        fclose(fp);
        quarantineFile(FILE_ADMIN);
        return;
    }
    fclose(fp);
}

void saveAdmins(void)
{
    char  tmp[128];
    FILE* fp;
    int   ok;

    fp = openTempFile(FILE_ADMIN, tmp, (int)sizeof(tmp));
    if (fp == NULL)
    {
        printf("admin.dat 保存失败，请检查 write 权限。\n");
        return;
    }

    ok = (fwrite(&g_adminCnt, sizeof(int), 1, fp) == 1);
    if (ok && g_adminCnt > 0)
        ok = (fwrite(g_admins, sizeof(Admin), (size_t)g_adminCnt, fp) == (size_t)g_adminCnt);
    if (!ok)
    {
        fclose(fp);
        remove(tmp);
        printf("admin.dat 写入失败，原文件未改动。\n");
        return;
    }
    if (!commitTempFile(fp, tmp, FILE_ADMIN))
        printf("admin.dat 替换失败，原文件未改动。\n");
}

static void seedAdmin(void)
{
    if (g_adminCnt > 0) return;

    g_admins[0].id = 1;
    strcpy(g_admins[0].account, "admin");
    strcpy(g_admins[0].pwd, ADMIN_PWD);
    g_admins[0].role = 1;
    g_admins[0].status = 1;
    getNow(g_admins[0].create_time);
    g_admins[0].last_login[0] = '\0';
    g_adminCnt = 1;

    saveAdmins();
    printf("已创建默认超级管理员：admin（密码 123456）\n");
}

void dataSaveAll(void)
{
    saveUsers();
    saveCinema();
    saveTickets();
    saveAdmins();
}

void dataInit(void)
{
    _mkdir(DATA_DIR);           /* 已存在会返回 -1，不用管 */
    initConsoleColor();         /* 探测能不能上色、方块占几列，必须在打印之前 */
    getToday(g_today);
    loadUsers();
    loadCinema();
    loadTickets();
    loadAdmins();
    seedAdmin();

    /* 确保数据文件都存在，便于备份/恢复 */
    saveUsers();
    saveCinema();
    saveTickets();
    saveAdmins();

    printf("数据载入完成：用户 %d 个，影片 %d 部，场次 %d 场，订单 %d 条，管理员 %d 个。\n",
        g_userCnt, g_movieCnt, g_schedCnt, g_ticketCnt, g_adminCnt);
    if (g_movieCnt == 0)
        printf("提示：还没有任何影片，请先用管理员账号排片。\n");
    pauseScreen();
}

/*==================== 3. 工具 ====================*/
/* 全项目禁止直接用 scanf：
     1) 输入字母会死循环刷屏；
     2) 会往缓冲区留下 '\n'，害得后面的 pauseScreen 被跳过。 */

int inputInt(int lo, int hi)
{
    char buf[64];
    int  v;

    while (1)
    {
        if (fgets(buf, sizeof(buf), stdin) == NULL)   /* Ctrl+Z 之类 */
            return lo;
        if (sscanf(buf, "%d", &v) == 1 && v >= lo && v <= hi)
            return v;
        printf("  输入无效，请输入 %d ~ %d 之间的整数：", lo, hi);
    }
}

void inputStr(char* buf, int size)
{
    char tmp[256];
    int  len;

    while (1)
    {
        if (fgets(tmp, sizeof(tmp), stdin) == NULL)
        {
            buf[0] = '\0';
            return;
        }
        tmp[strcspn(tmp, "\r\n")] = '\0';       /* 去掉行尾换行 */
        len = (int)strlen(tmp);
        if (len == 0)
        {
            printf("  不能为空，请重新输入：");
            continue;
        }
        if (len >= size)                        /* 注意：中文一个字占 3 字节 */
        {
            printf("  太长了（上限 %d 字节，约 %d 个汉字），请重新输入：",
                size - 1, (size - 1) / 3);
            continue;
        }
        strcpy(buf, tmp);
        return;
    }
}

/* 键盘缓冲区里还有没有没读完的字符（仅控制台有意义） */
static int inputPending(void)
{
#ifdef _WIN32
    return _kbhit();
#else
    return 0;
#endif
}

/* 把当前行剩下的字符读掉。
   原来这里写的是 fflush(stdin)：那是微软扩展，标准 C 里是未定义行为，
   换成 GCC/Clang 编译，"按回车继续"会被整段跳过，提示一闪就没。
   注意不能空清：交互终端上缓冲本来就是空的，硬读一行会多要一次回车。 */
static void clearInputLine(void)
{
    int ch;

    if (_isatty(_fileno(stdin)) && !inputPending())
        return;

    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}

/* 按"显示宽度"截断并补齐：汉字 2 列、ASCII 1 列。
   printf 的 %-16s 按字节对齐，片名汉字一多整行就顶歪。
   以前 ticket.c 的 fitText 和 schedule.c 的 schedFit 是同一份代码，
   现在只留这一份，改规则不会再漏掉另一处。 */
void fitText(char* out, const char* src, int cols)
{
    int used = 0, o = 0, i = 0;

    while (src[i] != '\0')
    {
        unsigned char ch = (unsigned char)src[i];
        int w     = (ch < 0x80) ? 1 : 2;
        int bytes = (ch < 0x80) ? 1 : 2;

        if (used + w > cols) break;

        out[o++] = src[i];
        if (bytes == 2 && src[i + 1] != '\0') out[o++] = src[i + 1];

        i += bytes;
        used += w;
    }
    while (used < cols) { out[o++] = ' '; used++; }
    out[o] = '\0';
}

/* 票房加减统一走这里：每次变动后按"分"四舍五入。
   float 本身是近似值，1.6 万条订单反复加减会越漂越远；
   量化到分以后误差不再累积，负数也在这里兜住。 */
void addIncome(int sIdx, float delta)
{
    float v;

    if (sIdx < 0 || sIdx >= g_schedCnt) return;

    v = g_scheds[sIdx].income + delta;
    if (v < 0.0f) v = 0.0f;
    g_scheds[sIdx].income = (float)((long)(v * 100.0f + 0.5f)) / 100.0f;
}

/* 日期必须真的是日历上存在的一天：2026-02-31 这种"格式对、日子不对"的一律不收。
   做法：拆出年月日，交给 mktime 归一化，再读回来比对。 */
int isValidDate(const char* s)
{
    struct tm t;
    int y, m, d;

    if (s == NULL || strlen(s) != 10) return 0;
    if (s[4] != '-' || s[7] != '-')   return 0;
    if (sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3) return 0;
    if (y < 1970 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return 0;

    memset(&t, 0, sizeof(t));
    t.tm_year  = y - 1900;
    t.tm_mon   = m - 1;
    t.tm_mday  = d;
    t.tm_hour  = 12;
    t.tm_isdst = -1;
    if (mktime(&t) == (time_t)-1) return 0;

    return (t.tm_year == y - 1900 && t.tm_mon == m - 1 && t.tm_mday == d);
}

void pauseScreen(void)
{
    printf("\n按回车继续...");
    clearInputLine();   /* 有残留换行就先清掉，避免"没停住"就闪过去 */
    getchar();
}

const char* slotTime(int slot)
{
    switch (slot)
    {
    case 1:  return "10:00";
    case 2:  return "14:30";
    case 3:  return "19:00";
    default: return "--:--";
    }
}

const char* rowName(int row)
{
    static const char* names[] = { "A", "B", "C", "D", "E",
                                   "F", "G", "H", "I", "J" };
    if (row < 0 || row >= (int)(sizeof(names) / sizeof(names[0])))
        return "?";
    return names[row];
}

void addDays(char* out, int days)
{
    time_t     t = time(NULL);
    struct tm* p = localtime(&t);

    p->tm_mday += days;
    mktime(p);                      /* 规范化，自动处理跨月跨年 */
    strftime(out, 11, "%Y-%m-%d", p);
}

/* 编号 = 现有最大编号 + 1。不回收，删掉的最多留个洞，避免 id 重复。 */
int newUserId(void)
{
    int i, max = 0;
    for (i = 0; i < g_userCnt; i++)
        if (g_users[i].id > max) max = g_users[i].id;
    return max + 1;
}

int newMovieId(void)
{
    int i, max = 0;
    for (i = 0; i < g_movieCnt; i++)
        if (g_movies[i].id > max) max = g_movies[i].id;
    return max + 1;
}

int newSchedId(void)
{
    int i, max = 0;
    for (i = 0; i < g_schedCnt; i++)
        if (g_scheds[i].id > max) max = g_scheds[i].id;
    return max + 1;
}

int newTicketId(void)
{
    int i, max = 0;
    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].id > max) max = g_tickets[i].id;
    return max + 1;
}


int newAdminId(void)
{
    int i, max = 0;
    for (i = 0; i < g_adminCnt; i++)
        if (g_admins[i].id > max) max = g_admins[i].id;
    return max + 1;
}

/*==================== 4. 主菜单 ====================*/
void showMainMenu(void)
{
    printf("==============================================\n");
    printf("            模拟电影院售票系统\n");
    printf("            今天 %s   每场 %d 座\n", g_today, SEAT_TOTAL);
    printf("==============================================\n");
    printf("   1. 管理员登录\n");
    printf("   2. 用户注册\n");
    printf("   3. 用户登录\n");
    printf("   0. 退出系统\n");
    printf("----------------------------------------------\n");
    printf("请选择 [0-3]：");
}

int main(void)
{
    int  op;
    int  uid;
    char uname[20];

    dataInit();                     /* 建目录 + 取日期 + 三个文件读进内存 */

    while (1)
    {
        system("cls");
        showMainMenu();
        op = inputInt(0, 3);

        switch (op)
        {
        case 1:                                     /* 管理员 */
            if (adminLogin())
                adminMenu();                        /* 内部自带循环和暂停 */
            break;

        case 2:                                     /* 注册 */
            registerUser();                         /* 内部自带提示和暂停 */
            break;

        case 3:                                     /* 观众 */
            uname[0] = '\0';                        /* 清掉上一次登录残留的用户名 */
            uid = 0;
            if (loginUser(uname, &uid))
                userMenu(uid, uname);               /* 内部自带循环和暂停 */
            break;

        case 0:                                     /* 退出 */
            dataSaveAll();                          /* 平时"改完即存"，这里再兜一次底 */
            printf("\n数据已保存，程序退出。\n");
            return 0;
        }
    }
    return 0;
}



/* ==================== 日志 / 备份 / 一致性 ==================== */

static int copyFileBinary(const char* src, const char* dst)
{
    FILE* in = fopen(src, "rb");
    FILE* out;
    char buf[4096];
    size_t n;

    if (in == NULL) return 0;
    out = fopen(dst, "wb");
    if (out == NULL) { fclose(in); return 0; }

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);

    fclose(in);
    fclose(out);
    return 1;
}

void writeAdminLog(const char* who, const char* action)
{
    FILE* fp = fopen(FILE_ADMIN_LOG, "a");
    char ts[DATETIME_LEN];

    if (fp == NULL) return;
    getNow(ts);
    fprintf(fp, "[%s] %s %s\n", ts, who, action);
    fclose(fp);
}

int backupAllData(void)
{
    char info[256];
    char ts[DATETIME_LEN];
    int ok = 0;
    FILE* fp;

    _mkdir("data/backup");

    ok += copyFileBinary(FILE_USER, "data/backup/user.dat.bak");
    ok += copyFileBinary(FILE_CINEMA, "data/backup/cinema.dat.bak");
    ok += copyFileBinary(FILE_TICKET, "data/backup/ticket.dat.bak");
    ok += copyFileBinary(FILE_ADMIN, "data/backup/admin.dat.bak");

    if (ok < 4)
    {
        printf("备份失败：部分文件缺失。\n");
        return 0;
    }

    getNow(ts);
    sprintf(info, "备份时间：%s\n用户：%d\n电影：%d\n场次：%d\n订单：%d\n管理员：%d\n",
        ts, g_userCnt, g_movieCnt, g_schedCnt, g_ticketCnt, g_adminCnt);

    fp = fopen("data/backup/backup_info.txt", "w");
    if (fp) { fputs(info, fp); fclose(fp); }

    printf("备份完成。\n");
    writeAdminLog(g_currentAdminAccount, "备份数据");
    return 1;
}

int restoreAllData(void)
{
    int ok = 0;
    int confirm;

    printf("恢复最近备份会覆盖当前所有数据，是否继续？1=是，0=取消：");
    confirm = inputInt(0, 1);
    if (confirm == 0) { printf("已取消。\n"); return 0; }

    ok += copyFileBinary("data/backup/user.dat.bak", FILE_USER);
    ok += copyFileBinary("data/backup/cinema.dat.bak", FILE_CINEMA);
    ok += copyFileBinary("data/backup/ticket.dat.bak", FILE_TICKET);
    ok += copyFileBinary("data/backup/admin.dat.bak", FILE_ADMIN);

    if (ok < 4) { printf("恢复失败：备份不完整。\n"); return 0; }

    loadUsers();
    loadCinema();
    loadTickets();
    loadAdmins();

    writeAdminLog(g_currentAdminAccount, "恢复备份");

    g_currentAdminId = 0;
    g_currentAdminRole = 0;
    g_currentAdminAccount[0] = '\0';

    printf("恢复完成，请返回主菜单重新登录。\n");
    return 1;
}

void checkData(void)
{
    int i, r, c, seatSold, problems = 0;
    int t, sIdx;
    float ticketIncome, diff;

    printf("==== 数据一致性检查 ====\n");

    for (i = 0; i < g_schedCnt; i++)
    {
        seatSold = 0;
        for (r = 0; r < ROW_COUNT; r++)
            for (c = 0; c < COL_COUNT; c++)
                if (g_scheds[i].seat[r][c] == 1) seatSold++;

        if (seatSold != g_scheds[i].soldCnt)
        {
            printf("场次 %d：座位已售 %d，soldCnt=%d，不一致。\n",
                g_scheds[i].id, seatSold, g_scheds[i].soldCnt);
            problems++;
        }

        /* 净票房口径：income 应当等于该场次所有有效票（1 正常 / 4 改签票）票面之和。
           退票和改签都是整张冲减，手续费不进 income，所以这个等式必须严格成立 */
        ticketIncome = 0.0f;
        for (t = 0; t < g_ticketCnt; t++)
            if (g_tickets[t].schedId == g_scheds[i].id &&
                TICKET_VALID(g_tickets[t].state))
                ticketIncome += g_tickets[t].price;

        diff = g_scheds[i].income - ticketIncome;
        if (diff < 0) diff = -diff;
        if (diff > 0.05f)
        {
            printf("场次 %d：income=%.2f，有效票金额=%.2f，不一致。\n",
                g_scheds[i].id, g_scheds[i].income, ticketIncome);
            problems++;
        }
    }

    for (t = 0; t < g_ticketCnt; t++)
    {
        /* 改签票也是“要进场的票”，座位必须仍然标着已售 */
        if (!TICKET_VALID(g_tickets[t].state)) continue;

        sIdx = findSchedule(g_tickets[t].schedId);
        if (sIdx < 0)
        {
            printf("票 %d：有效票但场次不存在。\n", g_tickets[t].id);
            problems++;
            continue;
        }
        if (g_scheds[sIdx].seat[g_tickets[t].row][g_tickets[t].col] != 1)
        {
            printf("票 %d：有效票但座位未标记已售。\n", g_tickets[t].id);
            problems++;
        }
    }

    if (problems == 0)
        printf("一致性检查通过，未发现问题。\n");
    else
        printf("共发现 %d 处不一致。\n", problems);
}

