/*==================================================================
  seat.c  ——  座位图与座位状态（框架空壳）
==================================================================*/
#include "cinema.h"

void showSeats(int sIdx)
{
    printf("【框架提示：显示 10×10 座位图（场次 #%d）】\n", sIdx);
    pauseScreen();
}

int countLeft(int sIdx)
{
    printf("【框架提示：查询余票（场次 #%d）】\n", sIdx);
    return SEAT_TOTAL;
}

int seatSell(int sIdx, int row, int col)
{
    printf("【框架提示：占座（场次 #%d，第 %d 排 %d 座）】\n", sIdx, row + 1, col + 1);
    return 1;
}

int seatFree(int sIdx, int row, int col)
{
    printf("【框架提示：释放座位（场次 #%d，第 %d 排 %d 座）】\n", sIdx, row + 1, col + 1);
    return 1;
}
