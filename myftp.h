#define SRVPORT "50021"
#define DATASIZE 1024
#define NARGS  256

struct myftph_data {
    uint8_t  type;
    uint8_t  code;
    uint16_t length;
    char     data[DATASIZE];
};

struct myftph {
    uint8_t  type;
    uint8_t  code;
    uint16_t length;
};

void getargs(int *, char *[], char *);

void quit_proc(int, int, char *[]);
void pwd_proc(int, int, char *[]);
void cd_proc(int, int, char *[]);
void dir_proc(int, int, char *[]);
void lpwd_proc(int, int, char *[]);
void lcd_proc(int, int, char *[]);
void ldir_proc(int, int, char *[]);
void get_proc(int, int, char *[]);
void put_proc(int, int, char *[]);
void help_proc(int, int, char *[]);

void quit_exec(int);
void pwd_exec(int);
void cwd_exec(int);
void list_exec(int);
void retr_exec(int);
void stor_exec(int);

int msg_send(int);
int msg_data_send(int);
int msg_recv(int);
int msg_data_recv(int);
