
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
#define DAY_NUM        90
#define MAX_SCHEDULES  3000

#define MAX_USERS    500
#define MAX_MOVIES    100
#define MAX_TICKETS 50000
#define MAX_ADMINS     20
#define ADMIN_PWD "123456"

#define MAX_PER_USER 4
#define DATETIME_LEN 20     /* "YYYY-MM-DD HH:MM:SS" + '\0' */


#define DATA_DIR    "data"
#define FILE_USER     "data/user.dat"
#define FILE_CINEMA   "data/cinema.dat"
#define FILE_TICKET   "data/ticket.dat"
#define FILE_ADMIN    "data/admin.dat"
#define FILE_ADMIN_LOG "data/admin_log.txt"

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
    int   hallId;           /* 影厅快照，场次删除后仍可显示 */
    int   row;
    int   col;
    float price;
    int   state;            /* 1=有效 2=已退 */
    char  purchase_time[DATETIME_LEN];
    char  refund_time[DATETIME_LEN];
    int   refund_admin_id;  /* 0=用户自己退，>0=管理员 id */
} Ticket;

typedef struct
{
    int   id;
    char  account[20];
    char  pwd[20];
    int   role;             /* 1=超级管理员 2=普通管理员 */
    int   status;           /* 1=允许登录 0=禁止登录 */
    char  create_time[DATETIME_LEN];
    char  last_login[DATETIME_LEN];
} Admin;

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
extern Admin    g_admins[MAX_ADMINS];
extern int      g_adminCnt;
extern int      g_currentAdminId;
extern int      g_currentAdminRole;
extern char     g_currentAdminAccount[20];

/*==================== 四、数据层 ====================*/
void dataInit(void);
void dataSaveAll(void);
void loadUsers(void);       void saveUsers(void);
void loadCinema(void);      void saveCinema(void);
void loadTickets(void);     void saveTickets(void);
void loadAdmins(void);      void saveAdmins(void);

/*==================== 五、工具 ====================*/
int  inputInt(int lo, int hi);
void inputStr(char* buf, int size);
void pauseScreen(void);
void showMainMenu(void);

const char* slotTime(int slot);
const char* rowName(int row);
void addDays(char* out, int days);
void getNow(char* out);

int  newUserId(void);
int  newMovieId(void);
int  newSchedId(void);
int  newTicketId(void);
int  newAdminId(void);

/*==================== 六、管理员端====================*/
int  adminLogin(void);
void adminMenu(void);

int  addMovie(void);
void showMovies(void);
int  findMovie(int id);

int  addSchedule(void);
void showSchedules(void);
int  findSchedule(int id);
int  addSchedulesBatch(void);
int  deleteSchedulesBatch(void);

void calculateBoxOffice(void);
void sortBoxOffice(void);
void exportCSVMenu(void);

/* 批量电影 */
int  addMoviesBatch(void);
int  deleteMoviesBatch(void);

/* 管理员管理 */
void adminList(void);
int  adminAdd(void);
int  adminResetPwd(void);
int  adminSetStatus(int enable);
int  adminDelete(void);

/*==================== 七、观众端====================*/
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
void showAllOrders(void);
void showOrdersByUser(void);
int  batchRefundMenu(void);
int  ticketRefundOne(int tIdx, int adminId);

/*==================== 八、系统管理 / 日志 ====================*/
void writeAdminLog(const char* who, const char* action);
void showAdminLog(void);
int  backupAllData(void);
int  restoreAllData(void);
void checkData(void);
void calculateBoxOfficeRange(void);

#endif
