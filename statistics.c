/*==================================================================
  statistics.c  ——  票房统计 + 排行
==================================================================*/
#include "cinema.h"

static int movieNameOf(int movieId, char* out)   /* 把片名抄进 out，找不到写“未知影片” */
{
    int i;
    for (i = 0; i < g_movieCnt; i++)
        if (g_movies[i].id == movieId)
        {
            strcpy(out, g_movies[i].name); return i;
        }
    strcpy(out, "未知影片");
    return -1;
}

void calculateBoxOffice(void)
{
    char mname[50];
    int  i, sold = 0;
    float total = 0.0f;

    system("cls");
    printf("================ 票房统计 ================\n");

    if (g_schedCnt == 0)
    {
        printf("  还没有排过任何场次。\n");
        pauseScreen();
        return;
    }

    for (i = 0; i < g_schedCnt; i++)
    {
        movieNameOf(g_scheds[i].movieId, mname);
        printf("  %s %-5s %d号厅 《%s》  售%3d/%3d  %.2f元\n",
            g_scheds[i].date, slotTime(g_scheds[i].slot), g_scheds[i].hallId,
            mname, g_scheds[i].soldCnt, SEAT_TOTAL, g_scheds[i].income);

        sold += g_scheds[i].soldCnt;
        total += g_scheds[i].income;
    }

    printf("  ------------------------------------------------\n");
    printf("  场次 %d 场   售出 %d 张   总票房 %.2f 元\n",
        g_schedCnt, sold, total);
    printf("  平均上座率 %.1f%%\n",
        sold * 100.0 / (g_schedCnt * (double)SEAT_TOTAL));
    pauseScreen();
}

void sortBoxOffice(void)
{
    float income[MAX_MOVIES];
    int   sold[MAX_MOVIES];
    int   idx[MAX_MOVIES];
    int   n = g_movieCnt, i, j, tmp, m;

    system("cls");
    printf("================ 票房排行 ================\n");

    if (n == 0)
    {
        printf("  还没有任何电影。\n");
        pauseScreen();
        return;
    }

    for (i = 0; i < n; i++)
    {
        income[i] = 0.0f;
        sold[i] = 0;
        idx[i] = i;
    }

    /* 按电影汇总票房和售票数 */
    for (i = 0; i < g_schedCnt; i++)
    {
        m = findMovie(g_scheds[i].movieId);
        if (m >= 0)
        {
            income[m] += g_scheds[i].income;
            sold[m]   += g_scheds[i].soldCnt;
        }
    }

    /* 冒泡排序：票房从高到低 */
    for (i = 0; i < n - 1; i++)
        for (j = 0; j < n - 1 - i; j++)
            if (income[idx[j]] < income[idx[j + 1]])
            {
                tmp = idx[j];
                idx[j] = idx[j + 1];
                idx[j + 1] = tmp;
            }

    for (i = 0; i < n; i++)
    {
        m = idx[i];
        printf("  第%2d名  %8.2f元  《%s》  售%d张\n",
            i + 1, income[m], g_movies[m].name, sold[m]);
    }
    pauseScreen();
}



/* ==================== CSV 导出 ==================== */

/* 写一个 CSV 字段，含逗号/引号时加引号转义 */
static void csvField(FILE* fp, const char* s)
{
    int needQuote = 0;
    const char* p;

    for (p = s; *p; p++)
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r')
        { needQuote = 1; break; }

    if (needQuote)
    {
        fputc('"', fp);
        for (p = s; *p; p++)
        {
            if (*p == '"') fputc('"', fp);
            fputc(*p, fp);
        }
        fputc('"', fp);
    }
    else
    {
        fputs(s, fp);
    }
}

/* 生成时间串，用于文件名 */
static void getTimeStr(char* out)
{
    time_t t = time(NULL);
    struct tm* p = localtime(&t);
    sprintf(out, "%04d%02d%02d_%02d%02d%02d",
        p->tm_year + 1900, p->tm_mon + 1, p->tm_mday,
        p->tm_hour, p->tm_min, p->tm_sec);
}

static void userNameOf(int uid, char* out)
{
    int i;

    for (i = 0; i < g_userCnt; i++)
        if (g_users[i].id == uid) { strcpy(out, g_users[i].name); return; }

    sprintf(out, "用户#%d", uid);
}

static void adminAccountOf(int aid, char* out)
{
    int i;

    if (aid == 0) { strcpy(out, "用户"); return; }

    for (i = 0; i < g_adminCnt; i++)
        if (g_admins[i].id == aid) { strcpy(out, g_admins[i].account); return; }

    sprintf(out, "管理员#%d", aid);
}

void exportOrdersCSV(void)
{
    FILE* fp;
    char path[128];
    char ts[32];
    char uname[20], op[20];
    int i;

    getTimeStr(ts);
    sprintf(path, "data/orders_%s.csv", ts);

    fp = fopen(path, "w");
    if (fp == NULL) { printf("导出失败，无法创建 %s。\n", path); return; }

    fprintf(fp, "票号,用户编号,用户名,电影,观影日期,时段,影厅,排,座,票价,状态,购票时间,退票时间,退票操作员\n");

    for (i = 0; i < g_ticketCnt; i++)
    {
        Ticket* t = &g_tickets[i];
        userNameOf(t->userId, uname);
        adminAccountOf(t->refund_admin_id, op);

        fprintf(fp, "%d,%d,", t->id, t->userId);
        csvField(fp, uname); fputc(',', fp);
        csvField(fp, t->movieName); fputc(',', fp);
        csvField(fp, t->date); fputc(',', fp);
        fprintf(fp, "%d,%d,%d,%d,%.2f,", t->slot, t->hallId, t->row + 1, t->col + 1, t->price);
        csvField(fp, t->state == 1 ? "有效" : "已退"); fputc(',', fp);
        csvField(fp, t->purchase_time); fputc(',', fp);
        csvField(fp, t->refund_time[0] == '\0' ? "" : t->refund_time); fputc(',', fp);
        csvField(fp, op);
        fputc('\n', fp);
    }
    fclose(fp);

    printf("已导出 %d 条订单到 %s\n", g_ticketCnt, path);
}

void exportMovieRevenueCSV(void)
{
    FILE* fp;
    char path[128];
    char ts[32];
    float revenue[MAX_MOVIES];
    int sold[MAX_MOVIES];
    int i, sIdx, m;

    getTimeStr(ts);
    sprintf(path, "data/movie_revenue_%s.csv", ts);

    for (i = 0; i < MAX_MOVIES; i++) { revenue[i] = 0.0f; sold[i] = 0; }

    for (i = 0; i < g_ticketCnt; i++)
    {
        if (g_tickets[i].state != 1) continue;
        sIdx = findSchedule(g_tickets[i].schedId);
        if (sIdx < 0) continue;
        m = findMovie(g_scheds[sIdx].movieId);
        if (m < 0) continue;
        sold[m]++;
        revenue[m] += g_tickets[i].price;
    }

    fp = fopen(path, "w");
    if (fp == NULL) { printf("导出失败，无法创建 %s。\n", path); return; }

    fprintf(fp, "电影编号,电影名,售票数,净票房\n");
    for (i = 0; i < g_movieCnt; i++)
    {
        fprintf(fp, "%d,", g_movies[i].id);
        csvField(fp, g_movies[i].name); fputc(',', fp);
        fprintf(fp, "%d,%.2f\n", sold[i], revenue[i]);
    }
    fclose(fp);

    printf("已导出电影票房到 %s\n", path);
}

void exportScheduleRevenueCSV(void)
{
    FILE* fp;
    char path[128];
    char ts[32];
    char mname[50];
    int i;

    getTimeStr(ts);
    sprintf(path, "data/schedule_revenue_%s.csv", ts);

    fp = fopen(path, "w");
    if (fp == NULL) { printf("导出失败，无法创建 %s。\n", path); return; }

    fprintf(fp, "场次号,日期,时段,影厅,电影,已售,总座位,上座率,票房\n");

    for (i = 0; i < g_schedCnt; i++)
    {
        movieNameOf(g_scheds[i].movieId, mname);
        fprintf(fp, "%d,", g_scheds[i].id);
        csvField(fp, g_scheds[i].date); fputc(',', fp);
        csvField(fp, slotTime(g_scheds[i].slot)); fputc(',', fp);
        fprintf(fp, "%d,", g_scheds[i].hallId);
        csvField(fp, mname); fputc(',', fp);
        fprintf(fp, "%d,%d,%.1f%%,%.2f\n",
            g_scheds[i].soldCnt, SEAT_TOTAL,
            g_scheds[i].soldCnt * 100.0 / SEAT_TOTAL,
            g_scheds[i].income);
    }
    fclose(fp);

    printf("已导出场次票房到 %s\n", path);
}

void exportCSVMenu(void)
{
    int op;

    printf("\n1. 订单明细\n");
    printf("2. 电影票房\n");
    printf("3. 场次票房\n");
    printf("0. 返回\n");
    printf("请选择 [0-3]：");
    op = inputInt(0, 3);

    if (op == 1) exportOrdersCSV();
    if (op == 2) exportMovieRevenueCSV();
    if (op == 3) exportScheduleRevenueCSV();
}



/* ==================== 日期区间统计 ==================== */

void calculateBoxOfficeRange(void)
{
    char startDate[11], endDate[11];
    int startDay, endDay;
    int i, sold = 0, schedInRange = 0;
    float total = 0.0f;
    char mname[50];

    printf("起始日期偏移（0=今天）：");
    startDay = inputInt(0, DAY_NUM - 1);
    printf("结束日期偏移（%d~%d）：", startDay, DAY_NUM - 1);
    endDay = inputInt(startDay, DAY_NUM - 1);

    addDays(startDate, startDay);
    addDays(endDate, endDay);

    system("cls");
    printf("==== 票房统计（%s ~ %s）====\n", startDate, endDate);

    for (i = 0; i < g_schedCnt; i++)
    {
        if (strcmp(g_scheds[i].date, startDate) < 0) continue;
        if (strcmp(g_scheds[i].date, endDate) > 0) continue;

        movieNameOf(g_scheds[i].movieId, mname);
        printf("  %s %-5s %d号厅 《%s》  售%3d/%3d  %.2f元\n",
            g_scheds[i].date, slotTime(g_scheds[i].slot), g_scheds[i].hallId,
            mname, g_scheds[i].soldCnt, SEAT_TOTAL, g_scheds[i].income);

        sold += g_scheds[i].soldCnt;
        total += g_scheds[i].income;
        schedInRange++;
    }

    printf("  ------------------------------------------------\n");
    printf("  区间场次 %d 场   售出 %d 张   票房 %.2f 元\n",
        schedInRange, sold, total);
    if (schedInRange > 0)
        printf("  平均上座率 %.1f%%\n",
            sold * 100.0 / (schedInRange * (double)SEAT_TOTAL));
    pauseScreen();
}

