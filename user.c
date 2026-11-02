/*==================================================================
  user.c  ——  用户注册 / 登录 / 观众菜单
==================================================================*/
#include "cinema.h"

int registerUser(void)
{
    char name[20];
    char pwd[20];

    system("cls");
    printf("============ 用户注册 ============\n");
    printf("请输入新用户名：");
    inputStr(name, sizeof(name));
    printf("请输入新密码：");
    inputStr(pwd, sizeof(pwd));

    printf("\n【框架提示：用户注册】\n");
    printf("账号 %s 已接收，框架版不写入文件。\n", name);
    pauseScreen();
    return 1;
}

int loginUser(char* name, int* uid)
{
    char pwd[20];

    system("cls");
    printf("============ 用户登录 ============\n");
    printf("请输入用户名：");
    inputStr(name, 20);
    printf("请输入密码：");
    inputStr(pwd, sizeof(pwd));

    *uid = 1;
    printf("\n【框架提示：用户登录校验】\n");
    printf("框架版不做校验，欢迎 %s。\n", name);
    pauseScreen();
    return 1;
}

void userMenu(int uid, const char* name)
{
    int op;

    while (1)
    {
        system("cls");
        printf("==============================================\n");
        printf("              观众菜单\n");
        printf("              当前用户：%s\n", name);
        printf("==============================================\n");
        printf("   1. 注册\n");
        printf("   2. 查看在售场次\n");
        printf("   3. 购票\n");
        printf("   4. 退票\n");
        printf("   5. 查看我的订单\n");
        printf("   0. 返回主菜单\n");
        printf("----------------------------------------------\n");
        printf("请选择 [0-5]：");

        op = inputInt(0, 5);

        switch (op)
        {
        case 1: registerUser();       break;
        case 2: showOnSale();         break;
        case 3: buyTicket(uid);       break;
        case 4: refundTicket(uid);    break;
        case 5: showMyTickets(uid);   break;
        case 0: return;
        }
    }
}
