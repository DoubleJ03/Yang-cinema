#define _CRT_SECURE_NO_WARNINGS
#include "cinema.h"
#include <stdio.h>
#include <string.h>

/* 管理员登录：账号+密码，成功设置当前管理员会话，返回 1 */
int adminLogin(void)
{
    char account[20];
    char pwd[20];
    int  i;

    system("cls");
    printf("============ 管理员登录 ============\n");
    printf("请输入账号：");
    inputStr(account, sizeof(account));
    printf("请输入密码：");
    inputPassword(pwd, sizeof(pwd));   /* 显示 * ，不露密码 */

    for (i = 0; i < g_adminCnt; i++)
    {
        if (strcmp(g_admins[i].account, account) == 0 &&
            strcmp(g_admins[i].pwd, pwd) == 0)
        {
            if (g_admins[i].status == 0)
            {
                printf("\n该账号已被禁用。\n");
                writeAdminLog(account, "登录失败（账号已禁用）");
                pauseScreen();
                return 0;
            }

            g_currentAdminId = g_admins[i].id;
            g_currentAdminRole = g_admins[i].role;
            strcpy(g_currentAdminAccount, g_admins[i].account);
            getNow(g_admins[i].last_login);
            saveAdmins();

            printf("\n登录成功，欢迎 %s。\n", account);
            writeAdminLog(account, "登录成功");
            pauseScreen();
            return 1;
        }
    }

    printf("\n账号或密码错误。\n");
    writeAdminLog(account, "登录失败");
    pauseScreen();
    return 0;
}

/* 按编号查找管理员，返回数组下标，找不到返回 -1 */
static int findAdminById(int id)
{
    int i;

    for (i = 0; i < g_adminCnt; i++)
        if (g_admins[i].id == id) return i;

    return -1;
}

/* 统计超级管理员数量 */
static int countSuper(void)
{
    int i, n = 0;

    for (i = 0; i < g_adminCnt; i++)
        if (g_admins[i].role == 1) n++;

    return n;
}

void adminList(void)
{
    int i;

    system("cls");
    printf("============ 管理员列表 ============\n");
    if (g_adminCnt == 0)
    {
        printf("暂无管理员。\n");
        return;
    }

    printf("编号\t账号\t角色\t状态\t创建时间\t\t最后登录\n");
    for (i = 0; i < g_adminCnt; i++)
    {
        printf("%d\t%s\t%s\t%s\t%s\t%s\n",
            g_admins[i].id,
            g_admins[i].account,
            g_admins[i].role == 1 ? "超级" : "普通",
            g_admins[i].status == 1 ? "正常" : "禁用",
            g_admins[i].create_time,
            g_admins[i].last_login[0] == '\0' ? "—" : g_admins[i].last_login);
    }
}

int adminAdd(void)
{
    char account[20];
    char pwd1[20];
    char pwd2[20];
    int  i;

    if (g_adminCnt >= MAX_ADMINS)
    {
        printf("管理员数量已满。\n");
        return 0;
    }

    printf("请输入新管理员账号：");
    inputStr(account, sizeof(account));

    for (i = 0; i < g_adminCnt; i++)
        if (strcmp(g_admins[i].account, account) == 0)
        {
            printf("该账号已存在。\n");
            return 0;
        }

    printf("请设置密码：");
    inputStr(pwd1, sizeof(pwd1));
    printf("请再次输入密码：");
    inputStr(pwd2, sizeof(pwd2));

    if (strcmp(pwd1, pwd2) != 0)
    {
        printf("两次密码不一致。\n");
        return 0;
    }

    g_admins[g_adminCnt].id = newAdminId();
    strcpy(g_admins[g_adminCnt].account, account);
    strcpy(g_admins[g_adminCnt].pwd, pwd1);
    g_admins[g_adminCnt].role = 2;
    g_admins[g_adminCnt].status = 1;
    getNow(g_admins[g_adminCnt].create_time);
    g_admins[g_adminCnt].last_login[0] = '\0';
    g_adminCnt++;

    saveAdmins();
    printf("新增成功，管理员编号：%d\n", g_admins[g_adminCnt - 1].id);
    writeAdminLog(g_currentAdminAccount, "新增管理员");
    return 1;
}

int adminResetPwd(void)
{
    int id, idx;
    char pwd[20];

    printf("请输入要重置密码的管理员编号：");
    id = inputInt(1, 1000000);

    idx = findAdminById(id);
    if (idx == -1)
    {
        printf("管理员不存在。\n");
        return 0;
    }

    printf("请输入新密码：");
    inputStr(pwd, sizeof(pwd));

    strcpy(g_admins[idx].pwd, pwd);
    saveAdmins();
    printf("已重置管理员 %s 的密码。\n", g_admins[idx].account);
    writeAdminLog(g_currentAdminAccount, "重置管理员密码");
    return 1;
}

int adminSetStatus(int enable)
{
    int id, idx;

    printf("请输入管理员编号：");
    id = inputInt(1, 1000000);

    idx = findAdminById(id);
    if (idx == -1)
    {
        printf("管理员不存在。\n");
        return 0;
    }

    if (g_admins[idx].id == g_currentAdminId)
    {
        printf("不能修改自己的登录状态。\n");
        return 0;
    }

    if (enable == 0 && g_admins[idx].role == 1 && countSuper() <= 1)
    {
        printf("不能禁用最后一个超级管理员。\n");
        return 0;
    }

    g_admins[idx].status = enable;
    saveAdmins();
    printf("已将 %s %s。\n", g_admins[idx].account, enable ? "恢复登录" : "禁用登录");
    writeAdminLog(g_currentAdminAccount, enable ? "恢复管理员登录" : "禁用管理员登录");
    return 1;
}

int adminDelete(void)
{
    int id, idx, j;

    printf("请输入要删除的管理员编号：");
    id = inputInt(1, 1000000);

    idx = findAdminById(id);
    if (idx == -1)
    {
        printf("管理员不存在。\n");
        return 0;
    }

    if (g_admins[idx].id == g_currentAdminId)
    {
        printf("不能删除自己。\n");
        return 0;
    }

    if (g_admins[idx].role == 1 && countSuper() <= 1)
    {
        printf("不能删除最后一个超级管理员。\n");
        return 0;
    }

    for (j = idx; j < g_adminCnt - 1; j++)
        g_admins[j] = g_admins[j + 1];
    g_adminCnt--;

    saveAdmins();
    printf("已删除管理员。\n");
    writeAdminLog(g_currentAdminAccount, "删除管理员");
    return 1;
}

/* 电影管理子菜单 */
static void adminMovieMenu(void)
{
    int op;

    while (1)
    {
        system("cls");
        printf("============== 电影管理 ==============\n");
        printf("   1. 查看电影\n");
        printf("   2. 添加电影\n");
        printf("   3. 批量添加电影\n");
        printf("   4. 批量删除电影\n");
        printf("   0. 返回\n");
        printf("请选择 [0-4]：");
        op = inputInt(0, 4);

        switch (op)
        {
        case 1: showMovies(); break;
        case 2: addMovie(); pauseScreen(); break;
        case 3: addMoviesBatch(); pauseScreen(); break;
        case 4: deleteMoviesBatch(); pauseScreen(); break;
        case 0: return;
        }
    }
}

/* 排片管理子菜单 */
static void adminScheduleMenu(void)
{
    int op;

    while (1)
    {
        system("cls");
        printf("============== 排片管理 ==============\n");
        printf("   1. 查看场次\n");
        printf("   2. 添加场次\n");
        printf("   3. 批量添加场次\n");
        printf("   4. 批量删除场次\n");
        printf("   0. 返回\n");
        printf("请选择 [0-4]：");
        op = inputInt(0, 4);

        switch (op)
        {
        case 1: showSchedules(); break;
        case 2: addSchedule(); pauseScreen(); break;
        case 3: addSchedulesBatch(); pauseScreen(); break;
        case 4: deleteSchedulesBatch(); pauseScreen(); break;
        case 0: return;
        }
    }
}

/* 订单管理子菜单 */
static void adminOrderMenu(void)
{
    int op;

    while (1)
    {
        system("cls");
        printf("============== 订单管理 ==============\n");
        printf("   1. 全部订单\n");
        printf("   2. 按用户查看\n");
        printf("   3. 批量退票\n");
        printf("   0. 返回\n");
        printf("请选择 [0-3]：");
        op = inputInt(0, 3);

        switch (op)
        {
        case 1: showAllOrders(); pauseScreen(); break;
        case 2: showOrdersByUser(); pauseScreen(); break;
        case 3: batchRefundMenu(); pauseScreen(); break;
        case 0: return;
        }
    }
}

/* 统计报表子菜单 */
static void adminStatsMenu(void)
{
    int op;

    while (1)
    {
        system("cls");
        printf("============== 统计报表 ==============\n");
        printf("   1. 票房统计\n");
        printf("   2. 票房排序\n");
        printf("   3. 导出 CSV\n");
        printf("   4. 日期区间统计\n");
        printf("   0. 返回\n");
        printf("请选择 [0-4]：");
        op = inputInt(0, 4);

        switch (op)
        {
        case 1: calculateBoxOffice(); break;
        case 2: sortBoxOffice(); break;
        case 3: exportCSVMenu(); pauseScreen(); break;
        case 4: calculateBoxOfficeRange(); pauseScreen(); break;
        case 0: return;
        }
    }
}

/* 系统管理子菜单（仅超级管理员） */
static void adminSystemMenu(void)
{
    int op;

    while (1)
    {
        system("cls");
        printf("============== 系统管理 ==============\n");
        printf("   1. 备份数据\n");
        printf("   2. 恢复最近备份\n");
        printf("   3. 操作日志\n");
        printf("   4. 数据一致性检查\n");
        printf("   0. 返回\n");
        printf("请选择 [0-4]：");
        op = inputInt(0, 4);

        switch (op)
        {
        case 1: backupAllData(); pauseScreen(); break;
        case 2: restoreAllData(); pauseScreen(); break;
        case 3: showAdminLog(); pauseScreen(); break;
        case 4: checkData(); pauseScreen(); break;
        case 0: return;
        }
    }
}

/* 管理员管理子菜单（仅超级管理员） */
static void adminManageMenu(void)
{
    int op;

    while (1)
    {
        system("cls");
        printf("============== 管理员管理 ==============\n");
        printf("   1. 管理员列表\n");
        printf("   2. 新增普通管理员\n");
        printf("   3. 重置密码\n");
        printf("   4. 禁用登录\n");
        printf("   5. 恢复登录\n");
        printf("   6. 删除管理员\n");
        printf("   0. 返回\n");
        printf("请选择 [0-6]：");
        op = inputInt(0, 6);

        switch (op)
        {
        case 1: adminList(); pauseScreen(); break;
        case 2: adminAdd(); pauseScreen(); break;
        case 3: adminResetPwd(); pauseScreen(); break;
        case 4: adminSetStatus(0); pauseScreen(); break;
        case 5: adminSetStatus(1); pauseScreen(); break;
        case 6: adminDelete(); pauseScreen(); break;
        case 0: return;
        }
    }
}

/* 管理员主菜单（二级菜单入口） */
void adminMenu(void)
{
    int op;
    int max = (g_currentAdminRole == 1) ? 6 : 4;

    while (1)
    {
        system("cls");
        printf("==============================================\n");
        printf("                管理员菜单\n");
        printf("==============================================\n");
        printf("   1. 电影管理\n");
        printf("   2. 排片管理\n");
        printf("   3. 订单管理\n");
        printf("   4. 统计报表\n");
        if (g_currentAdminRole == 1)
        {
            printf("   5. 系统管理\n");
            printf("   6. 管理员管理\n");
        }
        printf("   0. 返回主菜单\n");
        printf("----------------------------------------------\n");
        printf("请选择 [0-%d]：", max);

        op = inputInt(0, max);

        switch (op)
        {
        case 1: adminMovieMenu(); break;
        case 2: adminScheduleMenu(); break;
        case 3: adminOrderMenu(); break;
        case 4: adminStatsMenu(); break;
        case 5: if (g_currentAdminRole == 1) adminSystemMenu(); break;
        case 6: if (g_currentAdminRole == 1) adminManageMenu(); break;
        case 0: return;
        }
    }
}



void showAdminLog(void)
{
    FILE* fp = fopen(FILE_ADMIN_LOG, "r");
    char line[512];

    if (fp == NULL)
    {
        printf("还没有操作日志。\n");
        return;
    }

    system("cls");
    printf("============ 操作日志 ============\n");
    while (fgets(line, sizeof(line), fp) != NULL)
        printf("%s", line);
    fclose(fp);
}

