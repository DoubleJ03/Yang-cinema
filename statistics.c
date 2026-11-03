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
    int  idx[MAX_SCHEDULES];
    char mname[50];
    int  n = g_schedCnt, i, j, tmp, s;

    system("cls");
    printf("================ 票房排行 ================\n");

    if (n == 0)
    {
        printf("  还没有排过任何场次。\n");
        pauseScreen();
        return;
    }

    for (i = 0; i < n; i++) idx[i] = i;         /* 只排下标，不动原数组 */

    for (i = 0; i < n - 1; i++)                 /* 冒泡，从高到低 */
        for (j = 0; j < n - 1 - i; j++)
            if (g_scheds[idx[j]].income < g_scheds[idx[j + 1]].income)
            {
                tmp = idx[j];
                idx[j] = idx[j + 1];
                idx[j + 1] = tmp;
            }

    for (i = 0; i < n; i++)
    {
        s = idx[i];
        movieNameOf(g_scheds[s].movieId, mname);
        printf("  第%2d名  %8.2f元  《%s》  %s %s %d号厅  售%d张\n",
            i + 1, g_scheds[s].income, mname, g_scheds[s].date,
            slotTime(g_scheds[s].slot), g_scheds[s].hallId, g_scheds[s].soldCnt);
    }
    pauseScreen();
}
