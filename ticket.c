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
    showSeats(&g_scheds[sIdx]);
    left = countLeft(&g_scheds[sIdx]);
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
    if (!seatSell(&g_scheds[sIdx], r, c)) {
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
        seatFree(&g_scheds[sIdx], t->row, t->col);
        if (g_scheds[sIdx].income >= t->price)
            g_scheds[sIdx].income -= t->price;
        else
            g_scheds[sIdx].income = 0.0;     /* 兜底，不许出现负票房 */
    }

    /* ④ 票不删，只改状态，留痕 */
    t->state = 2;

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
