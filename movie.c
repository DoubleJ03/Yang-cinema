#include "cinema.h"


static float inputFloat(const char* prompt, float minValue, float maxValue)
{
    float value;

    while (1) {
        printf("%s", prompt);

        if (scanf("%f", &value) != 1) {
            while (getchar() != '\n') {
            }
            printf("输入错误，请输入数字。\n");
            continue;
        }

        while (getchar() != '\n') {
        }

        if (value >= minValue && value <= maxValue) {
            return value;
        }

        printf("请输入 %.2f 到 %.2f 之间的数。\n", minValue, maxValue);
    }
}

/* 校验并写入一部电影（不保存文件），成功返回 1，失败返回 0 */
static int movieInsert(Movie* m)
{
    int i;

    if (g_movieCnt >= MAX_MOVIES) return 0;
    if (m->name[0] == '\0') return 0;

    for (i = 0; i < g_movieCnt; i++) {
        if (strcmp(g_movies[i].name, m->name) == 0) return 0;
    }

    m->id = newMovieId();
    g_movies[g_movieCnt] = *m;
    g_movieCnt++;
    return 1;
}

/* 从控制台读入一部电影，成功返回 1，失败返回 0 */
static int inputMovie(Movie* m)
{
    printf("请输入电影名称：");
    inputStr(m->name, sizeof(m->name));

    m->price = inputFloat("请输入票价（元）：", 1.0f, 999.0f);

    printf("请输入电影时长（分钟）：");
    m->minutes = inputInt(1, 300);

    return 1;
}

int addMovie(void)
{
    Movie m;

    if (!inputMovie(&m)) return 0;

    if (!movieInsert(&m)) {
        printf("电影《%s》已经存在，或数量已满。\n", m.name);
        return 0;
    }

    saveCinema();
    printf("添加成功，电影编号：%d\n", m.id);
    writeAdminLog(g_currentAdminAccount, "添加电影");
    return 1;
}

void showMovies(void)
{
    int i;

    if (g_movieCnt == 0) {
        printf("暂无电影，请先添加。\n");
        return;
    }

    printf("\n编号\t名称\t\t票价\t时长\n");

    for (i = 0; i < g_movieCnt; i++) {
        printf("%d\t%-16s\t%.2f\t%d 分钟\n",
            g_movies[i].id,
            g_movies[i].name,
            g_movies[i].price,
            g_movies[i].minutes);
    }
    pauseScreen();
}

int findMovie(int id)
{
    int i;

    for (i = 0; i < g_movieCnt; i++) {
        if (g_movies[i].id == id) {
            return i;
        }
    }

    return -1;
}

/* 批量：输入数量连续录入 */
static int addMoviesByCount(void)
{
    int n, i, added = 0, failed = 0;
    Movie m;

    printf("请输入要添加的电影数量：");
    n = inputInt(1, 1000);

    for (i = 0; i < n; i++) {
        printf("\n—— 第 %d/%d 部 ——\n", i + 1, n);
        if (inputMovie(&m) && movieInsert(&m)) {
            added++;
        } else {
            failed++;
        }
    }

    if (added > 0) saveCinema();
    printf("批量添加完成：成功 %d 部，失败 %d 部。\n", added, failed);
    return added;
}

/* 批量：从文本文件导入，格式 电影名|票价|时长 */
static int importMoviesFile(void)
{
    FILE* fp;
    char line[256];
    int lineNo = 0, added = 0, failed = 0;
    Movie m;

    fp = fopen("data/movies_batch.txt", "r");
    if (fp == NULL) {
        printf("找不到 data/movies_batch.txt，请先创建。\n");
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        lineNo++;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        if (sscanf(line, "%49[^|]|%f|%d", m.name, &m.price, &m.minutes) != 3) {
            printf("第 %d 行格式错误，跳过。\n", lineNo);
            failed++;
            continue;
        }
        if (m.name[0] == '\0' || m.price <= 0.0f || m.minutes < 1 || m.minutes > 300) {
            printf("第 %d 行数据非法，跳过。\n", lineNo);
            failed++;
            continue;
        }

        if (movieInsert(&m)) {
            added++;
        } else {
            printf("第 %d 行重名或已满，跳过。\n", lineNo);
            failed++;
        }
    }
    fclose(fp);

    if (added > 0) saveCinema();
    printf("文件导入完成：成功 %d 部，失败 %d 部。\n", added, failed);
    return added;
}

int addMoviesBatch(void)
{
    int op;
    int r = 0;

    printf("\n1. 输入数量连续录入\n");
    printf("2. 从文件导入 data/movies_batch.txt\n");
    printf("0. 返回\n");
    printf("请选择 [0-2]：");
    op = inputInt(0, 2);

    if (op == 1) r = addMoviesByCount();
    else if (op == 2) r = importMoviesFile();
    else return 0;

    if (r > 0) writeAdminLog(g_currentAdminAccount, "批量添加电影");
    return r;
}

int deleteMoviesBatch(void)
{
    int ids[MAX_MOVIES];
    int n = 0, id, i, j;
    int dup, has, idx;
    int deleted = 0, skipped = 0;

    printf("请输入要删除的电影编号，输入 0 结束：\n");
    while (n < MAX_MOVIES) {
        printf("电影编号：");
        id = inputInt(0, 1000000);
        if (id == 0) break;

        dup = 0;
        for (j = 0; j < n; j++) {
            if (ids[j] == id) { dup = 1; break; }
        }
        if (dup) { printf("该编号已选过。\n"); continue; }

        ids[n++] = id;
    }

    if (n == 0) {
        printf("未选择任何电影。\n");
        return 0;
    }

    for (i = 0; i < n; i++) {
        idx = findMovie(ids[i]);
        if (idx == -1) {
            printf("电影编号 %d 不存在，跳过。\n", ids[i]);
            skipped++;
            continue;
        }

        has = 0;
        for (j = 0; j < g_schedCnt; j++) {
            if (g_scheds[j].movieId == ids[i]) { has = 1; break; }
        }

        if (has) {
            printf("电影《%s》还有排片，未删除。\n", g_movies[idx].name);
            skipped++;
            continue;
        }

        for (j = idx; j < g_movieCnt - 1; j++) {
            g_movies[j] = g_movies[j + 1];
        }
        g_movieCnt--;
        deleted++;
    }

    if (deleted > 0) { saveCinema(); writeAdminLog(g_currentAdminAccount, "批量删除电影"); }
    printf("批量删除完成：删除 %d 部，跳过 %d 部。\n", deleted, skipped);
    return deleted;
}
