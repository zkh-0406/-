#include "lcd.h"
#include "Bmp.h"
#include "touch.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define PASSWORD "1234"
#define MAX_ATTEMPTS 3

char * bmpname[] = {"1.bmp","2.bmp","3.bmp","4.bmp","5.bmp","6.bmp"};
#define BMP_COUNT (sizeof(bmpname) / sizeof(bmpname[0]))

int current_index = 0;
bool auto_play = false;
pthread_mutex_t mutex;
pthread_t auto_play_thread;
pthread_t music_thread;
int ts_x_start = 0, ts_y_start = 0;

// 自动播放线程函数
void *auto_play_function(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        if (auto_play) {
            current_index = (current_index + 1) % BMP_COUNT;
            show_bmp(bmpname[current_index], 0, 0);
            printf("auto: %s\n", bmpname[current_index]);
        }
        pthread_mutex_unlock(&mutex);
        sleep(3); // 每 3 秒切换一次图片
    }
    return NULL;
}

// 音乐播放线程函数
void *music_play_function(void *arg) {
    // 使用 madplay 播放 1.mp3
    system("madplay -Q 1.mp3");
    return NULL;
}

// 获取触摸屏坐标
void get_touch_coordinates(int *x, int *y, int *rs) {
    int fd = open("/dev/input/event0", O_RDONLY);
    if (fd < 0) {
        perror("open event0");
        return;
    }

    struct input_event input_buf;
    int x_start = -1, y_start = -1;
    while (1) {
        read(fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS) {
            if (input_buf.code == ABS_X) {
                if (x_start == -1) {
                    x_start = input_buf.value * 800 / 1024;
                }
                *x = input_buf.value * 800 / 1024;
            } else if (input_buf.code == ABS_Y) {
                if (y_start == -1) {
                    y_start = input_buf.value * 480 / 600;
                }
                *y = input_buf.value * 480 / 600;
            }
        } else if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            if (x_start != -1 && *x != -1) {
                if (*x - x_start > 0) {
                    *rs = 4; // 右滑
                } else if (*x - x_start < 0) {
                    *rs = 3; // 左滑
                } else {
                    *rs = 0; // 无有效滑动
                }
            }
            break;
        }
    }
    close(fd);
}

// 密码输入函数
bool input_password() {
    int attempts = 0;
    char input[10] = {0};
    int ts_x, ts_y, rs;
    int fd = ts_open();

    while (attempts < MAX_ATTEMPTS) {
        show_bmp("password.bmp", 0, 0);
        memset(input, 0, sizeof(input));
        int index = 0;

        while (index < 4) {
            get_touch_coordinates(&ts_x, &ts_y, &rs);
            // 输出触摸坐标
            printf(" x and y address: x = %d, y = %d\n", ts_x, ts_y);

            // 数字 1
            if ((ts_x>250)&&(ts_x<300)&&(ts_y>130)&&(ts_y<195)) {
                input[index++] = '1';
            }
            if ((ts_x>360)&&(ts_x<420)&&(ts_y>140)&&(ts_y<200)) {
                input[index++] = '2';
            }
            if ((ts_x>480)&&(ts_x<530)&&(ts_y>130)&&(ts_y<180)) {
                input[index++] = '3';
            }
            if ((ts_x>250)&&(ts_x<310)&&(ts_y>210)&&(ts_y<270)) {
                input[index++] = '4';
            }
            if ((ts_x>360)&&(ts_x<420)&&(ts_y>210)&&(ts_y<260)) {
                input[index++] = '5';
            }
            if ((ts_x>490)&&(ts_x<560)&&(ts_y>210)&&(ts_y<270)) {
                input[index++] = '6';
            }
            if ((ts_x>250)&&(ts_x<310)&&(ts_y>270)&&(ts_y<340)) {
                input[index++] = '7';
            }
            if ((ts_x>360)&&(ts_x<420)&&(ts_y>290)&&(ts_y<340)) {
                input[index++] = '8';
            }
            if ((ts_x>470)&&(ts_x<540)&&(ts_y>270)&&(ts_y<340)) {
                input[index++] = '9';
            }
            if ((ts_x>360)&&(ts_x<420)&&(ts_y>350)&&(ts_y<390)) {
                input[index++] = '0';
            }
            if ((ts_x>250)&&(ts_x<350)&&(ts_y>350)&&(ts_y<410)) {
                if (index > 0) {
                    input[--index] = '\0';
                }
            }
            // 新增打印实际输入的密码数字
            printf("Current password input: %s\n", input);
        }

        if (strcmp(input, PASSWORD) == 0) {
            close(fd);
            return true;
        } else {
            attempts++;
            show_bmp("error.bmp", 0, 0);
            sleep(2);
        }
    }
    close(fd);
    return false;
}

// 主函数
int main(int argc, char const *argv[]) {
    int rs = 0;

    // 初始化 LCD
    lcd_init();

    // 显示开机界面
    show_bmp("welcome.bmp", 0, 0);
    sleep(2);

    // 密码验证
    if (!input_password()) {
        lcd_close();
        return 1;
    }

    // 显示 ture.bmp 两秒
    show_bmp("ture.bmp", 0, 0);
    sleep(2);

    // 初始化互斥锁
    pthread_mutex_init(&mutex, NULL);

    // 创建自动播放线程
    pthread_create(&auto_play_thread, NULL, auto_play_function, NULL);

    // 创建音乐播放线程
    pthread_create(&music_thread, NULL, music_play_function, NULL);

    // 触摸
    while (1) {
        int ts_x = 0, ts_y = 0;
        get_touch_coordinates(&ts_x, &ts_y, &rs);

        // 增加打印坐标的功能
        printf("address: x = %d, y = %d\n", ts_x, ts_y);

        // 记录触摸开始坐标
        ts_x_start = ts_x;
        ts_y_start = ts_y;

        // 点击左上角开始/停止自动播放
        if (ts_x < 100 && ts_y < 100) {
            pthread_mutex_lock(&mutex);
            auto_play = !auto_play;
            if (auto_play) {
                printf("start play return 0\n");
            } else {
                printf("stop play\n");
            }
            pthread_mutex_unlock(&mutex);
        }
        // 停止自动播放并且在左下角左滑时，返回上一张图片
        else if (!auto_play && ts_x_start < 100 && ts_y_start > 300 && rs == 3) { // 左滑
            pthread_mutex_lock(&mutex);
            current_index = (current_index - 1 + BMP_COUNT) % BMP_COUNT;
            show_bmp(bmpname[current_index], 0, 0);
            printf("to previous: %s\n", bmpname[current_index]);
            pthread_mutex_unlock(&mutex);
        }
        // 停止自动播放并且在右下角右滑时，返回下一张图片
        else if (!auto_play && ts_x_start > 500 && ts_y_start > 300 && rs == 4) { // 右滑
            pthread_mutex_lock(&mutex);
            current_index = (current_index + 1) % BMP_COUNT;
            show_bmp(bmpname[current_index], 0, 0);
            printf("to next: %s\n", bmpname[current_index]);
            pthread_mutex_unlock(&mutex);
        }
    }
    // 清理资源
    pthread_join(auto_play_thread, NULL);
    pthread_join(music_thread, NULL);
    pthread_mutex_destroy(&mutex);
    lcd_close();
    return 0;
}
