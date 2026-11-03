#define _CRT_SECURE_NO_WARNINGS
#include "cinema.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * 文件：schedule.c
 * 功能：排片、场次显示、场次查找、在售场次、选场次
 *
 * 说明：
 *   1. g_scheds、g_schedCnt、g_today 定义在 main.c。
 *   2. saveCinema()、inputInt()、inputStr() 定义在 main.c。
 *   3. findMovie()、showMovies() 定义在 movie.c。
 *   4. showOnSale()、pickSchedule() 放在本文件，
 *      因此 user.c 里不要再定义一次。
 */

 /* ============================================================
  * 函数：slotTime
  * 功能：时间段编号转时间字符串
  * 参数：slot 1/2/3
  * 返回：对应时间；非法编号返回 "未知"
  * ============================================================ */

/* ============================================================
 * 函数：addDays
 * 功能：以 g_today 为基准，计算 days 天后的日期
 * 参数：out  输出缓冲区，至少 11 字节
 *       days 要加的天数
 * 返回：无
 * ============================================================ */

/* ============================================================
 * 函数：newSchedId
 * 功能：生成新的场次编号（当前最大编号 + 1）
 * 参数：无
 * 返回：新的场次编号
 * ============================================================ */


/* ============================================================
 * 函数：addSchedule
 * 功能：添加一个电影场次
 * 参数：无
 * 返回：1 成功，0 失败
 * ============================================================ */
 /* ============================================================
  * 函数：inputPriceOrDefault
  * 功能：读取票价；直接回车使用默认价
  * 参数：defaultPrice 默认票价
  * 返回：合法票价；输入非法返回 -1.0f
  * ============================================================ */
  /* ============================================================
   * 函数：readPriceOrDefault
   * 功能：读取票价；直接回车使用默认价
   * 参数：defaultPrice 默认票价
   * 返回：合法票价；输入非法返回 -1.0f
   * 说明：static 表示只在 schedule.c 内部使用
   * ============================================================ */
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
    char priceBuf[20];           /* 自定义票价输入 */
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

    /* 5. 选择日期：0=今天，1=明天，依此类推 */
    printf("\n请选择放映日期：\n");
    for (i = 0; i < DAY_NUM; i++) {
        addDays(date, i);
        printf("%d. %s\n", i, date);
    }

    printf("请输入 0~%d：", DAY_NUM - 1);
    dayOffset = inputInt(0, DAY_NUM - 1);
    addDays(date, dayOffset);

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
    return 1;
}

/* ============================================================
 * 函数：showSchedules
 * 功能：显示所有场次（管理员用）
 * 参数：无
 * 返回：无
 * ============================================================ */
void showSchedules(void)
{
    int i;
    int movieIndex;
    int left;

    if (g_schedCnt == 0) {
        printf("暂无场次，请先排片。\n");
        return;
    }

    printf("\n场次号\t电影\t\t影厅\t日期\t\t时间\t票价\t余票\n");

    for (i = 0; i < g_schedCnt; i++) {
        movieIndex = findMovie(g_scheds[i].movieId);
        left = SEAT_TOTAL - g_scheds[i].soldCnt;
        if (left < 0) {
            left = 0;
        }

        printf("%d\t%-16s\t%d\t%s\t%s\t%.2f\t%d\n",
            g_scheds[i].id,
            (movieIndex == -1 ? "未知电影" : g_movies[movieIndex].name),
            g_scheds[i].hallId,
            g_scheds[i].date,
            slotTime(g_scheds[i].slot),
            g_scheds[i].price,
            left);
    }
    pauseScreen();

}

/* ============================================================
 * 函数：findSchedule
 * 功能：按场次编号查找
 * 参数：id 场次编号
 * 返回：数组下标，找不到返回 -1
 * 注意：返回下标，不是场次编号。
 * ============================================================ */
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

/* ============================================================
 * 函数：showOnSale
 * 功能：显示今天及以后、还有余票的场次（观众用）
 * 参数：无
 * 返回：无
 * ============================================================ */
void showOnSale(void)
{
    int i;
    int movieIndex;
    int left;
    int found = 0;

    if (g_schedCnt == 0) {
        printf("暂无场次。\n");
        return;
    }

    printf("\n===== 正在售卖的场次 =====\n");
    printf("场次号\t电影\t\t影厅\t日期\t\t时间\t票价\t余票\n");

    for (i = 0; i < g_schedCnt; i++) {
        /* 只显示今天及以后的场次，过去的场次不卖 */
        if (strcmp(g_scheds[i].date, g_today) < 0) {
            continue;
        }

        left = SEAT_TOTAL - g_scheds[i].soldCnt;
        if (left <= 0) {
            continue;                /* 卖完了，不显示 */
        }

        movieIndex = findMovie(g_scheds[i].movieId);

        printf("%d\t%-16s\t%d\t%s\t%s\t%.2f\t%d\n",
            g_scheds[i].id,
            (movieIndex == -1 ? "未知电影" : g_movies[movieIndex].name),
            g_scheds[i].hallId,
            g_scheds[i].date,
            slotTime(g_scheds[i].slot),
            g_scheds[i].price,
            left);

        found = 1;
    }

    if (found == 0) {
        printf("暂无可售场次。\n");
    }
}

/* ============================================================
 * 函数：pickSchedule
 * 功能：显示在售场次，让用户输入场次编号，返回数组下标
 * 参数：无
 * 返回：场次数组下标；取消或失败返回 -1
 * 注意：返回的是下标，不是场次编号。
 * ============================================================ */
int pickSchedule(void)
{
    int id;
    int index;
    int left;

    showOnSale();

    printf("请输入场次编号（输入 0 取消）：");
    id = inputInt(0, 1000000);

    if (id == 0) {
        return -1;
    }

    index = findSchedule(id);
    if (index == -1) {
        printf("场次编号 %d 不存在。\n", id);
        return -1;
    }

    /* 再检查一次：日期不能是过去，余票不能为 0 */
    if (strcmp(g_scheds[index].date, g_today) < 0) {
        printf("该场次已经过期。\n");
        return -1;
    }

    left = SEAT_TOTAL - g_scheds[index].soldCnt;
    if (left <= 0) {
        printf("该场次已经售完。\n");
        return -1;
    }

    return index;
}
