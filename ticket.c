/*==================================================================
  ticket.c  ——  购票 / 退票 / 我的订单（框架空壳）
==================================================================*/
#include "cinema.h"

int buyTicket(int uid)
{
    printf("【框架提示：购票（用户 #%d）】\n", uid);
    pauseScreen();
    return 1;
}

int refundTicket(int uid)
{
    printf("【框架提示：退票（用户 #%d）】\n", uid);
    pauseScreen();
    return 1;
}

void showMyTickets(int uid)
{
    printf("【框架提示：查看我的订单（用户 #%d）】\n", uid);
    pauseScreen();
}
