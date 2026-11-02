/*==================================================================
  movie.c  ——  电影管理（框架空壳）
==================================================================*/
#include "cinema.h"

int addMovie(void)
{
    printf("【框架提示：添加电影】\n");
    pauseScreen();
    return 0;
}

void showMovies(void)
{
    printf("【框架提示：查看电影列表】\n");
    pauseScreen();
}

int findMovie(int id)
{
    printf("【框架提示：按编号查找电影（id=%d）】\n", id);
    return -1;
}
