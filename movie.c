/*==================================================================
  movie.c  ——  电影管理（框架空壳）
==================================================================*/
#include "cinema.h"


static float inputFloat(const char* prompt, float minValue, float maxValue)
{
    float value;                 /* 保存用户输入的浮点数 */

    while (1) {
        printf("%s", prompt);

        /* scanf 返回 1 才表示成功读到一个浮点数 */
        if (scanf("%f", &value) != 1) {
            /* 输入了字母等非法内容，清空这一行 */
            while (getchar() != '\n') {
                /* 空循环 */
            }
            printf("输入错误，请输入数字。\n");
            continue;
        }

        /* 吃掉数字后面的回车，防止影响下一次输入 */
        while (getchar() != '\n') {
            /* 空循环 */
        }

        /* 检查范围 */
        if (value >= minValue && value <= maxValue) {
            return value;
        }

        printf("请输入 %.2f 到 %.2f 之间的数。\n", minValue, maxValue);
    }
} 
int addMovie(void)
{
    Movie m;                     /* 临时保存新电影的信息 */
    int i;                       /* 循环变量，用来查重 */

    /* 1. 先判断数组是否已满 */
    if (g_movieCnt >= MAX_MOVIES) {
        printf("电影数量已满，最多 %d 部。\n", MAX_MOVIES);
        return 0;
    }

    /* 2. 输入电影名称 */
    printf("请输入电影名称：");
    inputStr(m.name, sizeof(m.name));

    if (m.name[0] == '\0') {
        printf("电影名称不能为空。\n");
        return 0;
    }

    /* 3. 检查是否重名 */
    for (i = 0; i < g_movieCnt; i++) {
        if (strcmp(g_movies[i].name, m.name) == 0) {
            printf("电影《%s》已经存在。\n", m.name);
            return 0;
        }
    }

    /* 4. 输入票价和时长 */
    m.price = inputFloat("请输入票价（元）：", 1.0f, 999.0f);

    printf("请输入电影时长（分钟）：");
    m.minutes = inputInt(1, 300);

    /* 5. 生成电影编号：当前最大编号 + 1 */
    m.id = newMovieId();

    /* 6. 放到数组末尾，然后数量加 1 */
    g_movies[g_movieCnt] = m;
    g_movieCnt++;

    /* 7. 写入 data/cinema.dat */
    saveCinema();

    printf("添加成功，电影编号：%d\n", m.id);
    return 0;
}

void showMovies(void)
{
    int i;                       /* 循环变量 */

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
    int i;                       /* 循环变量 */

    for (i = 0; i < g_movieCnt; i++) {
        if (g_movies[i].id == id) {
            return i;
        }
    }

    return -1;
}
