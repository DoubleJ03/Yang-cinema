#define _CRT_SECURE_NO_WARNINGS
#include "cinema.h"
#include <stdio.h>
#include <string.h>
#include <time.h>


static float readPriceOrDefault(float defaultPrice)
{
    char line[32];               /* 保存用户输入的一行 */
    float price;                 /* 解析后的票价 */
    char extra;                  /* 用来检查数字后面有没有垃圾字符 */
    int ch;                      /* 清空缓冲区用 */

    printf("默认票价 %.2f 元，直接回车使用默认价，否则输入新票价：",
        defaultPrice);

    /* 读一整行，而不是 scanf("%f") */
    if (fgets(line, sizeof(line), stdin) == NULL) {
        return defaultPrice;
    }

    /* 如果这一行太长，把剩余字符清掉，避免影响下一次输入 */
    if (strchr(line, '\n') == NULL) {
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* 空循环 */
        }
    }

    /* 去掉行尾的 \r 或 \n */
    line[strcspn(line, "\r\n")] = '\0';

    /* 空行：用户直接按了回车，使用默认价 */
    if (line[0] == '\0') {
        return defaultPrice;
    }

    /* 必须是一个合法数字，且不能有 "12abc" 这种尾巴 */
    if (sscanf(line, "%f %c", &price, &extra) != 1 || price <= 0.0f) {
        return -1.0f;
    }

    return price;
} 

int addSchedule(void)
{
    Schedule s;                  /* 临时保存新场次 */
    int movieId;                 /* 电影编号 */
    int movieIndex;              /* 电影数组下标 */
    int hallId;                  /* 影厅号 1~5 */
    int slot;                    /* 时间段 1~3 */
    int dayOffset;               /* 0~6 */
    int i;                       /* 循环变量 */
    char date[11];               /* 日期字符串 */
    float customPrice;           /* 解析后的票价 */

    /* 1. 容量检查 */
    if (g_schedCnt >= MAX_SCHEDULES) {
        printf("场次已满，最多 %d 个。\n", MAX_SCHEDULES);
        return 0;
    }

    /* 2. 没有电影不能排片 */
    if (g_movieCnt == 0) {
        printf("还没有电影，请先添加电影。\n");
        return 0;
    }

    /* 3. 显示电影列表 */
    showMovies();

    printf("请输入电影编号：");
    movieId = inputInt(1, 1000000);

    movieIndex = findMovie(movieId);
    if (movieIndex == -1) {
        printf("电影编号 %d 不存在。\n", movieId);
        return 0;
    }

    /* 4. 选择影厅 */
    printf("请输入影厅号（1~%d）：", HALL_COUNT);
    hallId = inputInt(1, HALL_COUNT);

    /* 5. 选择日期：0=今天，1=明天，依此类推（天数较多，不逐行打印） */
    printf("\n请输入放映日期偏移（0=今天，1=明天，最大 %d）：", DAY_NUM - 1);
    dayOffset = inputInt(0, DAY_NUM - 1);
    addDays(date, dayOffset);
    printf("已选择日期：%s\n", date);

    /* 6. 选择时间段 */
    printf("请选择时间段（1=10:00，2=14:30，3=19:00）：");
    slot = inputInt(1, 3);

    /* 7. 冲突检测：同厅、同日、同时间段只能有一场 */
    for (i = 0; i < g_schedCnt; i++) {
        if (g_scheds[i].hallId == hallId &&
            strcmp(g_scheds[i].date, date) == 0 &&
            g_scheds[i].slot == slot) {
            printf("排片冲突：%d 号厅 %s %s 已有场次（场次号 %d）。\n",
                hallId,
                date,
                slotTime(slot),
                g_scheds[i].id);
            return 0;
        }
    }

    /* 8. 清零结构体，座位全部变成待售 */
    memset(&s, 0, sizeof(s));

    s.id = newSchedId();
    s.movieId = movieId;
    s.hallId = hallId;
    strcpy(s.date, date);
    s.slot = slot;
    s.price = g_movies[movieIndex].price;   /* 默认电影票价 */
    s.soldCnt = 0;
    s.income = 0.0f;

    /* 9. 可选：修改场次票价 */
    customPrice = readPriceOrDefault(s.price);

    if (customPrice < 0.0f) {
        printf("票价输入非法。\n");
        return 0;
    }

    s.price = customPrice;

    /* 10. 放入数组并保存 */
    g_scheds[g_schedCnt] = s;
    g_schedCnt++;

    saveCinema();

    printf("排片成功，场次编号：%d\n", s.id);
    writeAdminLog(g_currentAdminAccount, "添加场次");
    return 1;
}


/* 场次状态：先看余票，再看时间。
   判断全部复用 isStarted()/minutesToStart()，不另写一套时间逻辑 */
static const char* schedState(int i, char* buf)
{
    int left = SEAT_TOTAL - g_scheds[i].soldCnt;

    if (left <= 0)                 strcpy(buf, "售罄");
    else if (isStarted(i))         strcpy(buf, "已开演");
    else if (!isStarted(i) && minutesToStart(i) <= 120) strcpy(buf, "即将开演");
    else                           strcpy(buf, "待映");

    return buf;
}

void showSchedules(void)
{
    int  i, movieIndex, left;
    char name[64], tag[16];
    const char* color;

    system("cls");
    if (g_schedCnt == 0) {
        printf("暂无场次，请先排片。\n");
        pauseScreen();
        return;
    }

    printf("=================== 全部场次 ===================\n");
    printf("  %-6s  %-16s  %-4s  %-11s  %-5s  %6s  %5s  %s\n",
        "场次号", "电影", "影厅", "日期", "时间", "票价", "余票", "状态");

    for (i = 0; i < g_schedCnt; i++) {
        movieIndex = findMovie(g_scheds[i].movieId);
        left = SEAT_TOTAL - g_scheds[i].soldCnt;
        if (left < 0) left = 0;

        fitText(name, (movieIndex == -1 ? "未知电影" : g_movies[movieIndex].name), 16);
        schedState(i, tag);

        if (g_colorOn)
        {
            if (left <= 0)                 color = "\x1b[1;33m";  /* 黄：售罄 */
            else if (isStarted(i))         color = "\x1b[1;31m";  /* 红：已开演 */
            else                           color = "\x1b[1;32m";  /* 绿：还能卖 */
        }
        else color = "";

        printf("  %-6d  %s  %-4d  %-11s  %-5s  %6.2f  %5d  %s%s%s\n",
            g_scheds[i].id, name, g_scheds[i].hallId, g_scheds[i].date,
            slotTime(g_scheds[i].slot), g_scheds[i].price, left,
            color, tag, g_colorOn ? "\x1b[0m" : "");
    }
    pauseScreen();
}


int findSchedule(int id)
{
    int i;

    for (i = 0; i < g_schedCnt; i++) {
        if (g_scheds[i].id == id) {
            return i;
        }
    }

    return -1;
}


void showOnSale(void)
{
    int  i, movieIndex, left, found = 0;
    char name[64], tag[16];
    const char* color;

    if (g_schedCnt == 0) {
        system("cls");
        printf("暂无场次。\n");
        return;
    }

    system("cls");
    printf("================ 正在售卖的场次 ================\n");
    printf("  %-6s  %-16s  %-4s  %-11s  %-5s  %6s  %5s  %s\n",
        "场次号", "电影", "影厅", "日期", "时间", "票价", "余票", "状态");

    for (i = 0; i < g_schedCnt; i++) {
        /* 已经开演的不卖；余票为 0 也不卖。
           以前只比日期，导致"今天已开场的场次还能买、买完却退不了" */
        if (isStarted(i)) continue;

        left = SEAT_TOTAL - g_scheds[i].soldCnt;
        if (left <= 0) continue;

        movieIndex = findMovie(g_scheds[i].movieId);
        fitText(name, (movieIndex == -1 ? "未知电影" : g_movies[movieIndex].name), 16);

        if (minutesToStart(i) <= 120) {
            strcpy(tag, "即将开演");
            color = g_colorOn ? "\x1b[1;33m" : "";
        }
        else {
            strcpy(tag, "售票中");
            color = g_colorOn ? "\x1b[1;32m" : "";
        }

        printf("  %-6d  %s  %-4d  %-11s  %-5s  %6.2f  %5d  %s%s%s\n",
            g_scheds[i].id, name, g_scheds[i].hallId, g_scheds[i].date,
            slotTime(g_scheds[i].slot), g_scheds[i].price, left,
            color, tag, g_colorOn ? "\x1b[0m" : "");

        found = 1;
    }

    if (found == 0)
        printf("  暂无可售场次。\n");
}


int pickSchedule(void)
{
    int id;
    int index;
    int left;

    showOnSale();

    printf("\n请输入场次编号（输入 0 取消）：");
    id = inputInt(0, 1000000);

    if (id == 0) {
        return -1;
    }

    index = findSchedule(id);
    if (index == -1) {
        printf("场次编号 %d 不存在。\n", id);
        return -1;
    }

    /* 再检查一次：开场时间过了就不卖，余票为 0 也不卖。
       这里用 isStarted 而不是比日期 —— 同一天不同时段差别很大 */
    if (isStarted(index)) {
        printf("该场次已经开场，不能再买票。\n");
        return -1;
    }

    left = SEAT_TOTAL - g_scheds[index].soldCnt;
    if (left <= 0) {
        printf("该场次已经售完。\n");
        return -1;
    }

    return index;
}



/* ==================== 批量排片 ==================== */

/* 判断某影厅、某日期、某时间段是否已有场次 */
static int scheduleExists(int hallId, const char* date, int slot)
{
    int i;

    for (i = 0; i < g_schedCnt; i++)
        if (g_scheds[i].hallId == hallId &&
            strcmp(g_scheds[i].date, date) == 0 &&
            g_scheds[i].slot == slot)
            return 1;

    return 0;
}

/* 创建一个场次（不保存文件）；冲突或已满返回 0 */
static int scheduleInsert(int movieId, int hallId, const char* date, int slot, float price)
{
    Schedule s;

    if (g_schedCnt >= MAX_SCHEDULES) return 0;
    if (scheduleExists(hallId, date, slot)) return 0;

    memset(&s, 0, sizeof(s));
    s.id = newSchedId();
    s.movieId = movieId;
    s.hallId = hallId;
    strcpy(s.date, date);
    s.slot = slot;
    s.price = price;
    s.soldCnt = 0;
    s.income = 0.0f;

    g_scheds[g_schedCnt] = s;
    g_schedCnt++;
    return 1;
}

/* 复制某天排片到另一天 */
static int copyScheduleDay(void)
{
    int srcDay, dstDay, i, cnt, added = 0, skipped = 0;
    char srcDate[11], dstDate[11];

    printf("源日期偏移（0=今天）：");
    srcDay = inputInt(0, DAY_NUM - 1);
    printf("目标日期偏移（0=今天）：");
    dstDay = inputInt(0, DAY_NUM - 1);

    addDays(srcDate, srcDay);
    addDays(dstDate, dstDay);

    if (strcmp(srcDate, dstDate) == 0)
    {
        printf("源日期和目标日期相同。\n");
        return 0;
    }

    cnt = g_schedCnt;
    for (i = 0; i < cnt; i++)
    {
        if (strcmp(g_scheds[i].date, srcDate) != 0) continue;
        if (scheduleInsert(g_scheds[i].movieId, g_scheds[i].hallId,
                           dstDate, g_scheds[i].slot, g_scheds[i].price))
            added++;
        else
            skipped++;
    }

    if (added > 0) saveCinema();
    printf("复制完成：新增 %d 场，跳过 %d 场。\n", added, skipped);
    return added;
}

/* 自动填充：日期范围 × 影厅 × 时段 */
static int autoFillSchedules(void)
{
    int startDay, endDay, mode;
    int movieId = 0, movieIdx = -1;
    int day, hall, slot;
    int added = 0, skipped = 0;
    char date[11];

    if (g_movieCnt == 0)
    {
        printf("还没有电影，请先添加。\n");
        return 0;
    }

    printf("起始日期偏移（0=今天）：");
    startDay = inputInt(0, DAY_NUM - 1);
    printf("结束日期偏移（%d~%d）：", startDay, DAY_NUM - 1);
    endDay = inputInt(startDay, DAY_NUM - 1);

    printf("1=所有电影轮播，2=指定一部电影：");
    mode = inputInt(1, 2);

    if (mode == 2)
    {
        showMovies();
        printf("请输入电影编号：");
        movieId = inputInt(1, 1000000);
        movieIdx = findMovie(movieId);
        if (movieIdx == -1)
        {
            printf("电影编号 %d 不存在。\n", movieId);
            return 0;
        }
    }

    for (day = startDay; day <= endDay; day++)
    {
        addDays(date, day);

        for (hall = 1; hall <= HALL_COUNT; hall++)
        {
            for (slot = 1; slot <= SHOW_PER_DAY; slot++)
            {
                if (g_schedCnt >= MAX_SCHEDULES)
                {
                    saveCinema();
                    printf("场次已满，提前结束。新增 %d 场，跳过 %d 场。\n", added, skipped);
                    return added;
                }

                if (scheduleExists(hall, date, slot)) { skipped++; continue; }

                if (mode == 1)
                    movieIdx = (day * HALL_COUNT * SHOW_PER_DAY +
                                (hall - 1) * SHOW_PER_DAY + (slot - 1)) % g_movieCnt;

                if (scheduleInsert(g_movies[movieIdx].id, hall, date, slot, g_movies[movieIdx].price))
                    added++;
                else
                    skipped++;
            }
        }
    }

    saveCinema();
    printf("自动填充完成：新增 %d 场，跳过 %d 场。\n", added, skipped);
    return added;
}

/* 从文本文件导入场次：日期|影厅|时段|电影编号|票价 */
static int importSchedulesFile(void)
{
    FILE* fp;
    char line[256];
    char date[11];
    char priceStr[32];
    char minDate[11], maxDate[11];
    int hall, slot, movieId, movieIdx;
    float price;
    int lineNo = 0, added = 0, skipped = 0, failed = 0;

    fp = fopen("data/schedules_batch.txt", "r");
    if (fp == NULL)
    {
        printf("找不到 data/schedules_batch.txt。\n");
        return 0;
    }

    strcpy(minDate, g_today);
    addDays(maxDate, DAY_NUM - 1);

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        lineNo++;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        if (sscanf(line, "%10[^|]|%d|%d|%d|%31[^|]",
                   date, &hall, &slot, &movieId, priceStr) != 5)
        {
            printf("第 %d 行格式错误，跳过。\n", lineNo);
            failed++;
            continue;
        }

        /* 先看是不是日历上真有这一天（2026-02-31 能过长度检查，但日子不存在），
           再看在不在可排片的时间线内 */
        if (strlen(date) != 10 || !isValidDate(date))
        {
            printf("第 %d 行日期不是合法日期，跳过。\n", lineNo);
            failed++;
            continue;
        }
        if (strcmp(date, minDate) < 0 || strcmp(date, maxDate) > 0)
        {
            printf("第 %d 行日期超出可排片时间线，跳过。\n", lineNo);
            failed++;
            continue;
        }

        if (hall < 1 || hall > HALL_COUNT || slot < 1 || slot > SHOW_PER_DAY)
        {
            printf("第 %d 行影厅或时段非法，跳过。\n", lineNo);
            failed++;
            continue;
        }

        movieIdx = findMovie(movieId);
        if (movieIdx == -1)
        {
            printf("第 %d 行电影编号不存在，跳过。\n", lineNo);
            failed++;
            continue;
        }

        price = 0.0f;
        sscanf(priceStr, "%f", &price);
        if (price <= 0.0f) price = g_movies[movieIdx].price;

        if (scheduleInsert(movieId, hall, date, slot, price))
            added++;
        else
            skipped++;
    }
    fclose(fp);

    if (added > 0) saveCinema();
    printf("文件导入完成：新增 %d 场，跳过 %d 场，失败 %d 场。\n", added, skipped, failed);
    return added;
}

/* 批量添加场次入口 */
int addSchedulesBatch(void)
{
    int op;

    printf("\n1. 复制某天排片\n");
    printf("2. 自动填充\n");
    printf("3. 文件导入\n");
    printf("0. 返回\n");
    printf("请选择 [0-3]：");
    op = inputInt(0, 3);

    if (op == 1) return copyScheduleDay();
    if (op == 2) return autoFillSchedules();
    if (op == 3) return importSchedulesFile();
    return 0;
}

/* 批量删除场次：筛选 + 编号；有有效票自动退票 */
int deleteSchedulesBatch(void)
{
    char del[MAX_SCHEDULES];
    int  sel[MAX_SCHEDULES];
    int  selCnt = 0;
    int  mode, i, j, w, s;
    int  dayOffset, hall, slot;
    int  id, dup;
    int  validCnt = 0;
    double refundTotal = 0.0;
    int  confirm;
    char date[11];

    memset(del, 0, sizeof(del));

    printf("\n1. 按日期/影厅/时段筛选\n2. 按场次编号\n0. 返回\n请选择 [0-2]：");
    mode = inputInt(0, 2);
    if (mode == 0) return 0;

    if (mode == 1)
    {
        printf("日期偏移（0=今天）：");
        dayOffset = inputInt(0, DAY_NUM - 1);
        addDays(date, dayOffset);
        printf("影厅（0=全部，1~%d）：", HALL_COUNT);
        hall = inputInt(0, HALL_COUNT);
        printf("时段（0=全部，1~%d）：", SHOW_PER_DAY);
        slot = inputInt(0, SHOW_PER_DAY);

        for (i = 0; i < g_schedCnt; i++)
        {
            if (strcmp(g_scheds[i].date, date) != 0) continue;
            if (hall != 0 && g_scheds[i].hallId != hall) continue;
            if (slot != 0 && g_scheds[i].slot != slot) continue;
            sel[selCnt++] = i;
        }
    }
    else
    {
        printf("输入场次编号，输入 0 结束：\n");
        while (selCnt < MAX_SCHEDULES)
        {
            printf("场次编号：");
            id = inputInt(0, 1000000);
            if (id == 0) break;

            i = findSchedule(id);
            if (i == -1) { printf("场次不存在。\n"); continue; }

            dup = 0;
            for (j = 0; j < selCnt; j++)
                if (sel[j] == i) { dup = 1; break; }
            if (dup) { printf("已选过。\n"); continue; }

            sel[selCnt++] = i;
        }
    }

    if (selCnt == 0) { printf("没有符合条件的场次。\n"); return 0; }

    /* 预览有效票 */
    for (i = 0; i < selCnt; i++)
    {
        s = sel[i];
        /* 改签票（4）也在这场上，撤场同样要赔，否则会变成“场次没了、票还有效”的孤儿票 */
        for (j = 0; j < g_ticketCnt; j++)
            if (g_tickets[j].schedId == g_scheds[s].id &&
                TICKET_VALID(g_tickets[j].state))
            {
                validCnt++;
                refundTotal += g_tickets[j].price;
            }
    }

    printf("\n将删除 %d 个场次。\n", selCnt);
    if (validCnt > 0)
    {
        printf("其中 %d 张有效票将被自动作废，全额退款 %.2f 元。\n", validCnt, refundTotal);
        printf("删除后订单记录仍保留（状态改为已退）。\n");
    }
    printf("确认删除？1=是，0=取消：");
    confirm = inputInt(0, 1);
    if (confirm == 0) { printf("已取消。\n"); return 0; }

    /* 先退票 */
    for (i = 0; i < selCnt; i++)
    {
        s = sel[i];
        /* 撤场属于影院方原因：不走阶梯手续费，任何在售票一律全额作废 */
        for (j = 0; j < g_ticketCnt; j++)
            if (g_tickets[j].schedId == g_scheds[s].id &&
                TICKET_VALID(g_tickets[j].state))
                ticketForceVoid(j, g_currentAdminId);
    }

    /* 标记删除 */
    for (i = 0; i < selCnt; i++) del[sel[i]] = 1;

    /* 重建数组，跳过被删的场次 */
    w = 0;
    for (i = 0; i < g_schedCnt; i++)
        if (!del[i]) g_scheds[w++] = g_scheds[i];
    g_schedCnt = w;

    saveCinema();
    saveTickets();
    printf("删除完成：共删除 %d 个场次，退票 %d 张。\n", selCnt, validCnt);
    writeAdminLog(g_currentAdminAccount, "批量删除场次");
    return selCnt;
}

