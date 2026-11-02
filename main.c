/*==================================================================
  main.c  ——  全局变量 + 主循环 + 工具函数
  框架预览版：不读写任何文件，不创建 data 目录
==================================================================*/
#include "cinema.h"

/*---------------- 全局变量定义 ----------------*/
User     g_users[MAX_USERS];
int      g_userCnt = 0;

Movie    g_movies[MAX_MOVIES];
int      g_movieCnt = 0;

Schedule g_scheds[MAX_SCHEDULES];
int      g_schedCnt = 0;

Ticket   g_tickets[MAX_TICKETS];
int      g_ticketCnt = 0;

char     g_today[11] = "0000-00-00";   /* 框架版不取系统时间 */

/*---------------- 主菜单 ----------------*/
void showMainMenu(void)
{
    printf("==============================================\n");
    printf("           模拟电影院售票系统\n");
    printf("           （界面框架预览版）\n");
    printf("==============================================\n");
    printf("   1. 管理员登录\n");
    printf("   2. 用户注册\n");
    printf("   3. 用户登录\n");
    printf("   0. 退出系统\n");
    printf("----------------------------------------------\n");
    printf("请选择 [0-3]：");
}

/*---------------- 工具函数 ----------------*/
/* 读一个 [lo,hi] 区间内的整数，非法输入自动重读 */
int inputInt(int lo, int hi)
{
    char buf[64];
    int  v;

    while (1)
    {
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            return lo;
        if (sscanf(buf, "%d", &v) == 1 && v >= lo && v <= hi)
            return v;
        printf("  输入无效，请输入 %d ~ %d 之间的整数：", lo, hi);
    }
}

/* 读一行非空字符串，长度不够则要求重输 */
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
        tmp[strcspn(tmp, "\r\n")] = '\0';
        len = (int)strlen(tmp);
        if (len == 0)
        {
            printf("  不能为空，请重新输入：");
            continue;
        }
        if (len >= size)
        {
            printf("  太长了（最多 %d 字节），请重新输入：", size - 1);
            continue;
        }
        strcpy(buf, tmp);
        return;
    }
}

/* 按回车继续：先清掉缓冲区残留，避免"一闪而过" */
void pauseScreen(void)
{
    printf("\n按回车继续...");
    fflush(stdin);
    getchar();
}

/*---------------- 主函数 ----------------*/
int main(void)
{
    int  op;
    int  uid;
    char uname[20];

    while (1)
    {
        system("cls");
        showMainMenu();
        op = inputInt(0, 3);

        switch (op)
        {
        case 1:
            if (adminLogin())
                adminMenu();
            break;

        case 2:
            registerUser();
            break;

        case 3:
            uname[0] = '\0';
            uid = 0;
            if (loginUser(uname, &uid))
                userMenu(uid, uname);
            break;

        case 0:
            system("cls");
            printf("感谢使用，再见！\n");
            return 0;
        }
    }
    return 0;
}
