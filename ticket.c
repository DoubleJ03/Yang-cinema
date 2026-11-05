/*==================================================================
  ticket.c —— 购票 / 退票 / 我的订单
  购票分成两层：ticketBuyCore() 只算账，buyTicket() 只做界面。
  其余对外函数就是退票、改签、订单查询这一组，别的一概不写
==================================================================*/
#include "cinema.h"
#include <string.h>

/*------------------------------------------------------------------
  1. 购票

  分两层：
    ticketBuyCore()  只算账：判断规则 + 写内存，不打印、不读键盘、不存盘
    buyTicket()      只做界面：问观众、调内核、把结果说给人听

  这样网页购票、自动抢票这些新入口可以直接调内核，
  不用把"打印座位图、等输入"那一套再抄一遍。
------------------------------------------------------------------*/

/* 数一个人在某场已经占了几张有效票（改签票也占名额，不然能靠改签超买） */
static int countValidByUser(int uid, int schedId)
{
    int i, n = 0;

    for (i = 0; i < g_ticketCnt; i++)
    {
        if (g_tickets[i].userId == uid &&
            g_tickets[i].schedId == schedId &&
            TICKET_VALID(g_tickets[i].state))
            n++;
    }
    return n;
}

/* 取片名做快照：管理员以后改片名、调价格，都不影响这张老票 */
static void movieNameOfSched(int sIdx, char* dst, int size)
{
    int i;

    dst[0] = '\0';
    for (i = 0; i < g_movieCnt; i++)
    {
        if (g_movies[i].id == g_scheds[sIdx].movieId)
        {
            strncpy(dst, g_movies[i].name, size - 1);
            dst[size - 1] = '\0';
            return;
        }
    }
}

/*------------------------------------------------------------------
  购票内核：纯业务，不碰屏幕和键盘，也不存盘。
  成功时把票写进 out、落进订单表并加票房；失败只返回原因码。
  调用方（控制台壳 / 网页 / 抢票脚本）自己决定怎么提示、何时保存。
------------------------------------------------------------------*/
BuyResult ticketBuyCore(int uid, int schedId, int row, int col, Ticket* out)
{
    int sIdx;
    Ticket* t;

    if (g_ticketCnt >= MAX_TICKETS) return BUY_TABLE_FULL;

    sIdx = findSchedule(schedId);
    if (sIdx < 0) return BUY_NO_SCHED;

    /* 网页入口不经过 pickSchedule，这几条必须在这里再判一遍：
       已开场不卖、座位号越界不卖、满座不卖 */
    if (isStarted(sIdx)) return BUY_STARTED;
    if (row < 0 || row >= ROW_COUNT || col < 0 || col >= COL_COUNT)
        return BUY_BAD_SEAT;
    if (countLeft(sIdx) <= 0) return BUY_SOLD_OUT;
    if (countValidByUser(uid, schedId) >= MAX_PER_USER) return BUY_LIMIT;

    /* 最后裁决：座位到底还在不在。座位图可能是几秒前刷出来的 */
    if (!seatSell(sIdx, row, col)) return BUY_SEAT_TAKEN;

    t = &g_tickets[g_ticketCnt];
    memset(t, 0, sizeof(Ticket));
    t->id      = newTicketId();
    t->userId  = uid;
    t->schedId = schedId;
    movieNameOfSched(sIdx, t->movieName, sizeof(t->movieName));
    strcpy(t->date, g_scheds[sIdx].date);
    t->slot    = g_scheds[sIdx].slot;
    t->hallId  = g_scheds[sIdx].hallId;
    t->row     = row;
    t->col     = col;
    t->price   = g_scheds[sIdx].price;
    t->state   = TICKET_OK;
    getNow(t->purchase_time);
    strncpy(t->buyerName, userNameById(uid), sizeof(t->buyerName) - 1);
    g_ticketCnt++;

    /* 票房在核心里加，seatSell 只负责座位 */
    addIncome(sIdx, t->price);

    *out = *t;
    return BUY_OK;
}

/* 结果码翻译成人话，界面层专用 */
static void printBuyResult(BuyResult res, const Ticket* t, int used)
{
    switch (res)
    {
    case BUY_OK:
        printf("\n  ---------------- 购 票 成 功 ----------------\n");
        printf("   票号：%d\n", t->id);
        printf("   影片：《%s》\n", t->movieName);
        printf("   场次：%s 第 %d 场\n", t->date, t->slot);
        printf("   影厅：%d 号厅\n", t->hallId);
        printf("   座位：%d 排 %d 座\n", t->row + 1, t->col + 1);
        printf("   购票人：%s\n", t->buyerName);
        printf("   金额：%.2f 元\n", t->price);
        printf("  --------------------------------------------\n");
        break;
    case BUY_CANCELLED:
        printf("\n  已取消购票。\n");
        break;
    case BUY_TABLE_FULL:
        printf("\n  订单表已满（上限 %d 条），暂时无法出票。\n", MAX_TICKETS);
        break;
    case BUY_NO_SCHED:
        printf("\n  场次不存在。\n");
        break;
    case BUY_STARTED:
        printf("\n  该场次已经开场，不能再买票。\n");
        break;
    case BUY_BAD_SEAT:
        printf("\n  排号或座号超出范围。\n");
        break;
    case BUY_SOLD_OUT:
        printf("\n  这一场已经满座了。\n");
        break;
    case BUY_LIMIT:
        printf("\n  同一场次每人最多买 %d 张，你已经买了 %d 张。\n",
            MAX_PER_USER, used);
        break;
    case BUY_SEAT_TAKEN:
        printf("\n  这个座位刚刚被别人买走了，请重新选。\n");
        break;
    default:
        printf("\n  购票失败。\n");
        break;
    }
}

/*------------------------------------------------------------------
  控制台壳：只做"问 -> 调内核 -> 说"
  成功返回 1，取消或失败返回 0
------------------------------------------------------------------*/
int buyTicket(int uid)
{
    int sIdx, r, c, left, used;
    Ticket t;
    BuyResult res;

    if (g_ticketCnt >= MAX_TICKETS)
    {
        printBuyResult(BUY_TABLE_FULL, NULL, 0);
        pauseScreen();
        return 0;
    }

    sIdx = pickSchedule();          /* 场次不存在/已开场/售完，pickSchedule 已经拦过 */
    if (sIdx < 0)
    {
        printBuyResult(BUY_CANCELLED, NULL, 0);
        pauseScreen();
        return 0;
    }

    /* 先看名额再挑座，免得挑完才说买不了 */
    used = countValidByUser(uid, g_scheds[sIdx].id);
    if (used >= MAX_PER_USER)
    {
        printBuyResult(BUY_LIMIT, NULL, used);
        pauseScreen();
        return 0;
    }

    showSeats(sIdx);
    left = countLeft(sIdx);
    if (left <= 0)
    {
        printBuyResult(BUY_SOLD_OUT, NULL, 0);
        pauseScreen();
        return 0;
    }
    printf("\n  剩余空座：%d 个，请挑一个。\n", left);

    printf("  排号 1 ~ %d（输入 -1 放弃购票）：", ROW_COUNT);
    r = inputInt(-1, ROW_COUNT);
    if (r < 0)
    {
        printBuyResult(BUY_CANCELLED, NULL, 0);
        pauseScreen();
        return 0;
    }

    printf("  座号 1 ~ %d（输入 -1 放弃购票）：", COL_COUNT);
    c = inputInt(-1, COL_COUNT);
    if (c < 0)
    {
        printBuyResult(BUY_CANCELLED, NULL, 0);
        pauseScreen();
        return 0;
    }

    res = ticketBuyCore(uid, g_scheds[sIdx].id, r - 1, c - 1, &t);

    if (res == BUY_OK)              /* 落盘由壳决定，内核不碰文件 */
    {
        saveCinema();
        saveTickets();
    }
    printBuyResult(res, &t, 0);
    pauseScreen();
    return res == BUY_OK;
}

/*------------------------------------------------------------------
  2. 退票
  成功返回 1，取消或失败返回 0
------------------------------------------------------------------*/
/*------------------------------------------------------------------
  2. 退票（阶梯手续费）
  成功返回 1，取消或失败返回 0
  ─────────────────────────────────────────────────────────────
  手续费按"现在距开演还有多久"分四档：免费 / 2 / 5 / 10 元，
  改签是半价。数字不给用户拍脑袋，直接列在表里，
  用户不用先退一次才知道要扣多少。
------------------------------------------------------------------*/

/* 手续费档位的文字说明。
   边界必须和 calcFee() 一模一样，改档位要两处一起改 */
static const char* feeTag(long left)
{
    if (left >= 24L * 60) return "免费";
    if (left >= 6L * 60)  return "扣2元";
    if (left >= 2L * 60)  return "扣5元";
    return "扣10元";
}

/*------------------------------------------------------------------
  统一退票费率。按“谁的责任”分两类：
    a) 观众自己不想看了：用户自助退票、管理员按用户/按票号代客退票，
       都调本函数，费率只跟“距开场还有多少分钟”有关；
    b) 影院方原因：按场次退票（取消整场）、撤场删场次，
       原价全额退，不调本函数、不收手续费。
  改收费规则只改 calcFee()，a 类入口自动跟着变。
------------------------------------------------------------------*/
float refundFeeBySched(int sIdx)
{
    if (sIdx < 0) return 0.0f;
    return calcFee((long)minutesToStart(sIdx), 0);
}

/* 票状态文字。订单列表、小票、CSV 导出都调它，
   避免“改签票”在某个界面被显示成“已退” */
const char* ticketStateText(int state)
{
    switch (state)
    {
    case TICKET_OK:          return "有效";
    case TICKET_REFUNDED:    return "已退";
    case TICKET_CHANGED_OLD: return "已改签";
    case TICKET_CHANGED_NEW: return "有效(改签票)";
    default: return "未知";
    }
}

int refundTicket(int uid)
{
    int    idx[MAX_TICKETS];            /* 能退的票，存真实下标 */
    static float feeShown[MAX_TICKETS]; /* 列表这一刻的手续费，确认时用来比 */
    int    n = 0, i, pass, choice, real, sIdx;
    long   left;
    float  fee, back, nowFee;
    Ticket* t;
    char   name[64], when[24], seat[24], seat2[24];

    system("cls");
    printf("=============== 可退票的订单 ===============\n");
    printf("  %-4s  %-16s  %-11s  %-8s  %6s  %6s  %7s  %s\n",
        "票号", "影片", "场次", "座位", "票价", "手续费", "实退", "说明");

    /* 第一遍：现在真能退的（state=1、没开场、场次还在），编号 1、2、3… */
    for (i = 0; i < g_ticketCnt; i++)
    {
        if (g_tickets[i].userId != uid) continue;
        if (g_tickets[i].state != TICKET_OK) continue;   /* 只有原票能退，改签票不能 */

        sIdx = findSchedule(g_tickets[i].schedId);
        if (sIdx < 0) continue;
        if (isStarted(sIdx)) continue;

        left = (long)minutesToStart(sIdx);
        fee  = calcFee(left, 0);
        back = g_tickets[i].price - fee;
        if (back < 0.0f) back = 0.0f;

        n++;
        idx[n - 1]    = i;
        feeShown[n - 1] = fee;

        fitText(name, g_tickets[i].movieName, 16);
        sprintf(when, "%s %s", g_tickets[i].date + 5,
                slotTime(g_tickets[i].slot));
        sprintf(seat, "%d排%d座", g_tickets[i].row + 1, g_tickets[i].col + 1);

        fitText(seat2, seat, 8);         /* 座位补到 8 列 */
        printf("  %4d  %s  %s  %s  %6.2f  %6.2f  %7.2f  %s%s%s\n",
            n, name, when, seat2, g_tickets[i].price, fee, back,
            g_colorOn ? "\x1b[1;32m" : "", feeTag(left),
            g_colorOn ? "\x1b[0m" : "");
    }

    /* 第二遍：不可退的沉到底部，不给编号，标红说明原因。
       放在后面是怕用户第一眼看到"已开场"还以为程序坏了 */
    for (pass = 0; pass < 2; pass++)
    {
        for (i = 0; i < g_ticketCnt; i++)
        {
            const char* why = NULL;

            if (g_tickets[i].userId != uid) continue;

            if (g_tickets[i].state == TICKET_REFUNDED) continue;      /* 已退的不显示 */
            if (g_tickets[i].state == TICKET_CHANGED_NEW) why = "改签票不可退";
            else if (g_tickets[i].state == TICKET_CHANGED_OLD) why = "已改签";
            else
            {
                sIdx = findSchedule(g_tickets[i].schedId);
                if (sIdx < 0)            why = "场次已删除";
                else if (isStarted(sIdx)) why = "已开场";
            }
            if (why == NULL) continue;
            if (pass == 0 && g_tickets[i].state != TICKET_CHANGED_NEW) continue;  /* 先列改签票 */
            if (pass == 1 && g_tickets[i].state == TICKET_CHANGED_NEW) continue;

            fitText(name, g_tickets[i].movieName, 16);
            sprintf(when, "%s %s", g_tickets[i].date + 5,
                    slotTime(g_tickets[i].slot));
            sprintf(seat, "%d排%d座", g_tickets[i].row + 1, g_tickets[i].col + 1);

            fitText(seat2, seat, 8);
            printf("  %4s  %s  %s  %s  %6.2f  %6s  %7s  %s%s%s\n",
                "-", name, when, seat2, g_tickets[i].price, "--", "--",
                g_colorOn ? "\x1b[1;31m" : "", why,
                g_colorOn ? "\x1b[0m" : "");
        }
    }

    if (n == 0)
    {
        printf("\n  你现在没有可退的票。\n");
        pauseScreen();
        return 0;
    }

    printf("\n  请输入要退的编号（1 ~ %d，0 放弃）：", n);
    choice = inputInt(0, n);
    if (choice == 0)
    {
        printf("\n  已取消退票。\n");
        pauseScreen();
        return 0;
    }

    real = idx[choice - 1];
    t    = &g_tickets[real];

    /* 反查场次：场次被管理员删了就不给退，避免退了座位没处还 */
    sIdx = findSchedule(t->schedId);
    if (sIdx < 0)
    {
        printf("\n  这场已经被撤了，无法退票，请联系工作人员。\n");
        pauseScreen();
        return 0;
    }

    left   = (long)minutesToStart(sIdx);
    nowFee = calcFee(left, 0);
    back   = t->price - nowFee;
    if (back < 0.0f) back = 0.0f;

    /* 用户在列表上停留时可能刚好跨过档位（比如从"免费"滑进"扣2元"）。
       数字变了就说清楚，别让用户觉得前后不一致 */
    if (nowFee != feeShown[choice - 1])
    {
        printf("\n  提示：已跨过档位，手续费由 %.2f 元变为 %.2f 元。\n",
            feeShown[choice - 1], nowFee);
    }

    printf("\n  ================ 退票确认 ================\n");
    printf("  《%s》  %s %s  %d排%d座\n",
        t->movieName, t->date, slotTime(t->slot), t->row + 1, t->col + 1);
    printf("  票面 %.2f 元，手续费 %.2f 元（%s），实退 %.2f 元\n",
        t->price, nowFee, feeTag(left), back);
    printf("  确认退票？1=确认 0=放弃：");
    if (inputInt(0, 1) == 0)
    {
        printf("\n  已取消退票。\n");
        pauseScreen();
        return 0;
    }

    /* 座位放回池子。票房按“净票房”口径整张冲减：
       income 只记仍然有效的票面合计，手续费属于服务性收入，不进票房。
       口径统一之后，管理员批量退票走的是同一套算法 */
    seatFree(sIdx, t->row, t->col);
    addIncome(sIdx, -t->price);

    t->state = 2;
    getNow(t->refund_time);
    t->refund_admin_id = 0;                 /* 用户自己退票 */

    saveCinema();
    saveTickets();

    printf("\n  退票成功，票号 %d 已作废，手续费 %.2f 元，退回 %.2f 元。\n",
        t->id, nowFee, back);
    pauseScreen();
    return 1;
}

/*------------------------------------------------------------------
  2b. 改签
  把一张有效票换到另一个场次，座位重选。
  ─────────────────────────────────────────────────────────────
  规则（缺一条就会被钻空子）：
    ① 手续费按阶梯半价收：0 / 1 / 3 / 5 元，比退票便宜
    ② 差价另算：新场次贵就补，便宜不退
    ③ 每张票只能改一次 —— 老票标 state=3，不能再退也不能再改
    ④ 改签出来的新票标 state=4，能进场、算票房，但不可退不可改
       没有第 ④ 条，就能"先改签到 3 天后、再按免费档退掉"，
       阶梯收费等于白设
------------------------------------------------------------------*/
int changeTicket(int uid)
{
    int    idx[MAX_TICKETS];
    int    n = 0, i, choice, real, oldIdx, newIdx, r, c;
    long   left;
    float  fee, diff, pay;
    Ticket nt;
    Ticket* t;
    char   name[64], when[24], seat[24], seat2[24];

    /* 改签会往订单表里追加一张新票，容量必须先查。
       购票那边一直有这条，改签原来漏了，满表时会越界写 */
    if (g_ticketCnt >= MAX_TICKETS)
    {
        printf("\n  订单表已满（上限 %d 条），暂时无法改签。\n", MAX_TICKETS);
        pauseScreen();
        return 0;
    }

    system("cls");
    printf("=============== 可改签的订单 ===============\n");
    printf("  %-4s  %-16s  %-11s  %-8s  %6s  %6s\n",
        "票号", "影片", "场次", "座位", "票价", "手续费");

    for (i = 0; i < g_ticketCnt; i++)
    {
        if (g_tickets[i].userId != uid) continue;
        if (g_tickets[i].state != TICKET_OK) continue;   /* 已改签的老票、改签新票都不能再改 */

        oldIdx = findSchedule(g_tickets[i].schedId);
        if (oldIdx < 0) continue;
        if (isStarted(oldIdx)) continue;

        left = (long)minutesToStart(oldIdx);
        fee  = calcFee(left, 1);                    /* 1 = 改签，半价 */

        n++;
        idx[n - 1] = i;

        fitText(name, g_tickets[i].movieName, 16);
        sprintf(when, "%s %s", g_tickets[i].date + 5,
                slotTime(g_tickets[i].slot));
        sprintf(seat, "%d排%d座", g_tickets[i].row + 1, g_tickets[i].col + 1);
        fitText(seat2, seat, 8);

        printf("  %4d  %s  %s  %s  %6.2f  %6.2f\n",
            n, name, when, seat2, g_tickets[i].price, fee);
    }

    if (n == 0)
    {
        printf("\n  你现在没有可改签的票。\n");
        printf("  说明：每张票只能改签一次，改签过的票不能再退也不能再改。\n");
        pauseScreen();
        return 0;
    }

    printf("\n  请输入要改签的编号（1 ~ %d，0 放弃）：", n);
    choice = inputInt(0, n);
    if (choice == 0)
    {
        printf("\n  已取消改签。\n");
        pauseScreen();
        return 0;
    }

    real   = idx[choice - 1];
    t      = &g_tickets[real];
    oldIdx = findSchedule(t->schedId);
    left   = (long)minutesToStart(oldIdx);
    fee    = calcFee(left, 1);

    /* 选新场次：pickSchedule 里已经挡住了过期场次和满座场次 */
    printf("\n  请选择要改到的场次：\n");
    newIdx = pickSchedule();
    if (newIdx < 0)
    {
        printf("\n  已取消改签。\n");
        pauseScreen();
        return 0;
    }
    if (newIdx == oldIdx)
    {
        printf("\n  新场次和原来一样，等于没改。\n");
        pauseScreen();
        return 0;
    }

    /* 挑座位：默认还坐原来那个排号座号，被占了就自己重选 */
    showSeats(newIdx);
    printf("\n  剩余空座：%d 个。\n", countLeft(newIdx));
    printf("  排号 1 ~ %d（输入 -1 放弃）：", ROW_COUNT);
    r = inputInt(-1, ROW_COUNT);
    if (r < 0) { printf("\n  已取消改签。\n"); pauseScreen(); return 0; }

    printf("  座号 1 ~ %d（输入 -1 放弃）：", COL_COUNT);
    c = inputInt(-1, COL_COUNT);
    if (c < 0) { printf("\n  已取消改签。\n"); pauseScreen(); return 0; }

    r--; c--;

    if (g_scheds[newIdx].seat[r][c] != 0)
    {
        printf("\n  这个座位已经卖出去了，请重新操作。\n");
        pauseScreen();
        return 0;
    }

    /* 差价：新场次贵就补，便宜不退（票价是影院定的，不因改签退钱） */
    diff = g_scheds[newIdx].price - t->price;
    pay  = fee + (diff > 0.0f ? diff : 0.0f);

    printf("\n  ================ 改签确认 ================\n");
    printf("  原场次：《%s》 %s %s  %d排%d座  %.2f 元\n",
        t->movieName, t->date, slotTime(t->slot), t->row + 1, t->col + 1, t->price);
    printf("  新场次：%s %s  %d号厅  %d排%d座  %.2f 元\n",
        g_scheds[newIdx].date, slotTime(g_scheds[newIdx].slot),
        g_scheds[newIdx].hallId, r + 1, c + 1, g_scheds[newIdx].price);
    printf("  手续费 %.2f 元（%s，改签半价），差价 %.2f 元，应付 %.2f 元\n",
        fee, feeTag(left), diff > 0.0f ? diff : 0.0f, pay);
    printf("  注意：改签后这张票不能退，也不能再次改签。\n");
    printf("  确认改签？1=确认 0=放弃：");
    if (inputInt(0, 1) == 0)
    {
        printf("\n  已取消改签。\n");
        pauseScreen();
        return 0;
    }

    /* 新票先建好，再动老票，任何一步失败都不会留下半张票 */
    memset(&nt, 0, sizeof(nt));
    nt.id     = newTicketId();
    nt.userId = uid;
    nt.schedId = g_scheds[newIdx].id;
    strcpy(nt.movieName, "");
    for (i = 0; i < g_movieCnt; i++)
        if (g_movies[i].id == g_scheds[newIdx].movieId)
        {
            strcpy(nt.movieName, g_movies[i].name);
            break;
        }
    strcpy(nt.date, g_scheds[newIdx].date);
    nt.slot   = g_scheds[newIdx].slot;
    nt.hallId = g_scheds[newIdx].hallId;
    nt.row    = r;
    nt.col    = c;
    nt.price  = g_scheds[newIdx].price;
    nt.state  = TICKET_CHANGED_NEW;           /* 改签票：不可退不可改 */
    getNow(nt.purchase_time);
    nt.refund_time[0] = '\0';
    nt.refund_admin_id = 0;

    /* 购票人跟着老票走：改签只是换场次，不换人。
       老票要是没记名字的旧数据，才补当前用户名 */
    if (t->buyerName[0] != '\0')
        strncpy(nt.buyerName, t->buyerName, 19);
    else
        strncpy(nt.buyerName, userNameById(uid), 19);
    nt.buyerName[19] = '\0';

    /* 老票作废（state=3 留痕，不能退也不能再改）。
       老场次票房整张冲减，和退票同一算法；手续费不进票房 */
    seatFree(oldIdx, t->row, t->col);
    addIncome(oldIdx, -t->price);
    t->state = 3;
    getNow(t->refund_time);

    /* 新票占座、计票房 */
    seatSell(newIdx, r, c);
    addIncome(newIdx, nt.price);
    g_tickets[g_ticketCnt] = nt;
    g_ticketCnt++;

    saveCinema();
    saveTickets();

    printf("\n  ---------------- 改 签 成 功 ----------------\n");
    printf("   新票号：%d\n", nt.id);
    printf("   影片：《%s》\n", nt.movieName);
    printf("   场次：%s %s\n", nt.date, slotTime(nt.slot));
    printf("   影厅：%d 号厅\n", nt.hallId);
    printf("   座位：%d 排 %d 座\n", r + 1, c + 1);
    printf("   本次支付：%.2f 元\n", pay);
    printf("  --------------------------------------------\n");
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
            ticketStateText(g_tickets[i].state));

        /* 购票人是下单时的快照：用户改过名，这里仍显示当时那个名字 */
        if (g_tickets[i].buyerName[0] != '\0')
            printf("      购票时间：%s    购票人：%s\n",
                g_tickets[i].purchase_time, g_tickets[i].buyerName);
        else
            printf("      购票时间：%s\n", g_tickets[i].purchase_time);

        if (TICKET_VALID(g_tickets[i].state)) {
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

/* 一张票作废的内核（不保存文件）。
   allowChanged=0：普通退票，改签票（state=4）不能退、已改签老票（3）不能退；
   allowChanged=1：影院撤场强制作废，改签票一并作废。
   票房一律按净票房口径整张冲减，和用户自助退票完全一致。 */
static int ticketVoid(int tIdx, int adminId, int allowChanged)
{
    Ticket* t;
    int sIdx;

    if (tIdx < 0 || tIdx >= g_ticketCnt) return 0;

    t = &g_tickets[tIdx];
    if (t->state != 1 && !(allowChanged && t->state == 4)) return 0;

    t->state = 2;
    getNow(t->refund_time);
    t->refund_admin_id = adminId;

    sIdx = findSchedule(t->schedId);
    if (sIdx >= 0)
    {
        seatFree(sIdx, t->row, t->col);
        addIncome(sIdx, -t->price);
    }

    return 1;
}

/* 普通退票：用户自助、管理员批量退票都走这里 */
int ticketRefundOne(int tIdx, int adminId)
{
    return ticketVoid(tIdx, adminId, 0);
}

/* 影院撤场：任何在售票（含改签票）一律作废，全额退 */
int ticketForceVoid(int tIdx, int adminId)
{
    return ticketVoid(tIdx, adminId, 1);
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

    /* 按场次批量退票 = 影院取消这一场，责任在影院，所以：
       1) 一律按票面原价全额退，不走阶梯手续费；
       2) 场次里的改签票（state=4）也一并作废，否则它们会变成孤儿票。
       手续费只对“观众自己不想看了”的退票收，见 refundFeeBySched() */

    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].schedId == id && TICKET_VALID(g_tickets[i].state))
        {
            n++;
            total += g_tickets[i].price;
        }

    if (n == 0) { printf("该场次没有有效票。\n"); return 0; }

    printf("该场次有 %d 张有效票，影院取消场次，全额退款 %.2f 元（不收手续费）。确认退票？1=是，0=否：",
        n, total);
    confirm = inputInt(0, 1);
    if (confirm == 0) { printf("已取消。\n"); return 0; }

    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].schedId == id && TICKET_VALID(g_tickets[i].state))
            ticketForceVoid(i, g_currentAdminId);

    saveCinema();
    saveTickets();
    printf("已退 %d 张票，全额退款 %.2f 元。\n", n, total);
    return n;
}

/* 按用户批量退票 */

static int refundByUser(void)
{
    int uid, i, n = 0, confirm;
    double total = 0.0, feeSum = 0.0;
    float fee, back;

    printf("请输入用户编号：");
    uid = inputInt(1, 1000000);

    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].userId == uid && g_tickets[i].state == TICKET_OK)
        {
            fee  = refundFeeBySched(findSchedule(g_tickets[i].schedId));
            back = g_tickets[i].price - fee;
            if (back < 0.0f) back = 0.0f;
            n++; feeSum += fee; total += back;
        }

    if (n == 0) { printf("该用户没有有效票。\n"); return 0; }

    printf("该用户有 %d 张有效票，手续费共 %.2f 元，实退 %.2f 元。确认退票？1=是，0=否：",
        n, feeSum, total);
    confirm = inputInt(0, 1);
    if (confirm == 0) { printf("已取消。\n"); return 0; }

    for (i = 0; i < g_ticketCnt; i++)
        if (g_tickets[i].userId == uid && g_tickets[i].state == TICKET_OK)
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
    double total = 0.0, feeSum = 0.0;
    float fee, back;

    printf("输入票号，输入 0 结束：\n");
    while (n < MAX_TICKETS)
    {
        printf("票号：");
        id = inputInt(0, 1000000);
        if (id == 0) break;

        idx = findTicketById(id);
        if (idx == -1) { printf("票不存在。\n"); continue; }
        if (g_tickets[idx].state != TICKET_OK)
        {
            /* 把真实状态说清楚，别一律说“已退” */
            printf("该票状态为“%s”，不能退票。\n",
                ticketStateText(g_tickets[idx].state));
            continue;
        }

        fee  = refundFeeBySched(findSchedule(g_tickets[idx].schedId));
        back = g_tickets[idx].price - fee;
        if (back < 0.0f) back = 0.0f;

        ids[n++] = idx;
        feeSum += fee;
        total  += back;
    }

    if (n == 0) { printf("未选择任何票。\n"); return 0; }

    printf("共 %d 张票，手续费共 %.2f 元，实退 %.2f 元。确认退票？1=是，0=否：",
        n, feeSum, total);
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

    printf("\n1. 按场次退票（影院取消整场，原价全额退）\n");
    printf("2. 按用户退票（代客退票，按阶梯收手续费）\n");
    printf("3. 按票号退票（代客退票，按阶梯收手续费）\n");
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
        t->row + 1, t->col + 1, t->price, ticketStateText(t->state));
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
    int total, valid, refunded, changed;
    double spent;
    char lastDate[11];
    int found = 0;
    int idx[MAX_TICKETS];
    int cnt = 0;

    printf("==== 已购票用户汇总 ====\n");
    printf("用户编号\t用户名\t总订单\t有效\t已退\t已改签\t总消费\t最近观影\n");

    for (i = 0; i < g_userCnt; i++)
    {
        total = 0; valid = 0; refunded = 0; changed = 0;
        spent = 0.0;
        lastDate[0] = '\0';

        for (j = 0; j < g_ticketCnt; j++)
        {
            if (g_tickets[j].userId != g_users[i].id) continue;
            total++;

            /* 改签票（4）和正常有效票（1）一样算“有效、计入消费”；
               已改签的老票（3）单列一栏，别混进“已退” */
            if (TICKET_VALID(g_tickets[j].state))
            {
                valid++;
                spent += g_tickets[j].price;
            }
            else if (g_tickets[j].state == TICKET_REFUNDED) refunded++;
            else if (g_tickets[j].state == TICKET_CHANGED_OLD) changed++;

            if (lastDate[0] == '\0' || strcmp(g_tickets[j].date, lastDate) > 0)
                strcpy(lastDate, g_tickets[j].date);
        }

        if (total == 0) continue;

        printf("%d\t%s\t%d\t%d\t%d\t%d\t%.2f\t%s\n",
            g_users[i].id, g_users[i].name, total, valid, refunded, changed,
            spent, lastDate);
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

