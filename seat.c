/*==================================================================
  seat.c  ——  座位图 + 座位状态
  规则：seat[r][c] == 0 空位，== 1 已售
        只有 seatSell / seatFree 能改这个值，别处一律只读
==================================================================*/
#include "cinema.h"

void showSeats(int sIdx)
{
    char mname[50];
    int  r, c, k;

    if (sIdx < 0 || sIdx >= g_schedCnt)
    {
        printf("\n  场次不存在。\n");
        return;
    }

    strcpy(mname, "未知影片");
    for (k = 0; k < g_movieCnt; k++)
        if (g_movies[k].id == g_scheds[sIdx].movieId)
        {
            strcpy(mname, g_movies[k].name); break;
        }

    printf("\n  《%s》  %s  %s（第%d场）  %d号厅  票价 %.2f 元\n",
        mname, g_scheds[sIdx].date, slotTime(g_scheds[sIdx].slot),
        g_scheds[sIdx].slot, g_scheds[sIdx].hallId, g_scheds[sIdx].price);

    printf("      ");                                    /* 与下面排号对齐 */
    for (c = 1; c <= COL_COUNT; c++) printf("%-2d ", c);
    printf("\n");

    for (r = 0; r < ROW_COUNT; r++)
    {
        printf("  %s   ", rowName(r));
        for (c = 0; c < COL_COUNT; c++)
            printf("%s", g_scheds[sIdx].seat[r][c] == 0 ? "□ " : "■ ");
        printf("\n");
    }

    printf("\n  图例：□ 空位   ■ 已售      余票 %d / %d\n",
        countLeft(sIdx), SEAT_TOTAL);
}

/* 数还剩多少个空座 */
int countLeft(int sIdx)
{
    int r, c, n = 0;

    if (sIdx < 0 || sIdx >= g_schedCnt) return 0;

    for (r = 0; r < ROW_COUNT; r++)
        for (c = 0; c < COL_COUNT; c++)
            if (g_scheds[sIdx].seat[r][c] == 0) n++;

    return n;
}

/* 占座：成功 1，位置非法或已售 0。不存盘、不动票房，由调用方负责 */
int seatSell(int sIdx, int row, int col)
{
    if (sIdx < 0 || sIdx >= g_schedCnt) return 0;
    if (row < 0 || row >= ROW_COUNT)    return 0;
    if (col < 0 || col >= COL_COUNT)    return 0;
    if (g_scheds[sIdx].seat[row][col] != 0) return 0;   /* 已经卖出去了 */

    g_scheds[sIdx].seat[row][col] = 1;
    g_scheds[sIdx].soldCnt++;
    return 1;
}

/* 退座：成功 1，位置非法或本来就空着 0 */
int seatFree(int sIdx, int row, int col)
{
    if (sIdx < 0 || sIdx >= g_schedCnt) return 0;
    if (row < 0 || row >= ROW_COUNT)    return 0;
    if (col < 0 || col >= COL_COUNT)    return 0;
    if (g_scheds[sIdx].seat[row][col] != 1) return 0;   /* 本来就没卖出去 */

    g_scheds[sIdx].seat[row][col] = 0;
    if (g_scheds[sIdx].soldCnt > 0) g_scheds[sIdx].soldCnt--;
    return 1;
}
