/*==================================================================
  seat.c  ——  座位图 + 座位状态
  规则：seat[r][c] == 0 空位，== 1 已售
        只有 seatSell / seatFree 能改这个值，别处一律只读
==================================================================*/
#include "cinema.h"

/* 画一个座位格：固定占 3 列（方块本身 g_cellW 列 + 补到 3 列的空白）。
   方块用 □/■ 而不是纯色块：控制台背景铺满整格、行间没留缝，
   纯色块上下会连成一条竖条，看不出是格子。 */
static void seatCell(int sold)
{
    int pad = 3 - g_cellW;
    if (pad < 1) pad = 1;

    if (g_colorOn)
        printf("%s□\x1b[0m%*s", sold ? "\x1b[1;31m" : "\x1b[1;32m", pad, "");
    else
        printf("%s%*s", sold ? "■" : "□", pad, "");
}

void showSeats(int sIdx)
{
    char mname[50];
    char bar[24];
    int  r, c, k, sold, left, fill;
    const char* C_RESET = g_colorOn ? "\x1b[0m"    : "";
    const char* C_CYAN  = g_colorOn ? "\x1b[1;36m" : "";
    const char* C_YELLOW= g_colorOn ? "\x1b[1;33m" : "";
    const char* C_GREEN = g_colorOn ? "\x1b[1;32m" : "";
    const char* C_RED   = g_colorOn ? "\x1b[1;31m" : "";

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

    sold = g_scheds[sIdx].soldCnt;
    if (sold < 0) sold = 0;
    if (sold > SEAT_TOTAL) sold = SEAT_TOTAL;
    left = SEAT_TOTAL - sold;

    printf("\n        %s================ 银  幕 ================%s\n", C_CYAN, C_RESET);
    printf("        《%s%s%s》  %s  %s  %d 号厅   票价 %.2f 元\n",
        C_YELLOW, mname, C_RESET,
        g_scheds[sIdx].date, slotTime(g_scheds[sIdx].slot),
        g_scheds[sIdx].hallId, g_scheds[sIdx].price);
    printf("\n");

    /* 列号：每格 3 列，和下面的方块严格对齐 */
    printf("      ");
    for (c = 1; c <= COL_COUNT; c++) printf("%-3d", c);
    printf("\n");

    for (r = 0; r < ROW_COUNT; r++)
    {
        printf("  %s   ", rowName(r));
        for (c = 0; c < COL_COUNT; c++)
            seatCell(g_scheds[sIdx].seat[r][c] != 0);
        printf("\n");
    }

    /* 上座率条：20 格，买过就至少亮 1 格，免得 1 个人看着像空场 */
    fill = (sold * 20 + SEAT_TOTAL - 1) / SEAT_TOTAL;
    if (sold > 0 && fill < 1) fill = 1;
    for (k = 0; k < 20; k++) bar[k] = (k < fill) ? '#' : '-';
    bar[20] = '\0';

    printf("\n  图例：");
    seatCell(0);
    printf("%s空位%s   ", C_GREEN, C_RESET);
    seatCell(1);
    printf("%s已售%s\n", C_RED, C_RESET);

    printf("  上座：%s[%s]%s  %d/%d   余票 %d 张\n",
        C_CYAN, bar, C_RESET, sold, SEAT_TOTAL, left);
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
