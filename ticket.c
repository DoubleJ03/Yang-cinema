/*==================================================================
  ticket.c —— 购票 / 退票 / 我的订单
  对外只有三个函数，别的一概不写
==================================================================*/
#include "cinema.h"
#include <string.h>

/*------------------------------------------------------------------
  1. 购票
  成功返回 1，取消或失败返回 0
------------------------------------------------------------------*/
int buyTicket(int uid)
{
    int sIdx, r, c, i, left, used = 0;
    char mname[50] = "";
    Ticket* t;

    /* ① 订单表满了就别再卖了，数组会越界 */
    if (g_ticketCnt >= MAX_TICKETS) {
        printf("\n  订单表已满（上限 %d 条），暂时无法出票。\n", MAX_TICKETS);
        pauseScreen();
        return 0;
    }

    /* ② 选场次，返回数组下标，-1 表示放弃 */
    sIdx = pickSchedule();
    if (sIdx < 0) {
        printf("\n  已取消购票。\n");
        pauseScreen();
        return 0;
    }

    /* ③ 限购：数这场里这人还有几张有效票 */
    for (i = 0; i < g_ticketCnt; i++) {
        if (g_tickets[i].userId == uid &&
            g_tickets[i].schedId == g_scheds[sIdx].id &&
            g_tickets[i].state == 1) {
            used++;
        }
    }
    if (used >= MAX_PER_USER) {
        printf("\n  同一场次每人最多买 %d 张，你已经买了 %d 张。\n",
            MAX_PER_USER, used);
        pauseScreen();
        return 0;
    }

    /* ④ 看座位图，顺便判断有没有空位 */
    showSeats(sIdx);
    left = countLeft(sIdx);
    if (left <= 0) {
        printf("\n  这一场已经满座了。\n");
        pauseScreen();
        return 0;
    }
    printf("\n  剩余空座：%d 个，请挑一个。\n", left);

    /* ⑤ 输入排号座号，-1 放弃 */
    printf("  排号 1 ~ %d（输入 -1 放弃购票）：", ROW_COUNT);
    r = inputInt(-1, ROW_COUNT);
    if (r < 0) {
        printf("\n  已取消购票。\n");
        pauseScreen();
        return 0;
    }

    printf("  座号 1 ~ %d（输入 -1 放弃购票）：", COL_COUNT);
    c = inputInt(-1, COL_COUNT);
    if (c < 0) {
        printf("\n  已取消购票。\n");
        pauseScreen();
        return 0;
    }

    r--;  c--;          /* 用户从 1 数起，数组从 0 数起 */

    /* ⑥ 占座：seatSell 负责改 seat 数组和 soldCnt，返回 0 代表被人抢先了 */
    if (!seatSell(sIdx, r, c)) {
        printf("\n  这个座位刚刚被别人买走了，请重新选。\n");
        pauseScreen();
        return 0;
    }

    /* ⑦ 取片名做快照，管理员以后改片名调价格都不影响这张老票 */
    for (i = 0; i < g_movieCnt; i++) {
        if (g_movies[i].id == g_scheds[sIdx].movieId) {
            strcpy(mname, g_movies[i].name);
            break;
        }
    }

    /* ⑧ 写订单 */
    t = &g_tickets[g_ticketCnt];
    t->id = newTicketId();
    t->userId = uid;
    t->schedId = g_scheds[sIdx].id;
    strcpy(t->movieName, mname);
    strcpy(t->date, g_scheds[sIdx].date);
    t->slot = g_scheds[sIdx].slot;
    t->row = r;
    t->col = c;
    t->price = g_scheds[sIdx].price;
    t->state = 1;
    t->hallId = g_scheds[sIdx].hallId;
    getNow(t->purchase_time);
    t->refund_time[0] = '\0';
    t->refund_admin_id = 0;
    g_ticketCnt++;

    /* ⑨ 票房在这里加，seatSell 里不管钱 */
    g_scheds[sIdx].income += g_scheds[sIdx].price;

    saveCinema();
    saveTickets();

    /* ⑩ 小票 */
    printf("\n  ---------------- 购 票 成 功 ----------------\n");
    printf("   票号：%d\n", t->id);
    printf("   影片：《%s》\n", t->movieName);
    printf("   场次：%s 第 %d 场\n", t->date, t->slot);
    printf("   影厅：%d 号厅\n", g_scheds[sIdx].hallId);
    printf("   座位：%d 排 %d 座\n", r + 1, c + 1);
    printf("   金额：%.2f 元\n", t->price);
    printf("  --------------------------------------------\n");
    pauseScreen();
    return 1;
}

/*------------------------------------------------------------------
  2. 退票
  成功返回 1，取消或失败返回 0
------------------------------------------------------------------*/
int refundTicket(int uid)
{
    int idx[MAX_TICKETS];       /* 存真正下标，因为票数组有空洞 */
    int n = 0, i, choice, real, sIdx;
    Ticket* t;

    /* ① 把这个人的有效票挑出来，按 1、2、3 编号给他看 */
    printf("\n============== 可退票的订单 ==============\n");
    for (i = 0; i < g_ticketCnt; i++) {
        if (g_tickets[i].userId != uid || g_tickets[i].state != 1) continue;
        idx[n] = i;
        n++;
        printf("  [%d] 《%s》  %s  第%d场  %d排%d座  %.2f 元\n",
            n, g_tickets[i].movieName, g_tickets[i].date,
            g_tickets[i].slot, g_tickets[i].row + 1,
            g_tickets[i].col + 1, g_tickets[i].price);
    }

    if (n == 0) {
        printf("  你还没有可退的票。\n");
        pauseScreen();
        return 0;
    }

    /* ② 让用户选第几张（不是票号，是上面那个编号） */
    printf("\n  请输入要退的编号（1 ~ %d，0 放弃）：", n);
    choice = inputInt(0, n);
    if (choice == 0) {
        printf("\n  已取消退票。\n");
        pauseScreen();
        return 0;
    }

    real = idx[choice - 1];     /* 重数一遍拿到真实下标 */
    t = &g_tickets[real];

    /* ③ 反查场次，把座位放回去，票房减掉 */
    sIdx = findSchedule(t->schedId);
    if (sIdx >= 0) {
        seatFree(sIdx, t->row, t->col);
        if (g_scheds[sIdx].income >= t->price)
            g_scheds[sIdx].income -= t->price;
        else
            g_scheds[sIdx].income = 0.0;     /* 兜底，不许出现负票房 */
    }

    /* ④ 票不删，只改状态，留痕 */
    t->state = 2;
    getNow(t->refund_time);
    t->refund_admin_id = 0;      /* 用户自己退票 */

    saveCinema();
    saveTickets();

    printf("\n  退票成功，票号 %d 已作废，退款 %.2f 元。\n", t->id, t->price);
    pauseScreen();
    return 1;
}

/*------------------------------------------------------------------
  3. 我的订单（含已退的，方便对账）
------------------------------------------------------------------*/
void showMyTickets(int uid)
{
    int i, n = 0, valid = 0;
    double paid = 0.0;

    printf("\n================ 我的订单 ================\n");
    for (i = 0; i < g_ticketCnt; i++) {
        if (g_tickets[i].userId != uid) continue;
        n++;
        printf("  [%d] 《%s》  %s  第%d场  %d排%d座  %.2f 元  %s\n",
            n, g_tickets[i].movieName, g_tickets[i].date,
            g_tickets[i].slot, g_tickets[i].row + 1,
            g_tickets[i].col + 1, g_tickets[i].price,
            g_tickets[i].state == 1 ? "有效" : "已退");
        if (g_tickets[i].state == 1) {
            valid++;
            paid += g_tickets[i].price;
        }
    }

    if (n == 0) {
        printf("  你还没有任何订单。\n");
    }
    else {
        printf("  ----------------------------------------\n");
        printf("  共 %d 条订单，其中有效 %d 张，合计 %.2f 元。\n", n, valid, paid);
    }
    pauseScreen();
}



/* ==================== 批量退票 / 退单辅助 ==================== */

/* 退一张有效票（不保存文件）；adminId=0 表示用户自己退 */
int ticketRefundOne(int tIdx, int adminId)
{
    Ticket* t;
    int sIdx;

    if (tIdx < 0 || tIdx >= g_ticketCnt) return 0;

    t = &g_tickets[tIdx];
    if (t->state != 1) return 0;

    t->state = 2;
    getNow(t->refund_time);
    t->refund_admin_id = adminId;

    sIdx = findSchedule(t->schedId);
    if (sIdx >= 0)
    {
        seatFree(sIdx, t->row, t->col);
        if (g_scheds[sIdx].income >= t->price)
            g_scheds[sIdx].income -= t->price;
        else
            g_scheds[sIdx].income = 0.0f;
    }

    return 1;
}

/* 按票号查找，返回数组下标，找不到 -1 */
static int findTicketById(int id)
{
    int i;

    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].id == id) return i;

    return -1;
}

/* 按场次批量退票 */
static int refundBySchedule(void)
{
    int id, sIdx, i, n = 0, confirm;
    double total = 0.0;

    printf("请输入场次编号：");
    id = inputInt(1, 1000000);
    sIdx = findSchedule(id);
    if (sIdx == -1) { printf("场次不存在。\n"); return 0; }

    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].schedId == id && g_tickets[i].state == 1)
        { n++; total += g_tickets[i].price; }

    if (n == 0) { printf("该场次没有有效票。\n"); return 0; }

    printf("该场次有 %d 张有效票，退款 %.2f 元。确认退票？1=是，0=否：", n, total);
    confirm = inputInt(0, 1);
    if (confirm == 0) { printf("已取消。\n"); return 0; }

    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].schedId == id && g_tickets[i].state == 1)
            ticketRefundOne(i, g_currentAdminId);

    saveCinema();
    saveTickets();
    printf("已退 %d 张票。\n", n);
    return n;
}

/* 按用户批量退票 */
static int refundByUser(void)
{
    int uid, i, n = 0, confirm;
    double total = 0.0;

    printf("请输入用户编号：");
    uid = inputInt(1, 1000000);

    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].userId == uid && g_tickets[i].state == 1)
        { n++; total += g_tickets[i].price; }

    if (n == 0) { printf("该用户没有有效票。\n"); return 0; }

    printf("该用户有 %d 张有效票，退款 %.2f 元。确认退票？1=是，0=否：", n, total);
    confirm = inputInt(0, 1);
    if (confirm == 0) { printf("已取消。\n"); return 0; }

    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].userId == uid && g_tickets[i].state == 1)
            ticketRefundOne(i, g_currentAdminId);

    saveCinema();
    saveTickets();
    printf("已退 %d 张票。\n", n);
    return n;
}

/* 按票号批量退票 */
static int refundByTicketIds(void)
{
    int ids[MAX_TICKETS];
    int n = 0, id, i, idx, done = 0, confirm;
    double total = 0.0;

    printf("输入票号，输入 0 结束：\n");
    while (n < MAX_TICKETS)
    {
        printf("票号：");
        id = inputInt(0, 1000000);
        if (id == 0) break;

        idx = findTicketById(id);
        if (idx == -1) { printf("票不存在。\n"); continue; }
        if (g_tickets[idx].state != 1) { printf("该票已退。\n"); continue; }

        ids[n++] = idx;
        total += g_tickets[idx].price;
    }

    if (n == 0) { printf("未选择任何票。\n"); return 0; }

    printf("共 %d 张票，退款 %.2f 元。确认退票？1=是，0=否：", n, total);
    confirm = inputInt(0, 1);
    if (confirm == 0) { printf("已取消。\n"); return 0; }

    for (i = 0; i < n; i++)
    {
        if (ticketRefundOne(ids[i], g_currentAdminId)) done++;
    }

    saveCinema();
    saveTickets();
    printf("已退 %d 张票。\n", done);
    return done;
}

/* 批量退票入口 */
int batchRefundMenu(void)
{
    int op;
    int r = 0;

    printf("\n1. 按场次退票\n");
    printf("2. 按用户退票\n");
    printf("3. 按票号退票\n");
    printf("0. 返回\n");
    printf("请选择 [0-3]：");
    op = inputInt(0, 3);

    if (op == 1) r = refundBySchedule();
    else if (op == 2) r = refundByUser();
    else if (op == 3) r = refundByTicketIds();
    else return 0;

    if (r > 0) writeAdminLog(g_currentAdminAccount, "批量退票");
    return r;
}



/* ==================== 订单查询 ==================== */

/* 根据用户编号取用户名 */
static void userNameOf(int uid, char* out)
{
    int i;

    for (i = 0; i < g_userCnt; i++)
        if (g_users[i].id == uid) { strcpy(out, g_users[i].name); return; }

    sprintf(out, "用户#%d", uid);
}

/* 根据管理员编号取账号（退票操作员） */
static void adminAccountOf(int aid, char* out)
{
    int i;

    if (aid == 0) { strcpy(out, "用户"); return; }

    for (i = 0; i < g_adminCnt; i++)
        if (g_admins[i].id == aid) { strcpy(out, g_admins[i].account); return; }

    sprintf(out, "管理员#%d", aid);
}

/* 打印一张票 */
static void printTicket(int ti, int no)
{
    Ticket* t = &g_tickets[ti];
    char uname[20];
    char op[20];

    userNameOf(t->userId, uname);
    adminAccountOf(t->refund_admin_id, op);

    printf("[%d] 票号%d 用户%s(%d)\n", no, t->id, uname, t->userId);
    printf("    《%s》 %s 第%d场 %d号厅 %d排%d座 %.2f元 %s\n",
        t->movieName, t->date, t->slot, t->hallId,
        t->row + 1, t->col + 1, t->price,
        t->state == 1 ? "有效" : "已退");
    printf("    购票时间：%s\n", t->purchase_time);
    if (t->state == 2)
        printf("    退票时间：%s  操作员：%s\n", t->refund_time, op);
}

/* 分页显示订单；idx 为 NULL 时按 0..cnt-1 顺序 */
static void showOrdersPaged(const int* idx, int cnt)
{
    int page = 0;
    int perPage = 20;
    int totalPages = (cnt + perPage - 1) / perPage;
    int i, start, end, choice;

    if (cnt == 0) { printf("没有订单。\n"); return; }

    while (1)
    {
        system("cls");
        printf("==== 订单列表（共 %d 条） 第 %d/%d 页 ====\n", cnt, page + 1, totalPages);

        start = page * perPage;
        end = start + perPage;
        if (end > cnt) end = cnt;

        for (i = start; i < end; i++)
        {
            int ti = (idx == NULL) ? i : idx[i];
            printTicket(ti, i + 1);
        }

        printf("\n1=上一页 2=下一页 0=返回：");
        choice = inputInt(0, 2);
        if (choice == 1 && page > 0) page--;
        else if (choice == 2 && page < totalPages - 1) page++;
        else if (choice == 0) return;
    }
}

void showAllOrders(void)
{
    if (g_ticketCnt == 0) { printf("还没有任何订单。\n"); return; }
    showOrdersPaged(NULL, g_ticketCnt);
}

void showOrdersByUser(void)
{
    int i, j, uid;
    int total, valid, refunded;
    double spent;
    char lastDate[11];
    int found = 0;
    int idx[MAX_TICKETS];
    int cnt = 0;

    printf("==== 已购票用户汇总 ====\n");
    printf("用户编号\t用户名\t总订单\t有效\t已退\t总消费\t最近观影\n");

    for (i = 0; i < g_userCnt; i++)
    {
        total = 0; valid = 0; refunded = 0;
        spent = 0.0;
        lastDate[0] = '\0';

        for (j = 0; j < g_ticketCnt; j++)
        {
            if (g_tickets[j].userId != g_users[i].id) continue;
            total++;
            if (g_tickets[j].state == 1) { valid++; spent += g_tickets[j].price; }
            else refunded++;
            if (lastDate[0] == '\0' || strcmp(g_tickets[j].date, lastDate) > 0)
                strcpy(lastDate, g_tickets[j].date);
        }

        if (total == 0) continue;

        printf("%d\t%s\t%d\t%d\t%d\t%.2f\t%s\n",
            g_users[i].id, g_users[i].name, total, valid, refunded, spent, lastDate);
        found = 1;
    }

    if (!found) { printf("还没有用户购票。\n"); return; }

    printf("\n请输入用户编号查看明细（0 返回）：");
    uid = inputInt(0, 1000000);
    if (uid == 0) return;

    for (j = 0; j < g_ticketCnt; j++)
        if (g_tickets[j].userId == uid) idx[cnt++] = j;

    if (cnt == 0) { printf("该用户没有订单。\n"); return; }
    showOrdersPaged(idx, cnt);
}

