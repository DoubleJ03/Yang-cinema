
/*==================================================================
  user.c  ——  用户注册 / 登录 / 观众菜单
  ------------------------------------------------------------------
  负责人：B
  说明：
    1) 本文件只放"账号相关"和"观众菜单"。
    2) 场次列表（showOnSale / pickSchedule）是 A 写在 schedule.c 里的，
       B 直接调用，不要在本文件里再实现一遍，否则 LNK2005 重复定义。
    3) 每个功能函数内部自己 pauseScreen()，观众菜单不再重复暂停，
       否则每点一次要按两下回车。
==================================================================*/
#include "cinema.h"

/*-------------------- 1. 用户注册 --------------------*/
/* 返回：1 注册成功；0 已满 / 重名 / 两次密码不一致 */
int registerUser(void)
{
    char uname[20];
    char pwd1[20];
    char pwd2[20];
    int  i;

    system("cls");
    printf("=============== 用户注册 ===============\n");

    /* ① 容量检查：数组写满就没地方放了 */
    if (g_userCnt >= MAX_USERS)
    {
        printf("用户数已达上限（%d 个），暂时无法注册。\n", MAX_USERS);
        pauseScreen();
        return 0;
    }

    /* ② 用户名：inputStr 已保证非空且不超长 */
    printf("请输入用户名（最多 6 个汉字）：");
    inputStr(uname, sizeof(uname));

    /* ③ 查重：把已有账号全扫一遍 */
    for (i = 0; i < g_userCnt; i++)
    {
        if (strcmp(g_users[i].name, uname) == 0)
        {
            printf("\n该用户名已被注册，请换一个。\n");
            pauseScreen();
            return 0;
        }
    }

    /* ④ 密码输两遍，防手抖 */
    printf("请设置密码：");
    inputStr(pwd1, sizeof(pwd1));
    printf("请再输一次密码：");
    inputStr(pwd2, sizeof(pwd2));

    if (strcmp(pwd1, pwd2) != 0)
    {
        printf("\n两次输入的密码不一致，注册失败。\n");
        pauseScreen();
        return 0;
    }

    /* ⑤ 写入内存：id 由 newUserId() 算，永远不会重复 */
    g_users[g_userCnt].id = newUserId();
    strcpy(g_users[g_userCnt].name, uname);
    strcpy(g_users[g_userCnt].pwd, pwd1);
    g_userCnt++;

    /* ⑥ 改完立刻存盘。别攒到退出时统一保存，
          中途关窗口或崩溃，这个账号就白注册了。 */
    saveUsers();

    printf("\n注册成功！你的用户编号是 %d，现在可以登录了。\n",
           g_users[g_userCnt - 1].id);
    pauseScreen();
    return 1;
}

/*-------------------- 2. 用户登录 --------------------*/
/* 返回：1 成功（回填 name 和 uid）；0 失败 */
int loginUser(char *name, int *uid)
{
    char uname[20];
    char pwd[20];
    int  i;

    /* 先清干净：main 里那个 uname 数组是复用的，
       上一次登录的名字还留在里面，不清会串号 */
    name[0] = '\0';
    *uid = 0;

    system("cls");
    printf("=============== 用户登录 ===============\n");

    printf("请输入用户名：");
    inputStr(uname, sizeof(uname));
    printf("请输入密码：");
    inputStr(pwd, sizeof(pwd));

    for (i = 0; i < g_userCnt; i++)
    {
        if (strcmp(g_users[i].name, uname) == 0 &&
            strcmp(g_users[i].pwd, pwd)  == 0)
        {
            strncpy(name, g_users[i].name, 19);   /* 目标只有 20 字节，别溢出 */
            name[19] = '\0';
            *uid = g_users[i].id;

            printf("\n登录成功，欢迎回来，%s！\n", name);
            pauseScreen();
            return 1;
        }
    }

    /* 失败只给一句话，不区分"没这个用户"还是"密码错"，
       否则别人可以拿它试探哪些账号存在 */
    printf("\n用户名或密码错误。\n");
    pauseScreen();
    return 0;
}

/*-------------------- 3. 观众菜单 --------------------*/
/* uid：当前用户编号（购票/退票/查记录都要用）
   name：当前用户名（只用于显示） */
void userMenu(int uid, const char *name)
{
    int op;
    int sIdx;

    while (1)
    {
        system("cls");
        printf("==============================================\n");
        printf("  观众：%s    今天 %s\n", name, g_today);
        printf("==============================================\n");
        printf("   1. 浏览在售场次\n");
        printf("   2. 查看座位图\n");
        printf("   3. 购票\n");
        printf("   4. 退票\n");
        printf("   5. 我的购票记录\n");
        printf("   0. 返回主菜单\n");
        printf("----------------------------------------------\n");
        printf("请选择 [0-5]：");
        op = inputInt(0, 5);

        if (op == 0) return;

        switch (op)
        {
        case 1:                                  /* A 写在 schedule.c */
            showOnSale();
            break;

        case 2:                                  /* 先选场次，再看座位 */
            sIdx = pickSchedule();               /* 取消返回 -1 */
            if (sIdx >= 0)
                showSeats(sIdx);
            break;

        case 3:
            buyTicket(uid);                      /* 写在 ticket.c */
            break;

        case 4:
            refundTicket(uid);                   /* 写在 ticket.c */
            break;

        case 5:
            showMyTickets(uid);                  /* 写在 ticket.c */
            break;
        }

        /* 这里故意不写 pauseScreen()：
           上面每个功能函数内部都已经暂停过一次，
           再暂停一次就成了"要点两下回车才回菜单" */
    }
}
