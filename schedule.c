/*==================================================================
  schedule.c  ——  排片与场次查询（框架空壳）
==================================================================*/
#include "cinema.h"

int addSchedule(void)
{
    printf("【框架提示：添加场次（选日期 + 选场次 + 选影片）】\n");
    pauseScreen();
    return 0;
}

void showSchedules(void)
{
    printf("【框架提示：查看场次列表】\n");
    pauseScreen();
}

int findSchedule(int id)
{
    printf("【框架提示：按编号查找场次（id=%d）】\n", id);
    return -1;
}

void showOnSale(void)
{
    printf("【框架提示：查看在售场次】\n");
    pauseScreen();
}

int pickSchedule(void)
{
    printf("【框架提示：选择一个场次】\n");
    pauseScreen();
    return -1;
}
