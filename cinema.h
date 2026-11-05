
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
    int   state;            /* 1=有效 2=已退 3=已改签(老票) 4=改签新票 */
    char  purchase_time[DATETIME_LEN];
    char  refund_time[DATETIME_LEN];
    int   refund_admin_id;  /* 0=用户自己退，>0=管理员 id */
    char  buyerName[20];    /* 购票人快照：用户以后改名，这张票上不会跟着变。
                               放在最后，让前 144 字节和老格式完全一样，
                               迁移时整块照搬即可 */
} Ticket;

/* 票的状态。界面写的是"有效 / 已退 / 已改签 / 改签新票"，
   代码里就别再写 1/2/3/4 了——加第五种状态时不用满文件搜数字。 */
typedef enum
{
    TICKET_OK          = 1,   /* 正常票：可退、可改签 */
    TICKET_REFUNDED    = 2,   /* 已退票 */
    TICKET_CHANGED_OLD = 3,   /* 改签后作废的老票，只留痕 */
    TICKET_CHANGED_NEW = 4    /* 改签得到的新票：能进场、计票房，不可退不可改 */
} TicketState;

/* 有效票：正常票 + 改签新票（都能进场、都计票房）。
   凡是判断“这张票还算不算数”，一律用它 */
#define TICKET_VALID(st) ((st) == TICKET_OK || (st) == TICKET_CHANGED_NEW)

/* 购票结果码：业务内核用它把"为什么没买成"交给调用方，
   不再靠 printf 把原因打到屏幕上。
   BUY_OK 固定为 0，其余是失败原因。 */
typedef enum
{
    BUY_OK = 0,          /* 出票成功 */
    BUY_CANCELLED,       /* 观众自己放弃 */
    BUY_TABLE_FULL,      /* 订单表已满 */
    BUY_NO_SCHED,        /* 场次不存在 */
    BUY_STARTED,         /* 已经开场 */
    BUY_BAD_SEAT,        /* 排号/座号越界 */
    BUY_SOLD_OUT,        /* 该场满座 */
    BUY_LIMIT,           /* 超过每人限购 */
    BUY_SEAT_TAKEN       /* 座位已被占 */
} BuyResult;

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
int   minutesToStart(int sIdx);   /* 距开演还有几分钟，已开场为负 */
int   isStarted(int sIdx);        /* 1=已开场 */
float calcFee(long minutesLeft, int isChange);  /* 阶梯手续费，改签取半价 */
const char* userNameById(int uid);              /* 按编号查用户名，找不到返回空串 */

/* 控制台：颜色与字符宽度（非控制台自动降级） */
extern int g_colorOn;
extern int g_cellW;
void initConsoleColor(void);
void inputPassword(char* buf, int size);   /* 输入时显示 * */
const char* rowName(int row);
void addDays(char* out, int days);
void getNow(char* out);

/* 公共工具：宽度补齐、票房加减、日期合法性。
   以前宽度补齐在 ticket.c / schedule.c 各有一份，现在只此一份 */
void fitText(char* out, const char* src, int cols);
void addIncome(int sIdx, float delta);       /* 按"分"四舍五入后加减票房 */
int  isValidDate(const char* s);             /* 2026-02-31 这类返回 0 */

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
void userMenu(int uid, char* name);
void userProfile(int uid, char* name);   /* 改昵称 / 改密码，name 就地更新 */
int  changeTicket(int uid);              /* 改签：半价手续费，限一次 */

void showOnSale(void);
int  pickSchedule(void);

void showSeats(int sIdx);
int  countLeft(int sIdx);
int  seatSell(int sIdx, int row, int col);
int  seatFree(int sIdx, int row, int col);

int  buyTicket(int uid);

/* 购票内核：只判断规则、写内存，不打印、不读键盘、不存盘。
   控制台壳 buyTicket() 和以后的网页/抢票入口都调它。 */
BuyResult ticketBuyCore(int uid, int schedId, int row, int col, Ticket* out);
int  refundTicket(int uid);
void showMyTickets(int uid);
void showAllOrders(void);
void showOrdersByUser(void);
int  batchRefundMenu(void);
int  ticketRefundOne(int tIdx, int adminId);
int  ticketForceVoid(int tIdx, int adminId);   /* 影院撤场：改签票也一并作废 */
float refundFeeBySched(int sIdx);              /* 统一退票费率，谁退都走这里 */
const char* ticketStateText(int state);        /* 有效 / 已退 / 已改签 / 改签票 */

/*==================== 八、系统管理 / 日志 ====================*/
void writeAdminLog(const char* who, const char* action);
void showAdminLog(void);
int  backupAllData(void);
int  restoreAllData(void);
void checkData(void);
void calculateBoxOfficeRange(void);

#endif
