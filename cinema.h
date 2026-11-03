
#ifndef CINEMA_H
#define CINEMA_H

#pragma warning(disable:4996)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*==================== 一、常量 ====================*/
#define HALL_COUNT     5
#define ROW_COUNT     10
#define COL_COUNT     10
#define SEAT_TOTAL  (ROW_COUNT * COL_COUNT)

#define SHOW_PER_DAY   3
#define DAY_NUM        7
#define MAX_SCHEDULES (HALL_COUNT * SHOW_PER_DAY * DAY_NUM)

#define MAX_USERS    200
#define MAX_MOVIES    30
#define MAX_TICKETS 2500
#define ADMIN_PWD "123456"
// 限购：单个账号同一场放映最多购买票数
#define MAX_PER_USER 4


#define DATA_DIR    "data"
#define FILE_USER     "data/user.dat"
#define FILE_CINEMA   "data/cinema.dat"
#define FILE_TICKET   "data/ticket.dat"

/*==================== 二、结构体 ====================*/
typedef struct
{
    int  id;
    char name[20];
    char pwd[20];
} User;

typedef struct
{
    int   id;
    char  name[50];
    float price;
    int   minutes;
} Movie;

typedef struct
{
    int   id;
    int   movieId;
    int   hallId;
    char  date[11];
    int   slot;             /* 1=10:00 2=14:30 3=19:00 */
    float price;
    int   seat[ROW_COUNT][COL_COUNT];
    int   soldCnt;
    float income;
} Schedule;

typedef struct
{
    int   id;
    int   userId;
    int   schedId;
    char  movieName[50];
    char  date[11];
    int   slot;
    int   row;
    int   col;
    float price;
    int   state;            /* 1=有效 2=已退 */
} Ticket;

/*==================== 三、全局数据 ====================*/
extern User     g_users[MAX_USERS];
extern int      g_userCnt;
extern Movie    g_movies[MAX_MOVIES];
extern int      g_movieCnt;
extern Schedule g_scheds[MAX_SCHEDULES];
extern int      g_schedCnt;
extern Ticket   g_tickets[MAX_TICKETS];
extern int      g_ticketCnt;
extern char     g_today[11];

/*==================== 四、数据层 ====================*/
void dataInit(void);
void dataSaveAll(void);
void loadUsers(void);       void saveUsers(void);
void loadCinema(void);      void saveCinema(void);
void loadTickets(void);     void saveTickets(void);

/*==================== 五、工具 ====================*/
int  inputInt(int lo, int hi);
void inputStr(char* buf, int size);
void pauseScreen(void);
void showMainMenu(void);

const char* slotTime(int slot);
const char* rowName(int row);
void addDays(char* out, int days);

int  newUserId(void);
int  newMovieId(void);
int  newSchedId(void);
int  newTicketId(void);

/*==================== 六、管理员端（A）====================*/
int  adminLogin(void);
void adminMenu(void);

int  addMovie(void);
void showMovies(void);
int  findMovie(int id);

int  addSchedule(void);
void showSchedules(void);
int  findSchedule(int id);

void calculateBoxOffice(void);
void sortBoxOffice(void);

/*==================== 七、观众端（B）====================*/
int  registerUser(void);
int  loginUser(char* name, int* uid);
void userMenu(int uid, const char* name);

void showOnSale(void);
int  pickSchedule(void);

void showSeats(int sIdx);
int  countLeft(int sIdx);
int  seatSell(int sIdx, int row, int col);
int  seatFree(int sIdx, int row, int col);

int  buyTicket(int uid);
int  refundTicket(int uid);
void showMyTickets(int uid);

#endif
